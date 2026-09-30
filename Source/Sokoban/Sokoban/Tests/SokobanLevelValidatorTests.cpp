#include "Sokoban/Rules/SokobanLevelValidator.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

namespace SokobanValidatorTests
{
	// 使用非正方形棋盘，确保测试也能发现行列索引写反的问题。
	FSokobanLevelDefinition MakeValidLevel()
	{
		FSokobanLevelDefinition Level;
		Level.Width = 4;
		Level.Height = 3;
		Level.Terrain.Init(ESokobanTerrain::Floor, 12);
		Level.PlayerSpawn = FIntPoint(0, 0);
		FSokobanBoxDefinition Box;
		Box.BoxId = 7;
		Box.Position = FIntPoint(1, 1);
		Level.Boxes.Add(Box);
		Level.Goals.Add(FIntPoint(3, 2));
		return Level;
	}

	const FSokobanValidationIssue* FindIssue(const TArray<FSokobanValidationIssue>& Issues, const TCHAR* Code)
	{
		return Issues.FindByPredicate([Code](const FSokobanValidationIssue& Issue)
		{
			return Issue.Code == FName(Code);
		});
	}

	bool HasIssue(const FSokobanLevelDefinition& Level, const TCHAR* Code)
	{
		return FindIssue(FSokobanLevelValidator::Validate(Level), Code) != nullptr;
	}
}

using namespace SokobanValidatorTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanValidatorValidLayoutsTest,
	"Sokoban.Validation.ValidLayouts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanValidatorValidLayoutsTest::RunTest(const FString& Parameters)
{
	FSokobanLevelDefinition Level = MakeValidLevel();
	TestEqual(TEXT("合法布局通过校验"), FSokobanLevelValidator::Validate(Level).Num(), 0);
	Level.Boxes[0].Position = Level.Goals[0];
	TestEqual(TEXT("箱子允许初始位于目标点上"), FSokobanLevelValidator::Validate(Level).Num(), 0);
	Level = MakeValidLevel();
	Level.PlayerSpawn = Level.Goals[0];
	TestEqual(TEXT("玩家允许初始位于目标点上"), FSokobanLevelValidator::Validate(Level).Num(), 0);
	Level = MakeValidLevel();
	Level.Terrain[2] = ESokobanTerrain::Wall;
	Level.Terrain[3] = ESokobanTerrain::Void;
	TestEqual(TEXT("未被占用的墙与空洞合法，不强制封闭边界"), FSokobanLevelValidator::Validate(Level).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanValidatorDimensionsTest,
	"Sokoban.Validation.DimensionsAndArrayBounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanValidatorDimensionsTest::RunTest(const FString& Parameters)
{
	for (const FIntPoint Size : { FIntPoint(0, 3), FIntPoint(4, 0), FIntPoint(-1, 3), FIntPoint(4, -1) })
	{
		FSokobanLevelDefinition Level = MakeValidLevel();
		Level.Width = Size.X;
		Level.Height = Size.Y;
		TestTrue(TEXT("拒绝非正尺寸"), HasIssue(Level, TEXT("InvalidDimensions")));
	}
	FSokobanLevelDefinition Level = MakeValidLevel();
	Level.Width = MAX_int32;
	Level.Height = MAX_int32;
	TestTrue(TEXT("先用 64 位计算，拒绝超过 32 位索引范围的棋盘"), HasIssue(Level, TEXT("GridSizeOverflow")));
	Level.Height = 1;
	TestTrue(TEXT("极大尺寸与短数组不匹配时不会访问越界"), HasIssue(Level, TEXT("TerrainSizeMismatch")));
	for (const int32 Length : { 0, 11, 13 })
	{
		Level = MakeValidLevel();
		Level.Terrain.SetNum(Length);
		TestTrue(TEXT("拒绝过短或过长地形数组"), HasIssue(Level, TEXT("TerrainSizeMismatch")));
	}
	Level = FSokobanLevelDefinition();
	const TArray<FSokobanValidationIssue> Issues = FSokobanLevelValidator::Validate(Level);
	TestTrue(TEXT("空草稿缺少有效尺寸"), FindIssue(Issues, TEXT("InvalidDimensions")) != nullptr);
	TestTrue(TEXT("空草稿缺少出生点"), FindIssue(Issues, TEXT("MissingPlayerSpawn")) != nullptr);
	TestTrue(TEXT("空草稿缺少箱子"), FindIssue(Issues, TEXT("NoBoxes")) != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanValidatorTerrainTest,
	"Sokoban.Validation.InvalidTerrain", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanValidatorTerrainTest::RunTest(const FString& Parameters)
{
	FSokobanLevelDefinition Level = MakeValidLevel();
	Level.Terrain[11] = static_cast<ESokobanTerrain>(255);
	const TArray<FSokobanValidationIssue> Issues = FSokobanLevelValidator::Validate(Level);
	const FSokobanValidationIssue* Issue = FindIssue(Issues, TEXT("InvalidTerrain"));
	TestNotNull(TEXT("报告非法枚举值"), Issue);
	if (Issue)
	{
		TestTrue(TEXT("按行索引转换出正确格子"), Issue->Cells.Contains(FIntPoint(3, 2)));
	}
	TestEqual(TEXT("目标点处非法地形不重复报告非地板错误"), Issues.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanValidatorSpawnTest,
	"Sokoban.Validation.PlayerSpawn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanValidatorSpawnTest::RunTest(const FString& Parameters)
{
	FSokobanLevelDefinition Level = MakeValidLevel();
	Level.PlayerSpawn = FIntPoint(-1, -1);
	TestTrue(TEXT("识别尚未放置的出生点"), HasIssue(Level, TEXT("MissingPlayerSpawn")));
	for (const FIntPoint Position : { FIntPoint(-1, 0), FIntPoint(0, -1), FIntPoint(4, 0), FIntPoint(0, 3), FIntPoint(MAX_int32, MIN_int32) })
	{
		Level.PlayerSpawn = Position;
		TestTrue(TEXT("拒绝越界出生点"), HasIssue(Level, TEXT("PlayerOutOfBounds")));
	}
	for (const ESokobanTerrain Terrain : { ESokobanTerrain::Wall, ESokobanTerrain::Void })
	{
		Level = MakeValidLevel();
		Level.Terrain[0] = Terrain;
		TestTrue(TEXT("出生点必须位于地板"), HasIssue(Level, TEXT("PlayerNotOnFloor")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanValidatorBoxesTest,
	"Sokoban.Validation.BoxIdentityAndOccupancy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanValidatorBoxesTest::RunTest(const FString& Parameters)
{
	FSokobanLevelDefinition Level = MakeValidLevel();
	Level.Boxes.Reset();
	TestTrue(TEXT("至少需要一个箱子"), HasIssue(Level, TEXT("NoBoxes")));
	TestTrue(TEXT("箱子与目标数量必须一致"), HasIssue(Level, TEXT("BoxGoalCountMismatch")));
	for (const int32 BoxId : { static_cast<int32>(INDEX_NONE), -2 })
	{
		Level = MakeValidLevel();
		Level.Boxes[0].BoxId = BoxId;
		TestTrue(TEXT("拒绝负数 ID"), HasIssue(Level, TEXT("InvalidBoxId")));
	}
	Level = MakeValidLevel();
	FSokobanBoxDefinition SecondBox = Level.Boxes[0];
	SecondBox.Position = FIntPoint(2, 1);
	Level.Boxes.Add(SecondBox);
	Level.Goals.Add(FIntPoint(2, 2));
	const TArray<FSokobanValidationIssue> Issues = FSokobanLevelValidator::Validate(Level);
	const FSokobanValidationIssue* DuplicateIssue = FindIssue(Issues, TEXT("DuplicateBoxId"));
	TestNotNull(TEXT("识别重复箱子 ID"), DuplicateIssue);
	if (DuplicateIssue)
	{
		TestEqual(TEXT("标出发生 ID 冲突的两个位置"), DuplicateIssue->Cells.Num(), 2);
		TestTrue(TEXT("包含第一个箱子位置"), DuplicateIssue->Cells.Contains(FIntPoint(1, 1)));
		TestTrue(TEXT("包含第二个箱子位置"), DuplicateIssue->Cells.Contains(FIntPoint(2, 1)));
	}
	Level.Boxes[1].BoxId = 0;
	TestEqual(TEXT("ID 不要求连续，也允许使用 0"), FSokobanLevelValidator::Validate(Level).Num(), 0);
	Level.Boxes[1].Position = Level.Boxes[0].Position;
	TestTrue(TEXT("不同 ID 的箱子也不能占用同一格"), HasIssue(Level, TEXT("OverlappingBoxes")));
	Level = MakeValidLevel();
	Level.Boxes[0].Position = Level.PlayerSpawn;
	TestTrue(TEXT("玩家不能与箱子重叠"), HasIssue(Level, TEXT("PlayerBoxOverlap")));
	Level.Boxes[0].Position = FIntPoint(4, 0);
	TestTrue(TEXT("箱子不能越界"), HasIssue(Level, TEXT("BoxOutOfBounds")));
	for (const ESokobanTerrain Terrain : { ESokobanTerrain::Wall, ESokobanTerrain::Void })
	{
		Level = MakeValidLevel();
		Level.Terrain[5] = Terrain;
		TestTrue(TEXT("箱子必须位于地板"), HasIssue(Level, TEXT("BoxNotOnFloor")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanValidatorGoalsTest,
	"Sokoban.Validation.Goals", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanValidatorGoalsTest::RunTest(const FString& Parameters)
{
	FSokobanLevelDefinition Level = MakeValidLevel();
	Level.Goals.Add(FIntPoint(3, 2));
	TestTrue(TEXT("重复目标点必须报告"), HasIssue(Level, TEXT("DuplicateGoal")));
	Level = MakeValidLevel();
	Level.Goals.Reset();
	TestTrue(TEXT("有箱子但没有目标点时报告数量不符"), HasIssue(Level, TEXT("BoxGoalCountMismatch")));
	for (const FIntPoint Position : { FIntPoint(-1, 0), FIntPoint(4, 2), FIntPoint(0, 3) })
	{
		Level = MakeValidLevel();
		Level.Goals[0] = Position;
		TestTrue(TEXT("目标点不能越界"), HasIssue(Level, TEXT("GoalOutOfBounds")));
	}
	for (const ESokobanTerrain Terrain : { ESokobanTerrain::Wall, ESokobanTerrain::Void })
	{
		Level = MakeValidLevel();
		Level.Terrain[11] = Terrain;
		TestTrue(TEXT("目标点必须位于地板"), HasIssue(Level, TEXT("GoalNotOnFloor")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanValidatorDiagnosticsTest,
	"Sokoban.Validation.DiagnosticsAndInputPreservation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanValidatorDiagnosticsTest::RunTest(const FString& Parameters)
{
	FSokobanLevelDefinition Level = MakeValidLevel();
	Level.Terrain.Reset();
	Level.PlayerSpawn = FIntPoint(-1, -1);
	Level.Boxes[0].BoxId = INDEX_NONE;
	Level.Goals.Add(FIntPoint(3, 2));
	const FSokobanLevelDefinition Before = Level;
	const TArray<FSokobanValidationIssue> Issues = FSokobanLevelValidator::Validate(Level);
	for (const TCHAR* Code : { TEXT("TerrainSizeMismatch"), TEXT("MissingPlayerSpawn"), TEXT("InvalidBoxId"), TEXT("DuplicateGoal"), TEXT("BoxGoalCountMismatch") })
	{
		TestTrue(FString::Printf(TEXT("一次报告独立问题：%s"), Code), FindIssue(Issues, Code) != nullptr);
	}
	TestEqual(TEXT("只报告实际错误，不因缺失地形追加地板错误"), Issues.Num(), 5);
	for (const FSokobanValidationIssue& Issue : Issues)
	{
		TestTrue(TEXT("阻止开局的问题标为错误"), Issue.Severity == ESokobanValidationSeverity::Error);
		TestFalse(TEXT("诊断必须包含给策划看的说明"), Issue.Message.IsEmpty());
		if (Issue.Code == FName(TEXT("TerrainSizeMismatch")))
		{
			TestTrue(TEXT("整体错误不指定格子"), Issue.Cells.IsEmpty());
		}
	}
	TestTrue(TEXT("校验不修改宽高"), Level.Width == Before.Width && Level.Height == Before.Height);
	TestTrue(TEXT("校验不修改地形"), Level.Terrain == Before.Terrain);
	TestTrue(TEXT("校验不修改目标"), Level.Goals == Before.Goals);
	TestTrue(TEXT("校验不修改出生点"), Level.PlayerSpawn == Before.PlayerSpawn);
	TestEqual(TEXT("校验不修改箱子数量"), Level.Boxes.Num(), Before.Boxes.Num());
	TestTrue(TEXT("校验不修改箱子内容"), Level.Boxes[0].BoxId == Before.Boxes[0].BoxId && Level.Boxes[0].Position == Before.Boxes[0].Position);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
