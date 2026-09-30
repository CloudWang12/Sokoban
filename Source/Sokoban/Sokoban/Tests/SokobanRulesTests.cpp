#include "Sokoban/Rules/SokobanRules.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"

namespace SokobanRulesTests
{
	// 非正方形棋盘：玩家右侧有一个箱子，再右侧为目标点。
	FSokobanLevelDefinition MakeLevel()
	{
		FSokobanLevelDefinition Level;
		Level.Width = 6;
		Level.Height = 5;
		Level.Terrain.Init(ESokobanTerrain::Floor, 30);
		Level.PlayerSpawn = FIntPoint(1, 2);
		FSokobanBoxDefinition Box;
		Box.BoxId = 7;
		Box.Position = FIntPoint(2, 2);
		Level.Boxes.Add(Box);
		Level.Goals.Add(FIntPoint(3, 2));
		return Level;
	}

	FSokobanBoardState MakeState(const FSokobanLevelDefinition& Level)
	{
		FSokobanBoardState State;
		State.PlayerPosition = Level.PlayerSpawn;
		for (const FSokobanBoxDefinition& Box : Level.Boxes)
		{
			State.BoxPositions.Add(Box.BoxId, Box.Position);
		}
		return State;
	}

	bool StatesEqual(const FSokobanBoardState& A, const FSokobanBoardState& B)
	{
		if (A.PlayerPosition != B.PlayerPosition || A.BoxPositions.Num() != B.BoxPositions.Num() ||
			A.Counters.MoveCount != B.Counters.MoveCount || A.Counters.PushCount != B.Counters.PushCount)
		{
			return false;
		}
		for (const TPair<int32, FIntPoint>& Box : A.BoxPositions)
		{
			const FIntPoint* Other = B.BoxPositions.Find(Box.Key);
			if (!Other || *Other != Box.Value)
			{
				return false;
			}
		}
		return true;
	}

	void AddSecondBox(FSokobanLevelDefinition& Level)
	{
		FSokobanBoxDefinition Box;
		Box.BoxId = 19;
		Box.Position = FIntPoint(4, 3);
		Level.Boxes.Add(Box);
		Level.Goals.Add(FIntPoint(5, 4));
	}
}

using namespace SokobanRulesTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanRulesDirectionsTest,
	"Sokoban.Rules.DirectionsAndPlanning", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanRulesDirectionsTest::RunTest(const FString& Parameters)
{
	FSokobanLevelDefinition Level = MakeLevel();
	Level.PlayerSpawn = FIntPoint(2, 1);
	Level.Boxes[0].Position = FIntPoint(4, 3);
	FSokobanBoardState State = MakeState(Level);
	State.Counters.MoveCount = 9;
	State.Counters.PushCount = 3;
	const FSokobanBoardState Before = State;
	const ESokobanDirection Directions[] = { ESokobanDirection::Up, ESokobanDirection::Right, ESokobanDirection::Down, ESokobanDirection::Left };
	const FIntPoint Offsets[] = { FIntPoint(0, -1), FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0) };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FSokobanMoveResult Result = FSokobanRules::TryBuildMove(Level, State, Directions[Index]);
		TestTrue(TEXT("四方向空地移动生成普通移动"), Result.Outcome == ESokobanMoveOutcome::Walk);
		TestTrue(TEXT("移动方向遵守行列坐标约定"), Result.Record.PlayerTo == State.PlayerPosition + Offsets[Index]);
		TestTrue(TEXT("记录起点正确"), Result.Record.PlayerFrom == State.PlayerPosition);
		TestEqual(TEXT("普通移动没有被推箱子"), Result.Record.BoxId, static_cast<int32>(INDEX_NONE));
		TestEqual(TEXT("普通移动步数加一"), Result.Record.CountersAfter.MoveCount, 10);
		TestEqual(TEXT("普通移动不增加推动数"), Result.Record.CountersAfter.PushCount, 3);
		TestTrue(TEXT("规划行动不会改变状态"), StatesEqual(State, Before));
		TestTrue(TEXT("普通移动可以执行"), FSokobanRules::ApplyMove(Level, State, Result.Record));
		TestTrue(TEXT("普通移动可以撤销"), FSokobanRules::RevertMove(Level, State, Result.Record));
		TestTrue(TEXT("撤销还原全部数据"), StatesEqual(State, Before));
	}
	for (const ESokobanDirection Direction : { ESokobanDirection::None, static_cast<ESokobanDirection>(255) })
	{
		const FSokobanMoveResult Result = FSokobanRules::TryBuildMove(Level, State, Direction);
		TestTrue(TEXT("无方向及未知枚举值被拒绝"), Result.Outcome == ESokobanMoveOutcome::InvalidDirection);
		TestTrue(TEXT("无效结果不携带行动起点"), Result.Record.PlayerFrom == FIntPoint(-1, -1));
		TestFalse(TEXT("无效记录不能执行"), FSokobanRules::ApplyMove(Level, State, Result.Record));
		TestTrue(TEXT("无效输入不改变状态"), StatesEqual(State, Before));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanRulesPushTest,
	"Sokoban.Rules.PushUndoRedo", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanRulesPushTest::RunTest(const FString& Parameters)
{
	const FSokobanLevelDefinition Level = MakeLevel();
	FSokobanBoardState State = MakeState(Level);
	const FSokobanBoardState Before = State;
	const FSokobanMoveResult Result = FSokobanRules::TryBuildMove(Level, State, ESokobanDirection::Right);
	TestTrue(TEXT("向箱子方向移动生成推动"), Result.Outcome == ESokobanMoveOutcome::Push);
	TestEqual(TEXT("记录稳定的箱子 ID"), Result.Record.BoxId, 7);
	TestTrue(TEXT("记录箱子起点"), Result.Record.BoxFrom == FIntPoint(2, 2));
	TestTrue(TEXT("记录箱子终点"), Result.Record.BoxTo == FIntPoint(3, 2));
	TestTrue(TEXT("生成记录不改变棋盘"), StatesEqual(State, Before));
	TestTrue(TEXT("推动成功"), FSokobanRules::ApplyMove(Level, State, Result.Record));
	TestTrue(TEXT("玩家进入箱子原位置"), State.PlayerPosition == FIntPoint(2, 2));
	TestTrue(TEXT("箱子向前一格"), State.BoxPositions.FindChecked(7) == FIntPoint(3, 2));
	TestEqual(TEXT("推动增加一步"), State.Counters.MoveCount, 1);
	TestEqual(TEXT("推动增加一次推动"), State.Counters.PushCount, 1);
	TestTrue(TEXT("到达目标后通关"), FSokobanRules::IsSolved(Level, State));
	const FSokobanBoardState After = State;
	TestFalse(TEXT("同一条记录不能重复执行"), FSokobanRules::ApplyMove(Level, State, Result.Record));
	TestTrue(TEXT("重复执行失败不改变状态"), StatesEqual(State, After));
	TestTrue(TEXT("推动可以整体撤销"), FSokobanRules::RevertMove(Level, State, Result.Record));
	TestTrue(TEXT("撤销同时还原角色、箱子与计数"), StatesEqual(State, Before));
	TestFalse(TEXT("同一条记录不能重复撤销"), FSokobanRules::RevertMove(Level, State, Result.Record));
	TestTrue(TEXT("重复撤销失败不改变状态"), StatesEqual(State, Before));
	TestTrue(TEXT("撤销后能够重做"), FSokobanRules::ApplyMove(Level, State, Result.Record));
	TestTrue(TEXT("重做恢复原结果"), StatesEqual(State, After));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanRulesNoPullTest,
	"Sokoban.Rules.WalkingDoesNotPull", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanRulesNoPullTest::RunTest(const FString& Parameters)
{
	const FSokobanLevelDefinition Level = MakeLevel();
	FSokobanBoardState State = MakeState(Level);
	const FSokobanMoveResult Push = FSokobanRules::TryBuildMove(Level, State, ESokobanDirection::Right);
	TestTrue(TEXT("先推动箱子"), FSokobanRules::ApplyMove(Level, State, Push.Record));
	const FSokobanBoardState AfterPush = State;
	const FSokobanMoveResult Walk = FSokobanRules::TryBuildMove(Level, State, ESokobanDirection::Left);
	TestTrue(TEXT("远离箱子时为普通移动"), Walk.Outcome == ESokobanMoveOutcome::Walk);
	TestTrue(TEXT("远离箱子移动成功"), FSokobanRules::ApplyMove(Level, State, Walk.Record));
	TestTrue(TEXT("箱子不会跟随玩家被拉回"), State.BoxPositions.FindChecked(7) == FIntPoint(3, 2));
	TestEqual(TEXT("普通移动保留已有推动计数"), State.Counters.PushCount, 1);
	TestTrue(TEXT("普通移动可撤销"), FSokobanRules::RevertMove(Level, State, Walk.Record));
	TestTrue(TEXT("撤销普通移动不误动箱子"), StatesEqual(State, AfterPush));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanRulesBlockingTest,
	"Sokoban.Rules.TerrainBoxesAndEdges", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanRulesBlockingTest::RunTest(const FString& Parameters)
{
	for (const ESokobanTerrain Terrain : { ESokobanTerrain::Wall, ESokobanTerrain::Void })
	{
		FSokobanLevelDefinition Level = MakeLevel();
		Level.Goals[0] = FIntPoint(5, 4);
		Level.Terrain[2 * Level.Width + 3] = Terrain;
		FSokobanBoardState State = MakeState(Level);
		const FSokobanBoardState Before = State;
		const FSokobanMoveResult Push = FSokobanRules::TryBuildMove(Level, State, ESokobanDirection::Right);
		TestTrue(TEXT("箱子后方的墙或空洞阻挡推动"), Push.Outcome == ESokobanMoveOutcome::BlockedByTerrain);
		TestTrue(TEXT("失败不生成箱子行动记录"), Push.Record.BoxId == INDEX_NONE);
		TestTrue(TEXT("阻挡不改变位置和计数"), StatesEqual(State, Before));
		State.PlayerPosition = FIntPoint(3, 1);
		TestTrue(TEXT("玩家不能走入墙或空洞"), FSokobanRules::TryBuildMove(Level, State, ESokobanDirection::Down).Outcome == ESokobanMoveOutcome::BlockedByTerrain);
	}
	FSokobanLevelDefinition Level = MakeLevel();
	AddSecondBox(Level);
	Level.Boxes[1].Position = FIntPoint(3, 2);
	FSokobanBoardState State = MakeState(Level);
	TestTrue(TEXT("禁止一次推动两个箱子"), FSokobanRules::TryBuildMove(Level, State, ESokobanDirection::Right).Outcome == ESokobanMoveOutcome::BlockedByBox);
	Level = MakeLevel();
	const ESokobanDirection Directions[] = { ESokobanDirection::Left, ESokobanDirection::Right, ESokobanDirection::Up, ESokobanDirection::Down };
	const FIntPoint Edges[] = { FIntPoint(0, 2), FIntPoint(5, 2), FIntPoint(2, 0), FIntPoint(2, 4) };
	const FIntPoint Offsets[] = { FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1) };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		State = MakeState(Level);
		State.PlayerPosition = Edges[Index];
		TestTrue(TEXT("四侧均不能走出棋盘"), FSokobanRules::TryBuildMove(Level, State, Directions[Index]).Outcome == ESokobanMoveOutcome::OutOfBounds);
		State.PlayerPosition = Edges[Index] - Offsets[Index];
		State.BoxPositions[7] = Edges[Index];
		TestTrue(TEXT("四侧均不能将箱子推出棋盘"), FSokobanRules::TryBuildMove(Level, State, Directions[Index]).Outcome == ESokobanMoveOutcome::OutOfBounds);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanRulesInvalidStateTest,
	"Sokoban.Rules.InvalidLayoutsAndStates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanRulesInvalidStateTest::RunTest(const FString& Parameters)
{
	const auto CheckInvalid = [this](const FSokobanLevelDefinition& Level, FSokobanBoardState State)
	{
		const FSokobanBoardState Before = State;
		TestTrue(TEXT("非法关卡或状态被拒绝"), FSokobanRules::TryBuildMove(Level, State, ESokobanDirection::Right).Outcome == ESokobanMoveOutcome::InvalidState);
		TestFalse(TEXT("非法状态不会判为通关"), FSokobanRules::IsSolved(Level, State));
		TestFalse(TEXT("非法状态不能执行空记录"), FSokobanRules::ApplyMove(Level, State, FSokobanMoveRecord()));
		TestFalse(TEXT("非法状态不能撤销空记录"), FSokobanRules::RevertMove(Level, State, FSokobanMoveRecord()));
		TestTrue(TEXT("拒绝后状态完全不变"), StatesEqual(State, Before));
	};
	FSokobanLevelDefinition Level = MakeLevel();
	for (int32 Case = 0; Case < 10; ++Case)
	{
		FSokobanBoardState State = MakeState(Level);
		switch (Case)
		{
		case 0: State.PlayerPosition = FIntPoint(MIN_int32, MAX_int32); break;
		case 1: State.BoxPositions[7] = FIntPoint(MAX_int32, MIN_int32); break;
		case 2: State.PlayerPosition = State.BoxPositions[7]; break;
		case 3: State.BoxPositions.Reset(); break;
		case 4: State.BoxPositions.Add(99, FIntPoint(0, 0)); break;
		case 5: State.BoxPositions.Remove(7); State.BoxPositions.Add(99, FIntPoint(2, 2)); break;
		case 6: State.Counters.MoveCount = -1; break;
		case 7: State.Counters.PushCount = -1; break;
		case 8: State.Counters.PushCount = 1; break;
		case 9: State.BoxPositions.Remove(7); State.BoxPositions.Add(INDEX_NONE, FIntPoint(2, 2)); break;
		}
		CheckInvalid(Level, State);
	}
	AddSecondBox(Level);
	FSokobanBoardState State = MakeState(Level);
	State.BoxPositions[19] = State.BoxPositions[7];
	CheckInvalid(Level, State);
	Level = MakeLevel();
	State = MakeState(Level);
	Level.Terrain[0] = ESokobanTerrain::Wall;
	State.BoxPositions[7] = FIntPoint(0, 0);
	CheckInvalid(Level, State);
	State = MakeState(Level);
	State.PlayerPosition = FIntPoint(0, 0);
	CheckInvalid(Level, State);
	Level = MakeLevel();
	State = MakeState(Level);
	Level.Terrain.Reset();
	CheckInvalid(Level, State);
	Level.Width = MAX_int32;
	Level.Height = MAX_int32;
	CheckInvalid(Level, State);
	Level = MakeLevel();
	Level.Terrain[0] = static_cast<ESokobanTerrain>(255);
	CheckInvalid(Level, MakeState(Level));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanRulesTamperedRecordsTest,
	"Sokoban.Rules.RejectTamperedRecordsAtomically", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanRulesTamperedRecordsTest::RunTest(const FString& Parameters)
{
	const FSokobanLevelDefinition Level = MakeLevel();
	const FSokobanBoardState Before = MakeState(Level);
	const FSokobanMoveRecord Original = FSokobanRules::TryBuildMove(Level, Before, ESokobanDirection::Right).Record;
	FSokobanBoardState After = Before;
	TestTrue(TEXT("建立合法操作后状态"), FSokobanRules::ApplyMove(Level, After, Original));
	for (int32 Case = 0; Case < 11; ++Case)
	{
		FSokobanMoveRecord Record = Original;
		switch (Case)
		{
		case 0: Record.PlayerFrom = FIntPoint(MIN_int32, MAX_int32); break;
		case 1: Record.PlayerTo.Y += 1; break;
		case 2: Record.PlayerTo.X += 2; break;
		case 3: Record.BoxId = 999; break;
		case 4: Record.BoxId = -2; break;
		case 5: Record.BoxId = INDEX_NONE; break;
		case 6: Record.BoxFrom = FIntPoint(0, 0); break;
		case 7: Record.BoxTo = FIntPoint(5, 4); break;
		case 8: Record.CountersBefore.MoveCount = -1; break;
		case 9: Record.CountersAfter.MoveCount += 1; break;
		case 10: Record.CountersAfter.PushCount = 0; break;
		}
		FSokobanBoardState State = Before;
		TestFalse(FString::Printf(TEXT("拒绝执行篡改记录 %d"), Case), FSokobanRules::ApplyMove(Level, State, Record));
		TestTrue(TEXT("执行失败无部分更新"), StatesEqual(State, Before));
		State = After;
		TestFalse(FString::Printf(TEXT("拒绝撤销篡改记录 %d"), Case), FSokobanRules::RevertMove(Level, State, Record));
		TestTrue(TEXT("撤销失败无部分更新"), StatesEqual(State, After));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanRulesStaleRecordsTest,
	"Sokoban.Rules.RejectMismatchedHistory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanRulesStaleRecordsTest::RunTest(const FString& Parameters)
{
	FSokobanLevelDefinition Level = MakeLevel();
	AddSecondBox(Level);
	FSokobanBoardState State = MakeState(Level);
	const FSokobanMoveRecord Record = FSokobanRules::TryBuildMove(Level, State, ESokobanDirection::Right).Record;
	State.Counters.MoveCount = 2;
	FSokobanBoardState Snapshot = State;
	TestFalse(TEXT("位置相同但计数不同也不能使用旧记录"), FSokobanRules::ApplyMove(Level, State, Record));
	TestTrue(TEXT("计数不匹配时保持原状"), StatesEqual(State, Snapshot));
	State = MakeState(Level);
	TestTrue(TEXT("准备正常推动"), FSokobanRules::ApplyMove(Level, State, Record));
	State.BoxPositions[19] = Record.PlayerFrom;
	Snapshot = State;
	TestFalse(TEXT("撤销会与另一箱子重叠时必须拒绝"), FSokobanRules::RevertMove(Level, State, Record));
	TestTrue(TEXT("副本验证失败不污染当前棋盘"), StatesEqual(State, Snapshot));
	Level = MakeLevel();
	Level.Goals[0] = FIntPoint(5, 4);
	State = MakeState(Level);
	const FSokobanMoveRecord BeforeEdit = FSokobanRules::TryBuildMove(Level, State, ESokobanDirection::Right).Record;
	Level.Terrain[2 * Level.Width + 3] = ESokobanTerrain::Wall;
	Snapshot = State;
	TestFalse(TEXT("生成记录后地形改变会重新判断合法性"), FSokobanRules::ApplyMove(Level, State, BeforeEdit));
	TestTrue(TEXT("地形阻挡时不移动角色或箱子"), StatesEqual(State, Snapshot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanRulesCounterLimitTest,
	"Sokoban.Rules.CounterOverflowProtection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanRulesCounterLimitTest::RunTest(const FString& Parameters)
{
	const FSokobanLevelDefinition Level = MakeLevel();
	FSokobanBoardState State = MakeState(Level);
	State.Counters.MoveCount = MAX_int32 - 1;
	State.Counters.PushCount = MAX_int32 - 1;
	const FSokobanBoardState Before = State;
	const FSokobanMoveResult Result = FSokobanRules::TryBuildMove(Level, State, ESokobanDirection::Right);
	TestTrue(TEXT("计数达到上限之前仍可行动"), Result.Outcome == ESokobanMoveOutcome::Push);
	TestTrue(TEXT("可以精确递增到最大值"), FSokobanRules::ApplyMove(Level, State, Result.Record));
	TestEqual(TEXT("步数没有溢出"), State.Counters.MoveCount, MAX_int32);
	TestEqual(TEXT("推动数没有溢出"), State.Counters.PushCount, MAX_int32);
	TestTrue(TEXT("达到上限不影响合法通关判断"), FSokobanRules::IsSolved(Level, State));
	TestTrue(TEXT("下一步拒绝溢出"), FSokobanRules::TryBuildMove(Level, State, ESokobanDirection::Down).Outcome == ESokobanMoveOutcome::InvalidState);
	TestTrue(TEXT("达到上限仍然可以撤销"), FSokobanRules::RevertMove(Level, State, Result.Record));
	TestTrue(TEXT("撤销还原原计数"), StatesEqual(State, Before));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanRulesGoalsTest,
	"Sokoban.Rules.GoalsAndCompletion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanRulesGoalsTest::RunTest(const FString& Parameters)
{
	FSokobanLevelDefinition Level = MakeLevel();
	FSokobanBoardState State = MakeState(Level);
	TestFalse(TEXT("箱子未到位时未通关"), FSokobanRules::IsSolved(Level, State));
	const FSokobanMoveRecord OntoGoal = FSokobanRules::TryBuildMove(Level, State, ESokobanDirection::Right).Record;
	TestTrue(TEXT("箱子可以进入目标点"), FSokobanRules::ApplyMove(Level, State, OntoGoal));
	TestTrue(TEXT("全部到位后通关"), FSokobanRules::IsSolved(Level, State));
	const FSokobanMoveRecord OffGoal = FSokobanRules::TryBuildMove(Level, State, ESokobanDirection::Right).Record;
	TestTrue(TEXT("规则本身允许离开目标点，结算锁定由单局流程控制"), FSokobanRules::ApplyMove(Level, State, OffGoal));
	TestFalse(TEXT("离开目标点后不再满足通关条件"), FSokobanRules::IsSolved(Level, State));
	TestTrue(TEXT("箱子离开后目标点仍存在"), Level.Goals[0] == FIntPoint(3, 2));
	TestTrue(TEXT("撤销恢复到位状态"), FSokobanRules::RevertMove(Level, State, OffGoal));
	TestTrue(TEXT("撤销后重新满足通关条件"), FSokobanRules::IsSolved(Level, State));
	AddSecondBox(Level);
	State = MakeState(Level);
	State.BoxPositions[7] = Level.Goals[0];
	TestFalse(TEXT("多箱子只到位一个不能通关"), FSokobanRules::IsSolved(Level, State));
	State.BoxPositions[19] = Level.Goals[1];
	TestTrue(TEXT("所有箱子各占一个目标点才通关"), FSokobanRules::IsSolved(Level, State));
	State.BoxPositions.Remove(19);
	TestFalse(TEXT("缺失箱子不能被误判为通关"), FSokobanRules::IsSolved(Level, State));
	TestFalse(TEXT("空草稿不能被误判为通关"), FSokobanRules::IsSolved(FSokobanLevelDefinition(), FSokobanBoardState()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanRulesSequenceTest,
	"Sokoban.Rules.SequenceRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanRulesSequenceTest::RunTest(const FString& Parameters)
{
	FSokobanLevelDefinition Level = MakeLevel();
	AddSecondBox(Level);
	FSokobanBoardState State = MakeState(Level);
	const FSokobanBoardState Initial = State;
	TArray<FSokobanMoveRecord> History;
	TArray<FSokobanBoardState> Snapshots;
	FRandomStream Random(20260929);
	const ESokobanDirection Directions[] = { ESokobanDirection::Up, ESokobanDirection::Right, ESokobanDirection::Down, ESokobanDirection::Left };
	for (int32 Step = 0; Step < 256; ++Step)
	{
		const FSokobanBoardState Before = State;
		const ESokobanDirection Direction = Step == 0 ? ESokobanDirection::Right : Directions[Random.RandRange(0, 3)];
		const FSokobanMoveResult Result = FSokobanRules::TryBuildMove(Level, State, Direction);
		TestTrue(TEXT("长序列中的规划也不能修改状态"), StatesEqual(State, Before));
		if (Result.Outcome != ESokobanMoveOutcome::Walk && Result.Outcome != ESokobanMoveOutcome::Push)
		{
			TestTrue(TEXT("合法状态只能被边界、地形或箱子阻挡"),
				Result.Outcome == ESokobanMoveOutcome::OutOfBounds || Result.Outcome == ESokobanMoveOutcome::BlockedByTerrain || Result.Outcome == ESokobanMoveOutcome::BlockedByBox);
			continue;
		}
		Snapshots.Add(Before);
		History.Add(Result.Record);
		if (!TestTrue(TEXT("合法记录可以执行"), FSokobanRules::ApplyMove(Level, State, Result.Record))) { return false; }
	}
	const FSokobanBoardState Final = State;
	TestTrue(TEXT("序列实际执行了多步操作"), History.Num() > 1);
	TestTrue(TEXT("序列包含推动"), Final.Counters.PushCount > 0);
	TestEqual(TEXT("每次成功操作增加一步"), Final.Counters.MoveCount, History.Num());
	for (int32 Index = History.Num() - 1; Index >= 0; --Index)
	{
		if (!TestTrue(TEXT("逐步逆序撤销成功"), FSokobanRules::RevertMove(Level, State, History[Index]))) { return false; }
		TestTrue(TEXT("每步撤销精确恢复对应快照"), StatesEqual(State, Snapshots[Index]));
	}
	TestTrue(TEXT("全部撤销后等于初始状态"), StatesEqual(State, Initial));
	for (const FSokobanMoveRecord& Record : History)
	{
		if (!TestTrue(TEXT("全部记录能够按顺序重做"), FSokobanRules::ApplyMove(Level, State, Record))) { return false; }
	}
	TestTrue(TEXT("全部重做后等于第一次最终状态"), StatesEqual(State, Final));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
