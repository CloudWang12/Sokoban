#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SokobanUITestFixtures.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sokoban/Data/SokobanLevelCatalog.h"
#include "Sokoban/Gameplay/SokobanGameMode.h"
#include "Sokoban/Presentation/SokobanBoardActor.h"
#include "Sokoban/Progress/SokobanProgressSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanFrontendWidgetTest, "Sokoban.Frontend.UMGScreens", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSokobanFrontendWidgetTest::RunTest(const FString& Parameters)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->AddToRoot();
	Instance->InitializeStandalone();
	auto* World = Instance->GetWorld();
	auto* Viewport = NewObject<UGameViewportClient>(GEngine);
	Instance->GetWorldContext()->GameViewport = Viewport;
	Viewport->Init(*Instance->GetWorldContext(), Instance, false);
	const FString Slot = TEXT("SokobanAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	auto* Progress = Instance->GetSubsystem<USokobanProgressSubsystem>();
	Progress->SetAutomationSlot(Slot);
	FString Error;
	auto* Player = Instance->CreateLocalPlayer(0, Error, false);
	auto* PC = World->SpawnActor<APlayerController>();
	if (Player) { PC->SetPlayer(Player); }
	auto* Catalog = NewObject<USokobanLevelCatalog>(Instance);
	auto* Level = NewObject<USokobanLevelData>(Catalog);
	Level->LevelId = TEXT("PreviewLevel");
	Level->DisplayName = FText::FromString(TEXT("测试关卡"));
	Level->Definition = ASokobanGameMode::MakeDemoLevel();
	Catalog->Levels = {Level};
	FURL URL;
	URL.AddOption(TEXT("game=/Script/Sokoban.SokobanGameMode"));
	World->SetGameMode(URL);
	auto* Mode = World->GetAuthGameMode<ASokobanGameMode>();
	Mode->LevelCatalog = Catalog;
	Mode->DispatchBeginPlay();
	auto* Flow = Instance->GetSubsystem<USokobanFlowSubsystem>();
	if (TestNotNull(TEXT("本地玩家"), Player))
	{
		TStrongObjectPtr<UWidgetBlueprint> Blueprint(MakeSokobanUITestBlueprint(USokobanMainMenuWidget::StaticClass()));
		TStrongObjectPtr<USokobanScreen> Widget(CreateWidget<USokobanScreen>(PC, Blueprint->GeneratedClass.Get()));
		TSharedPtr<SWidget> Slate = Widget->TakeWidget();
		TestEqual(TEXT("保留 WBP 根布局"), Widget->WidgetTree->RootWidget->GetFName(), FName(TEXT("DesignerOwnsThisRoot")));
		TArray<UWidget*> Widgets;
		Widget->WidgetTree->GetAllWidgets(Widgets);
		TestEqual(TEXT("C++ 没有额外创建视觉控件"), Widgets.Num(), 1);
		int32 Notifications = 0;
		Widget->OnViewDataChangedNative().AddLambda([&](const FSokobanUIViewData&) { ++Notifications; });
		Widget->RefreshViewData(true);
		Widget->RefreshViewData();
		TestEqual(TEXT("数据未变不重复通知"), Notifications, 1);
		TestTrue(TEXT("主菜单快照"), Widget->GetViewData().State == ESokobanFlowState::MainMenu);
		TestFalse(TEXT("没有待确认操作不能确认"), Widget->ResolveConfirmation(true));
		TestTrue(TEXT("选关命令"), Widget->RequestAction(ESokobanUIAction::Select));
		auto Entries = Widget->GetLevelEntries();
		TestEqual(TEXT("目录数据条数"), Entries.Num(), 1);
		TestTrue(TEXT("合法目录可以尝试开始"), Entries[0].bCanStart);
		TestEqual(TEXT("保留真实目录索引"), Entries[0].Index, 0);
		TestFalse(TEXT("非法索引拒绝"), Widget->RequestAction(ESokobanUIAction::StartLevel, 42));
		TestTrue(TEXT("开始关卡"), Widget->RequestAction(ESokobanUIAction::StartLevel, Entries[0].Index));
		TestFalse(TEXT("初始不可撤销"), Widget->GetViewData().bCanUndo);
		Mode->GetBoard()->MoveDuration = 10.f;
		Mode->RequestMove(ESokobanDirection::Up);
		TestTrue(TEXT("移动后动画锁可读"), Widget->GetViewData().bBusy);
		TestFalse(TEXT("动画期间按钮不可撤销"), Widget->GetViewData().bCanUndo);
		TestFalse(TEXT("动画期间后端拒绝撤销"), Widget->RequestAction(ESokobanUIAction::Undo));
		Mode->GetBoard()->Tick(20.f);
		Mode->GetBoard()->MoveDuration = 0.f;
		TestTrue(TEXT("动画结束可撤销"), Widget->GetViewData().bCanUndo);
		TestEqual(TEXT("快捷键/游戏请求同步到快照"), Widget->GetViewData().Counters.MoveCount, 1);
		TestTrue(TEXT("UI 撤销入口"), Widget->RequestAction(ESokobanUIAction::Undo));
		TestEqual(TEXT("撤销恢复计数"), Widget->GetViewData().Counters.MoveCount, 0);
		TestTrue(TEXT("撤销后可重做"), Widget->GetViewData().bCanRedo);
		for (auto Direction : {ESokobanDirection::Up, ESokobanDirection::Down, ESokobanDirection::Right, ESokobanDirection::Right, ESokobanDirection::Up}) { Mode->RequestMove(Direction); }
		Flow->TryComplete();
		const auto Result = Widget->GetViewData();
		TestTrue(TEXT("结算快照"), Result.State == ESokobanFlowState::Results);
		TestEqual(TEXT("冻结结算步数"), Result.Counters.MoveCount, 5);
		TestTrue(TEXT("结算有最佳成绩"), Result.bHasBest);
		TestFalse(TEXT("最后一关没有下一关"), Result.bHasNextLevel);
		TestFalse(TEXT("结算后不能从 UI 撤销"), Widget->RequestAction(ESokobanUIAction::Undo));
		TestTrue(TEXT("选关数据更新成绩"), Widget->GetLevelEntries()[0].bCompleted);
		++Level->ContentRevision;
		TestFalse(TEXT("新版关卡不展示旧版成绩"), Widget->GetLevelEntries()[0].bCompleted);
		Catalog->Levels.Add(nullptr);
		Entries = Widget->GetLevelEntries();
		TestEqual(TEXT("空引用也保留索引"), Entries[1].Index, 1);
		TestFalse(TEXT("目录非法时禁止开始"), Entries[0].bCanStart);
		Catalog->Levels.Pop();
		auto* FutureSave = NewObject<USokobanSaveGame>();
		FutureSave->SaveVersion = 999;
		UGameplayStatics::SaveGameToSlot(FutureSave, Slot, 0);
		Progress->SetAutomationSlot(Slot);
		TestTrue(TEXT("损坏存档写保护"), Widget->GetViewData().bWriteBlocked);
		TestFalse(TEXT("修复必须先确认"), Widget->RequestAction(ESokobanUIAction::Repair));
		TestTrue(TEXT("确认状态可供自定义弹窗读取"), Widget->HasPendingConfirmation());
		TestFalse(TEXT("重复点击不会代替确认"), Widget->RequestAction(ESokobanUIAction::Repair));
		TestTrue(TEXT("取消确认"), Widget->ResolveConfirmation(false));
		TestTrue(TEXT("取消不解除保护"), Progress->IsWriteBlocked());
		Widget->RequestAction(ESokobanUIAction::Quit);
		TestTrue(TEXT("未保存退出请求确认"), Widget->HasPendingConfirmation());
		Slate.Reset();
		Widget->ReleaseSlateResources(true);
		TestFalse(TEXT("页面销毁取消旧确认"), Widget->HasPendingConfirmation());
		TestFalse(TEXT("旧页面回调不能执行操作"), Widget->RequestAction(ESokobanUIAction::MainMenu));
		Instance->RemoveLocalPlayer(Player);
	}
	Mode->EndPlay(EEndPlayReason::RemovedFromWorld);
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	Instance->Shutdown();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	Instance->RemoveFromRoot();
	return true;
}
#endif
