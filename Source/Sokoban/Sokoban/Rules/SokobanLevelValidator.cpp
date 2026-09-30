#include "SokobanLevelValidator.h"

#define LOCTEXT_NAMESPACE "SokobanLevelValidator"

TArray<FSokobanValidationIssue> FSokobanLevelValidator::Validate(const FSokobanLevelDefinition& Level)
{
	TArray<FSokobanValidationIssue> Issues;
	const auto AddError = [&Issues](const TCHAR* Code, FText Message, TArray<FIntPoint> Cells = {})
	{
		FSokobanValidationIssue& Issue = Issues.AddDefaulted_GetRef();
		Issue.Severity = ESokobanValidationSeverity::Error;
		Issue.Code = FName(Code);
		Issue.Message = MoveTemp(Message);
		Issue.Cells = MoveTemp(Cells);
	};

	// 先用 64 位整数计算总格数，防止损坏数据导致乘法溢出或数组越界。
	bool bValidDimensions = Level.Width > 0 && Level.Height > 0;
	int64 CellCount = 0;
	if (!bValidDimensions)
	{
		AddError(TEXT("InvalidDimensions"), LOCTEXT("InvalidDimensions", "棋盘宽度和高度必须大于 0。"));
	}
	else
	{
		CellCount = static_cast<int64>(Level.Width) * Level.Height;
		if (CellCount > MAX_int32)
		{
			bValidDimensions = false;
			AddError(TEXT("GridSizeOverflow"), LOCTEXT("GridSizeOverflow", "棋盘格子总数超出数组索引范围，请减小宽度或高度。"));
		}
	}

	const bool bCanReadTerrain = bValidDimensions && CellCount == Level.Terrain.Num();
	if (bValidDimensions && !bCanReadTerrain)
	{
		AddError(TEXT("TerrainSizeMismatch"), FText::Format(
			LOCTEXT("TerrainSizeMismatch", "地形数组应包含 {0} 个格子，实际为 {1} 个。"),
			FText::AsNumber(CellCount), FText::AsNumber(Level.Terrain.Num())));
	}

	// 长度正确后才将数组索引转换为格子坐标；未知枚举值也属于非法数据。
	if (bCanReadTerrain)
	{
		for (int32 Index = 0; Index < Level.Terrain.Num(); ++Index)
		{
			const ESokobanTerrain Terrain = Level.Terrain[Index];
			if (Terrain != ESokobanTerrain::Void && Terrain != ESokobanTerrain::Floor && Terrain != ESokobanTerrain::Wall)
			{
				const FIntPoint Cell(Index % Level.Width, Index / Level.Width);
				AddError(TEXT("InvalidTerrain"), FText::Format(
					LOCTEXT("InvalidTerrain", "格子 ({0}, {1}) 包含无法识别的地形类型。"),
					FText::AsNumber(Cell.X), FText::AsNumber(Cell.Y)), { Cell });
			}
		}
	}

	const auto IsInside = [&Level, bValidDimensions](FIntPoint Cell)
	{
		return bValidDimensions && Cell.X >= 0 && Cell.X < Level.Width && Cell.Y >= 0 && Cell.Y < Level.Height;
	};

	const auto CheckPosition = [&](FIntPoint Cell, const FText& Label, const TCHAR* BoundsCode, const TCHAR* FloorCode)
	{
		if (!bValidDimensions)
		{
			return;
		}
		if (!IsInside(Cell))
		{
			AddError(BoundsCode, FText::Format(
				LOCTEXT("PositionOutOfBounds", "{0}的坐标 ({1}, {2}) 超出棋盘范围。"),
				Label, FText::AsNumber(Cell.X), FText::AsNumber(Cell.Y)), { Cell });
			return;
		}
		if (!bCanReadTerrain)
		{
			return;
		}

		const ESokobanTerrain Terrain = Level.Terrain[Cell.Y * Level.Width + Cell.X];
		// 未知地形已在上面报告，避免对同一原因追加误导性的“非地板”错误。
		if (Terrain == ESokobanTerrain::Wall || Terrain == ESokobanTerrain::Void)
		{
			AddError(FloorCode, FText::Format(
				LOCTEXT("PositionNotOnFloor", "{0}必须放在地板上，不能位于墙或空洞。"), Label), { Cell });
		}
	};

	if (Level.PlayerSpawn == FIntPoint(-1, -1))
	{
		AddError(TEXT("MissingPlayerSpawn"), LOCTEXT("MissingPlayerSpawn", "尚未放置玩家出生点。"));
	}
	else
	{
		CheckPosition(Level.PlayerSpawn, LOCTEXT("PlayerLabel", "玩家出生点"), TEXT("PlayerOutOfBounds"), TEXT("PlayerNotOnFloor"));
	}

	if (Level.Boxes.IsEmpty())
	{
		AddError(TEXT("NoBoxes"), LOCTEXT("NoBoxes", "关卡中必须至少有一个箱子。"));
	}
	if (Level.Boxes.Num() != Level.Goals.Num())
	{
		AddError(TEXT("BoxGoalCountMismatch"), FText::Format(
			LOCTEXT("BoxGoalCountMismatch", "箱子数量 ({0}) 与目标点数量 ({1}) 必须一致。"),
			FText::AsNumber(Level.Boxes.Num()), FText::AsNumber(Level.Goals.Num())));
	}

	TMap<int32, FIntPoint> FirstBoxPositionsById;
	TSet<FIntPoint> OccupiedBoxCells;
	for (const FSokobanBoxDefinition& Box : Level.Boxes)
	{
		if (Box.BoxId < 0)
		{
			AddError(TEXT("InvalidBoxId"), LOCTEXT("InvalidBoxId", "箱子 ID 必须为非负整数，请为箱子分配有效 ID。"), { Box.Position });
		}
		else if (const FIntPoint* FirstPosition = FirstBoxPositionsById.Find(Box.BoxId))
		{
			TArray<FIntPoint> Cells = { *FirstPosition };
			Cells.AddUnique(Box.Position);
			AddError(TEXT("DuplicateBoxId"), FText::Format(
				LOCTEXT("DuplicateBoxId", "箱子 ID {0} 重复，请为每个箱子设置唯一 ID。"),
				FText::AsNumber(Box.BoxId)), MoveTemp(Cells));
		}
		else
		{
			FirstBoxPositionsById.Add(Box.BoxId, Box.Position);
		}

		CheckPosition(Box.Position, FText::Format(LOCTEXT("BoxLabel", "箱子 {0}"), FText::AsNumber(Box.BoxId)),
			TEXT("BoxOutOfBounds"), TEXT("BoxNotOnFloor"));

		// 占用冲突只检查棋盘内的格子；越界的占位坐标不视为实际占用。
		if (IsInside(Box.Position))
		{
			if (OccupiedBoxCells.Contains(Box.Position))
			{
				AddError(TEXT("OverlappingBoxes"), LOCTEXT("OverlappingBoxes", "多个箱子占用了同一个格子。"), { Box.Position });
			}
			OccupiedBoxCells.Add(Box.Position);
			if (Box.Position == Level.PlayerSpawn)
			{
				AddError(TEXT("PlayerBoxOverlap"), LOCTEXT("PlayerBoxOverlap", "玩家出生点不能与箱子重叠。"), { Box.Position });
			}
		}
	}

	TSet<FIntPoint> SeenGoals;
	for (const FIntPoint Goal : Level.Goals)
	{
		if (SeenGoals.Contains(Goal))
		{
			AddError(TEXT("DuplicateGoal"), LOCTEXT("DuplicateGoal", "同一个格子被重复配置为目标点。"), { Goal });
		}
		SeenGoals.Add(Goal);
		CheckPosition(Goal, LOCTEXT("GoalLabel", "目标点"), TEXT("GoalOutOfBounds"), TEXT("GoalNotOnFloor"));
	}

	return Issues;
}

#undef LOCTEXT_NAMESPACE
