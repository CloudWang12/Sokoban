#include "SokobanRules.h"

#include "SokobanLevelValidator.h"

/*
 * 本文件使用的数据结构均在 SokobanTypes.h 中定义，通过 SokobanRules.h 引入：
 * - FSokobanLevelDefinition：关卡的初始设计图，提供尺寸、地形、目标点和初始箱子列表。
 * - FSokobanBoardState：当前对局的可变数据，提供玩家位置、箱子位置和计数。
 * - FSokobanBoxDefinition：单个箱子的初始配置；这里用它的 ID 核对当前状态中有没有少箱子或多箱子。
 * - FSokobanMoveCounters：移动次数与推动次数；每次行动记录操作前、操作后各一份。
 * - FSokobanMoveRecord：一步行动的前后对照，记录玩家与可选箱子的位移，供执行和撤销使用。
 * - FSokobanMoveResult：行动判断的答复，包含结果类型，以及成功时的行动记录。
 * - FSokobanValidationIssue：关卡校验器返回的问题，规则层遇到 Error 就拒绝继续。
 * UE 自带的 FIntPoint 表示整数格子坐标；这里不使用 Actor 的世界坐标或碰撞结果。
 * 下方的 private 命名空间用于归类实现细节，供四个对外接口共用，不是游戏状态的存储位置。
 */
namespace SokobanRulesPrivate
{
	// 将方向枚举换算成一格的位移。OutOffset 是输出参数；返回 false 时调用者不能继续使用它。
	bool TryGetOffset(ESokobanDirection Direction, FIntPoint& OutOffset)
	{
		switch (Direction)
		{
		case ESokobanDirection::Up: OutOffset = FIntPoint(0, -1); return true;
		case ESokobanDirection::Right: OutOffset = FIntPoint(1, 0); return true;
		case ESokobanDirection::Down: OutOffset = FIntPoint(0, 1); return true;
		case ESokobanDirection::Left: OutOffset = FIntPoint(-1, 0); return true;
		default: return false;
		}
	}

	// 从记录中的玩家起终点还原方向，同时拒绝原地不动、斜走和一次跨越多格的记录。
	ESokobanDirection GetRecordDirection(const FSokobanMoveRecord& Record)
	{
		// 记录可能来自损坏存档；先转为 64 位再相减，避免极端坐标溢出。
		const int64 DX = static_cast<int64>(Record.PlayerTo.X) - Record.PlayerFrom.X;
		const int64 DY = static_cast<int64>(Record.PlayerTo.Y) - Record.PlayerFrom.Y;
		if (DX == 0 && DY == -1) { return ESokobanDirection::Up; }
		if (DX == 1 && DY == 0) { return ESokobanDirection::Right; }
		if (DX == 0 && DY == 1) { return ESokobanDirection::Down; }
		if (DX == -1 && DY == 0) { return ESokobanDirection::Left; }
		return ESokobanDirection::None;
	}

	// 以下格子查询仅在关卡结构校验通过后使用。此处只看边界，不检查地形或占用。
	bool IsInside(const FSokobanLevelDefinition& Level, FIntPoint Cell)
	{
		return Cell.X >= 0 && Cell.X < Level.Width && Cell.Y >= 0 && Cell.Y < Level.Height;
	}

	// 先检查边界，利用 && 的短路求值避免访问越界坐标；地形数组按行展开。
	// “是地板”不代表“可以走进去”，该格是否有箱子由后续占用检查决定。
	bool IsFloor(const FSokobanLevelDefinition& Level, FIntPoint Cell)
	{
		return IsInside(Level, Cell) && Level.Terrain[Cell.Y * Level.Width + Cell.X] == ESokobanTerrain::Floor;
	}

	// 比较两份计数快照，用于防止把不匹配的操作前／后记录应用到当前进度。
	bool CountersEqual(const FSokobanMoveCounters& A, const FSokobanMoveCounters& B)
	{
		return A.MoveCount == B.MoveCount && A.PushCount == B.PushCount;
	}

	bool IsValidBoard(const FSokobanLevelDefinition& Level, const FSokobanBoardState& State)
	{
		// 检查 1：先验证初始设计图，确保宽高、数组、目标点和初始箱子等数据可用。
		// Issue 是诊断信息；这里只区分能否继续，详细说明由编辑器或单局管理层展示。
		for (const FSokobanValidationIssue& Issue : FSokobanLevelValidator::Validate(Level))
		{
			if (Issue.Severity == ESokobanValidationSeverity::Error)
			{
				return false;
			}
		}

		// 检查 2：再验证当前状态。一次推动也是一步，所以推动次数不能超过移动次数。
		// 当前箱子数量应与设计图一致，玩家必须站在有效的地板上。
		if (State.Counters.MoveCount < 0 || State.Counters.PushCount < 0 ||
			State.Counters.PushCount > State.Counters.MoveCount ||
			State.BoxPositions.Num() != Level.Boxes.Num() || !IsFloor(Level, State.PlayerPosition))
		{
			return false;
		}

		// 检查 3：用临时集合记录已占用格子，先放玩家，再逐个检查箱子。
		// 这样一次遍历就能同时发现“箱子与玩家重叠”和“两个箱子重叠”。
		TSet<FIntPoint> OccupiedCells;
		OccupiedCells.Add(State.PlayerPosition);
		for (const FSokobanBoxDefinition& Box : Level.Boxes)
		{
			// Box 来自初始配置，但这里只取稳定 ID；位置必须查询 State 中的当前位置。
			// Find 返回指针，找不到对应 ID 时为 nullptr；不能直接解引用。
			const FIntPoint* Position = State.BoxPositions.Find(Box.BoxId);
			if (!Position || !IsFloor(Level, *Position) || OccupiedCells.Contains(*Position))
			{
				return false;
			}
			OccupiedCells.Add(*Position);
		}
		// 数量相同且每个预期 ID 都存在，就能保证没有缺失或混入其他 ID 的箱子。
		return true;
	}

	// 反向查询：已知格子，查找占用它的箱子 ID；INDEX_NONE 表示该格没有箱子。
	// State.BoxPositions 是 TMap<int32, FIntPoint>，遍历得到的 TPair 中 Key 为 ID，Value 为位置。
	int32 FindBoxAt(const FSokobanBoardState& State, FIntPoint Cell)
	{
		for (const TPair<int32, FIntPoint>& Box : State.BoxPositions)
		{
			if (Box.Value == Cell)
			{
				return Box.Key;
			}
		}
		return INDEX_NONE;
	}

	// 只有普通移动和推动属于成功；其余结果都不能执行 Result.Record。
	bool IsSuccessful(const FSokobanMoveResult& Result)
	{
		return Result.Outcome == ESokobanMoveOutcome::Walk || Result.Outcome == ESokobanMoveOutcome::Push;
	}

	// 将重新计算的合法记录与外部传入的记录对照，检查位置、箱子身份和计数。
	// 这里核对的是本次行动涉及的数据，不负责证明记录来自哪个单局；历史归属由 Session 管理。
	bool RecordsMatch(const FSokobanMoveRecord& A, const FSokobanMoveRecord& B)
	{
		return A.PlayerFrom == B.PlayerFrom && A.PlayerTo == B.PlayerTo && A.BoxId == B.BoxId &&
			CountersEqual(A.CountersBefore, B.CountersBefore) && CountersEqual(A.CountersAfter, B.CountersAfter) &&
			// 普通移动不使用箱子坐标字段，保持与数据结构的约定一致。
			(A.BoxId == INDEX_NONE || (A.BoxFrom == B.BoxFrom && A.BoxTo == B.BoxTo));
	}
}

/*
 * 规划一步行动：Level 是关卡设计图，State 是当前棋盘，两者都是 const 引用，不会被修改。
 * Direction 是上层传入的格子方向；本函数无需知道它来自键盘、手柄还是解法回放。
 * 返回值 Result.Outcome 说明成功或阻挡原因，Result.Record 仅在成功时有实际用途。
 */
FSokobanMoveResult FSokobanRules::TryBuildMove(
	const FSokobanLevelDefinition& Level, const FSokobanBoardState& State, ESokobanDirection Direction)
{
	using namespace SokobanRulesPrivate;
	FSokobanMoveResult Result;
	FIntPoint Offset = FIntPoint::ZeroValue;
	// 检查 1：只接受四个方向，排除 None 和损坏数据中的未知枚举值。
	if (!TryGetOffset(Direction, Offset))
	{
		Result.Outcome = ESokobanMoveOutcome::InvalidDirection;
		return Result;
	}
	// 检查 2：确认整个棋盘状态合法，并保证本次 MoveCount + 1 不会溢出。
	if (!IsValidBoard(Level, State) || State.Counters.MoveCount == MAX_int32)
	{
		Result.Outcome = ESokobanMoveOutcome::InvalidState;
		return Result;
	}

	// 检查 3：玩家准备进入的格子必须在棋盘内，并且是地板。
	// PlayerTo 目前只是候选位置，这里尚未移动玩家。
	const FIntPoint PlayerTo = State.PlayerPosition + Offset;
	if (!IsInside(Level, PlayerTo))
	{
		Result.Outcome = ESokobanMoveOutcome::OutOfBounds;
		return Result;
	}
	if (!IsFloor(Level, PlayerTo))
	{
		Result.Outcome = ESokobanMoveOutcome::BlockedByTerrain;
		return Result;
	}

	// 检查 4：前方没有箱子就是普通移动；有箱子则需要继续检查箱子后面的一格。
	const int32 BoxId = FindBoxAt(State, PlayerTo);
	FIntPoint BoxTo(-1, -1);
	if (BoxId != INDEX_NONE)
	{
		// 推动时：玩家进入箱子原来的格子，箱子沿相同方向前进一格。
		BoxTo = PlayerTo + Offset;
		if (!IsInside(Level, BoxTo))
		{
			Result.Outcome = ESokobanMoveOutcome::OutOfBounds;
			return Result;
		}
		if (!IsFloor(Level, BoxTo))
		{
			Result.Outcome = ESokobanMoveOutcome::BlockedByTerrain;
			return Result;
		}
		// 后方还有箱子时拒绝推动；经典规则不允许一次推一排箱子。
		if (FindBoxAt(State, BoxTo) != INDEX_NONE)
		{
			Result.Outcome = ESokobanMoveOutcome::BlockedByBox;
			return Result;
		}
	}

	// 检查全部通过：只填入“准备如何变化”的记录，仍不修改 State。
	// FSokobanMoveRecord 同时保存位置与计数的前后值，后续执行、撤销可以使用同一条记录。
	Result.Outcome = BoxId == INDEX_NONE ? ESokobanMoveOutcome::Walk : ESokobanMoveOutcome::Push;
	Result.Record.PlayerFrom = State.PlayerPosition;
	Result.Record.PlayerTo = PlayerTo;
	Result.Record.CountersBefore = State.Counters;
	Result.Record.CountersAfter = State.Counters;
	++Result.Record.CountersAfter.MoveCount;
	if (BoxId != INDEX_NONE)
	{
		// 只有推动才填写箱子字段；普通移动的 BoxId 保持默认 INDEX_NONE。
		Result.Record.BoxId = BoxId;
		Result.Record.BoxFrom = PlayerTo;
		Result.Record.BoxTo = BoxTo;
		// 有效状态保证 PushCount <= MoveCount；上面已排除 MoveCount 达到上限。
		++Result.Record.CountersAfter.PushCount;
	}
	return Result;
}

/*
 * 执行一步行动：State 为可修改引用，Record 是调用者提交的计划。
 * 即使记录曾经合法，当前棋盘也可能已经变化，因此执行前必须重新核对。
 */
bool FSokobanRules::ApplyMove(const FSokobanLevelDefinition& Level, FSokobanBoardState& State, const FSokobanMoveRecord& Record)
{
	using namespace SokobanRulesPrivate;
	// 检查节点：从记录提取方向，再基于“现在的棋盘”生成 Expected。
	// 只有现在仍能执行，而且前后数据完全符合本次行动时，才允许提交。
	const FSokobanMoveResult Expected = TryBuildMove(Level, State, GetRecordDirection(Record));
	if (!IsSuccessful(Expected) || !RecordsMatch(Expected.Record, Record))
	{
		return false;
	}

	// 使用重新生成并核对过的记录。此后没有可能返回失败的检查，避免部分更新。
	State.PlayerPosition = Expected.Record.PlayerTo;
	State.Counters = Expected.Record.CountersAfter;
	if (Expected.Record.BoxId != INDEX_NONE)
	{
		// FindChecked 不会创建新箱子；前面的状态与记录校验已保证此 ID 存在。
		State.BoxPositions.FindChecked(Expected.Record.BoxId) = Expected.Record.BoxTo;
	}
	return true;
}

/*
 * 撤销一步行动：要求当前状态对应记录的操作后状态，然后恢复操作前状态。
 * 这属于历史还原，并不是给玩家增加“向后拉箱子”的正常移动能力。
 */
bool FSokobanRules::RevertMove(const FSokobanLevelDefinition& Level, FSokobanBoardState& State, const FSokobanMoveRecord& Record)
{
	using namespace SokobanRulesPrivate;
	// 检查 1：当前棋盘本身合法，玩家位置和计数与记录的操作后数据一致。
	if (!IsValidBoard(Level, State) || State.PlayerPosition != Record.PlayerTo ||
		!CountersEqual(State.Counters, Record.CountersAfter))
	{
		return false;
	}
	if (Record.BoxId != INDEX_NONE)
	{
		// 检查 2：如果撤销的是推动，对应箱子必须存在，并且仍在记录的终点。
		const FIntPoint* BoxPosition = State.BoxPositions.Find(Record.BoxId);
		if (!BoxPosition || *BoxPosition != Record.BoxTo)
		{
			return false;
		}
	}

	// 建立独立副本，只在副本上试着恢复；后续检查失败不会留下还原了一半的棋盘。
	FSokobanBoardState Before = State;
	Before.PlayerPosition = Record.PlayerFrom;
	Before.Counters = Record.CountersBefore;
	if (Record.BoxId != INDEX_NONE)
	{
		Before.BoxPositions.FindChecked(Record.BoxId) = Record.BoxFrom;
	}

	// 检查 3：恢复出的状态必须合法，而且从那里向前走一步必须产生相同记录。
	// 复用 TryBuildMove 可检查还原后的重叠、地形、计数等，不必另写一套逆向推动规则。
	const FSokobanMoveResult Expected = TryBuildMove(Level, Before, GetRecordDirection(Record));
	if (!IsSuccessful(Expected) || !RecordsMatch(Expected.Record, Record))
	{
		return false;
	}
	// 所有检查通过后一次性提交副本。MoveTemp 用于转移数据，减少容器复制。
	State = MoveTemp(Before);
	return true;
}

// 这里只判断通关条件，不弹结算界面，也不锁定输入；这些行为由后续单局流程负责。
bool FSokobanRules::IsSolved(const FSokobanLevelDefinition& Level, const FSokobanBoardState& State)
{
	// 检查 1：拒绝缺失箱子、非法位置和空关卡，避免“没有箱子可检查”被误判为通关。
	if (!SokobanRulesPrivate::IsValidBoard(Level, State))
	{
		return false;
	}
	// 将目标点坐标装进集合，方便按坐标查询；读取的是 Level 中不随游玩改变的目标点。
	TSet<FIntPoint> Goals;
	for (const FIntPoint Goal : Level.Goals)
	{
		Goals.Add(Goal);
	}
	// 检查 2：使用箱子的当前位置逐个查目标点，有任何一个未到位就尚未通关。
	for (const TPair<int32, FIntPoint>& Box : State.BoxPositions)
	{
		if (!Goals.Contains(Box.Value))
		{
			return false;
		}
	}
	// 前面的校验已保证箱子与目标数量相等且没有重叠，因此全部箱子到位即覆盖全部目标。
	return true;
}
