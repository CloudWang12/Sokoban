#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SokobanEditorDocument.h"
#include "SSokobanLevelEditor.h"
#include "Sokoban/Data/SokobanLevelData.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "Sokoban/Gameplay/SokobanGameMode.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanEditorPlacementTest, "Sokoban.Editor.CellPlacement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSokobanEditorPlacementTest::RunTest(const FString& Parameters)
{
	FSokobanEditorDocument Doc;
	Doc.NewLevel(4, 4);
	TestTrue(TEXT("可放置玩家"), Doc.ApplyCell({0, 0}, ESokobanTerrain::Floor, false, ESokobanCellOccupant::Player));
	Doc.ApplyCell({1, 0}, ESokobanTerrain::Floor, true, ESokobanCellOccupant::Player);
	TestTrue(TEXT("移动出生点，旧格不再有玩家"), Doc.GetOccupant({0, 0}) == ESokobanCellOccupant::None);
	TestTrue(TEXT("出生点可以在目标上"), Doc.GetData().Definition.Goals.Contains(FIntPoint(1, 0)));
	Doc.ApplyCell({2, 0}, ESokobanTerrain::Floor, false, ESokobanCellOccupant::Box);
	const int32 FirstId = Doc.GetData().Definition.Boxes[0].BoxId;
	Doc.ApplyCell({2, 0}, ESokobanTerrain::Floor, true, ESokobanCellOccupant::Box);
	Doc.ApplyCell({3, 0}, ESokobanTerrain::Floor, false, ESokobanCellOccupant::Box);
	TestEqual(TEXT("调整目标不改变箱子 ID"), Doc.GetData().Definition.Boxes[0].BoxId, FirstId);
	TestTrue(TEXT("新箱子自动分配不同 ID"), Doc.GetData().Definition.Boxes[0].BoxId != Doc.GetData().Definition.Boxes[1].BoxId);
	const auto BeforeWall = Doc.GetData();
	Doc.ApplyCell({2, 0}, ESokobanTerrain::Wall, true, ESokobanCellOccupant::Box);
	TestFalse(TEXT("墙不能保留目标"), Doc.GetData().Definition.Goals.Contains(FIntPoint(2, 0)));
	TestTrue(TEXT("墙不能保留箱子"), Doc.GetOccupant({2, 0}) == ESokobanCellOccupant::None);
	Doc.Undo();
	TestTrue(TEXT("撤销完整恢复箱子和目标"), Doc.GetData().Equals(BeforeWall));
	Doc.ApplyCell({1, 0}, ESokobanTerrain::Void, true, ESokobanCellOccupant::Player);
	TestTrue(TEXT("空洞清除出生点"), Doc.GetData().Definition.PlayerSpawn == FIntPoint(-1, -1));
	TestFalse(TEXT("越界修改被拒绝"), Doc.ApplyCell({100, 0}, ESokobanTerrain::Floor, false, ESokobanCellOccupant::Box));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanEditorResizeTest, "Sokoban.Editor.ResizeAndUndo", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSokobanEditorResizeTest::RunTest(const FString& Parameters)
{
	FSokobanEditorDocument Doc;
	Doc.NewLevel(4, 3);
	Doc.ApplyCell({0, 0}, ESokobanTerrain::Wall, false, ESokobanCellOccupant::None);
	Doc.ApplyCell({3, 2}, ESokobanTerrain::Floor, true, ESokobanCellOccupant::Box);
	Doc.ApplyCell({3, 1}, ESokobanTerrain::Floor, false, ESokobanCellOccupant::Player);
	const auto Before = Doc.GetData();
	Doc.Resize(2, 2);
	TestEqual(TEXT("尺寸和数组同步"), Doc.GetData().Definition.Terrain.Num(), 4);
	TestTrue(TEXT("保留左上角地形"), Doc.GetData().Definition.Terrain[0] == ESokobanTerrain::Wall);
	TestTrue(TEXT("裁剪越界占用"), Doc.GetData().Definition.Boxes.IsEmpty() && Doc.GetData().Definition.Goals.IsEmpty() && Doc.GetData().Definition.PlayerSpawn == FIntPoint(-1, -1));
	Doc.Undo();
	TestTrue(TEXT("撤销尺寸调整恢复全部内容"), Doc.GetData().Equals(Before));
	Doc.Resize(5, 5);
	TestTrue(TEXT("扩展区域默认地板"), Doc.GetData().Definition.Terrain[24] == ESokobanTerrain::Floor);
	TestEqual(TEXT("扩大不丢失箱子"), Doc.GetData().Definition.Boxes.Num(), 1);
	TestFalse(TEXT("拒绝超大尺寸"), Doc.Resize(MAX_int32, MAX_int32));
	TestFalse(TEXT("拒绝零尺寸"), Doc.Resize(0, 3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanEditorHistoryTest, "Sokoban.Editor.DirtyAndHistory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSokobanEditorHistoryTest::RunTest(const FString& Parameters)
{
	FSokobanEditorDocument Doc;
	Doc.NewLevel();
	TestTrue(TEXT("新草稿未保存"), Doc.IsDirty());
	Doc.MarkSaved();
	TestFalse(TEXT("保存基线后干净"), Doc.IsDirty());
	Doc.ApplyCell({0, 0}, ESokobanTerrain::Wall, false, ESokobanCellOccupant::None);
	TestTrue(TEXT("修改后脏标志"), Doc.IsDirty());
	Doc.Undo();
	TestFalse(TEXT("撤销到保存点恢复干净"), Doc.IsDirty());
	TestTrue(TEXT("可以重做"), Doc.CanRedo());
	Doc.ApplyCell({1, 0}, ESokobanTerrain::Wall, false, ESokobanCellOccupant::None);
	TestFalse(TEXT("新分支清除重做"), Doc.CanRedo());
	for (int32 Index = 0; Index < 160; ++Index)
	{
		auto Next = Doc.GetData();
		Next.LevelId = FName(*FString::Printf(TEXT("Level_%d"), Index));
		Doc.Commit(Next);
	}
	int32 Undos = 0;
	while (Doc.Undo()) { ++Undos; }
	TestEqual(TEXT("历史上限限制内存"), Undos, FSokobanEditorDocument::MaxHistory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanEditorAssetTest, "Sokoban.Editor.AssetCopyIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSokobanEditorAssetTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<USokobanLevelData> Asset(NewObject<USokobanLevelData>());
	FSokobanEditorDocument Doc;
	Doc.NewLevel(3, 3);
	auto Named = Doc.GetData();
	Named.LevelId = TEXT("EditorTest");
	Named.DisplayName = FText::FromString(TEXT("测试关卡"));
	Doc.Commit(Named);
	Doc.ApplyCell({0, 0}, ESokobanTerrain::Floor, false, ESokobanCellOccupant::Player);
	Doc.ApplyCell({1, 1}, ESokobanTerrain::Floor, true, ESokobanCellOccupant::Box);
	TestTrue(TEXT("完整草稿复用规则校验"), Doc.Validate().IsEmpty());
	Doc.GetData().WriteTo(*Asset.Get());
	FSokobanEditorDocument Reloaded;
	Reloaded.Load(FSokobanEditorSnapshot::FromAsset(*Asset.Get()));
	TestTrue(TEXT("写入资产再读取数据一致"), Reloaded.GetData().Equals(Doc.GetData()));
	Reloaded.ApplyCell({1, 1}, ESokobanTerrain::Wall, false, ESokobanCellOccupant::None);
	TestEqual(TEXT("编辑副本不会修改原资产"), Asset->Definition.Boxes.Num(), 1);
	TestFalse(TEXT("移除箱子与目标会报告不完整"), Reloaded.Validate().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanEditorMalformedTest, "Sokoban.Editor.MalformedDraft", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSokobanEditorMalformedTest::RunTest(const FString& Parameters)
{
	FSokobanEditorSnapshot Broken;
	Broken.Definition.Width = MAX_int32;
	Broken.Definition.Height = MAX_int32;
	Broken.Definition.Terrain.Add(ESokobanTerrain::Wall);
	FSokobanEditorDocument Doc;
	Doc.Load(Broken);
	TestFalse(TEXT("损坏数据不生成巨大网格"), Doc.CanDisplayGrid());
	TestFalse(TEXT("损坏网格不能直接摆放"), Doc.ApplyCell({0, 0}, ESokobanTerrain::Floor, false, ESokobanCellOccupant::Box));
	TestTrue(TEXT("可显式修复尺寸"), Doc.Resize(3, 3));
	TestTrue(TEXT("修复后可显示"), Doc.CanDisplayGrid());
	TestTrue(TEXT("保留已有第一格"), Doc.GetData().Definition.Terrain[0] == ESokobanTerrain::Wall);
	Doc.Undo();
	TestTrue(TEXT("修复也可撤销，不静默改原数据"), Doc.GetData().Equals(Broken));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanEditorWidgetTest, "Sokoban.Editor.PanelConstruction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSokobanEditorWidgetTest::RunTest(const FString& Parameters)
{
	// 在真正的 Slate 环境中构建完整面板，检查属性绑定和网格初始化时序。
	const TSharedRef<SSokobanLevelEditor> Panel = SNew(SSokobanLevelEditor);
	TestTrue(TEXT("编辑器面板成功构造"), Panel->GetChildren()->Num() > 0);
	// 可选离屏渲染用于人工检查布局。普通无界面测试不要求 GPU 或输出图片。
	if (FParse::Param(FCommandLine::Get(), TEXT("SokobanEditorPreview")))
	{
		TStrongObjectPtr<USokobanLevelData> PreviewAsset(NewObject<USokobanLevelData>());
		PreviewAsset->LevelId = TEXT("Preview_Level01");
		PreviewAsset->DisplayName = FText::FromString(TEXT("第一关：两个箱子"));
		PreviewAsset->Definition = ASokobanGameMode::MakeDemoLevel();
		const auto Preview = SNew(SSokobanLevelEditor).InitialAsset(PreviewAsset.Get());
		FWidgetRenderer Renderer(true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(Renderer.DrawWidget(Preview, FVector2D(1280, 800)));
		if (!TestNotNull(TEXT("生成面板预览"), Target.Get())) { return false; }
		FlushRenderingCommands();
		// AutoWrapText 在取得实际列宽后于下一次布局换行，预览需等布局稳定。
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			Preview->SlatePrepass();
			Renderer.DrawWidget(Target.Get(), Preview, FVector2D(1280, 800), 1.f / 60.f);
			FlushRenderingCommands();
		}
		FImage Image;
		if (!TestTrue(TEXT("读取预览像素"), FImageUtils::GetRenderTargetImage(Target.Get(), Image))) { return false; }
		const FString Folder = FPaths::ProjectSavedDir() / TEXT("Automation/LevelEditorPreview");
		IFileManager::Get().MakeDirectory(*Folder, true);
		TestTrue(TEXT("保存面板预览 PNG"), FImageUtils::SaveImageByExtension(*(Folder / TEXT("Panel.png")), Image));
	}
	return true;
}

#endif
