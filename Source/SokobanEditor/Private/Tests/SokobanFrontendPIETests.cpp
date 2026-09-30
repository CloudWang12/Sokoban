#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "SokobanUITestFixtures.h"
#include "UObject/StrongObjectPtr.h"
#include "Kismet/GameplayStatics.h"
#include "Sokoban/Flow/SokobanFlowSubsystem.h"
#include "Sokoban/Gameplay/SokobanGameMode.h"
#include "Sokoban/Gameplay/SokobanPlayerController.h"
#include "Sokoban/Presentation/SokobanBoardActor.h"
#include "Sokoban/Progress/SokobanProgressSubsystem.h"
#include "Sokoban/UI/SokobanScreen.h"
#include "Sokoban/UI/SokobanUIManagerComponent.h"

/** 在真实地图与蓝图控制器中走通 UI 命令；存档替换为专用随机槽，不改玩家进度。 */
class FSokobanPIEJourney : public IAutomationLatentCommand
{
public:
	explicit FSokobanPIEJourney(FAutomationTestBase* InTest) : Test(InTest), Started(FPlatformTime::Seconds()) {}
	virtual ~FSokobanPIEJourney() override
	{
		if (!Slot.IsEmpty()) { UGameplayStatics::DeleteGameInSlot(Slot, 0); }
	}
	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() - Started > 35.) { Test->AddError(TEXT("PIE 流程等待超时")); return true; }
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		if (!World || !World->HasBegunPlay()) { return false; }
		auto* Mode = World->GetAuthGameMode<ASokobanGameMode>();
		auto* PC = Cast<ASokobanPlayerController>(World->GetFirstPlayerController());
		if (!Mode || !PC || !PC->UIManager) { return false; }
		auto* Flow = World->GetGameInstance()->GetSubsystem<USokobanFlowSubsystem>();
		auto* Progress = Flow->GetProgress();
		if (Stage == 0)
		{
			Slot = TEXT("SokobanAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			Progress->SetAutomationSlot(Slot);
			// 不改用户 WBP 或蓝图配置；仅在当前 PIE 实例注入临时测试界面。
			PC->UIManager->MainMenuClass = nullptr;
			PC->UIManager->Refresh();
			Test->TestFalse(TEXT("未配置 WBP 时不生成 C++ 默认布局"), PC->UIManager->HasScreen());
			Test->TestFalse(TEXT("缺失配置有诊断"), PC->UIManager->GetConfigurationError().IsEmpty());
			for (UClass* Parent : {USokobanMainMenuWidget::StaticClass(), USokobanLevelSelectWidget::StaticClass(), USokobanPlayWidget::StaticClass(), USokobanResultsWidget::StaticClass()})
			{
				Fixtures.Emplace(MakeSokobanUITestBlueprint(Parent));
			}
			PC->UIManager->MainMenuClass = Fixtures[0]->GeneratedClass;
			PC->UIManager->LevelSelectClass = Fixtures[1]->GeneratedClass;
			PC->UIManager->PlayClass = Fixtures[2]->GeneratedClass;
			PC->UIManager->ResultsClass = Fixtures[3]->GeneratedClass;
			PC->UIManager->Refresh();
			if (!Test->TestNotNull(TEXT("蓝图界面创建成功"), PC->UIManager->GetScreen())) { return true; }
			auto* OriginalScreen = PC->UIManager->GetScreen();
			Flow->ShowMainMenu();
			Test->TestTrue(TEXT("同页通知复用 WBP 实例"), OriginalScreen == PC->UIManager->GetScreen());
			Test->TestTrue(TEXT("配置后清除诊断"), PC->UIManager->GetConfigurationError().IsEmpty());
			Test->TestTrue(TEXT("真实地图启用正式流程"), Mode->IsFlowManaged());
			Test->TestTrue(TEXT("主菜单 UMG"), PC->UIManager->GetScreen()->IsA<USokobanMainMenuWidget>());
			Test->TestTrue(TEXT("输入资产已配置"), PC->GetInputConfigurationError().IsEmpty());
			if (!Request(PC, ESokobanUIAction::Select)) { return true; }
			Test->TestTrue(TEXT("按钮进入选关"), Flow->GetState() == ESokobanFlowState::LevelSelect);
			if (!Request(PC, ESokobanUIAction::StartLevel, 0)) { return true; }
			Test->TestTrue(TEXT("目录 DA 成功进入游戏"), Flow->GetState() == ESokobanFlowState::Playing);
			if (!Mode->GetBoard()) { Test->AddError(TEXT("实际 DA 未生成棋盘")); return true; }
			Mode->GetBoard()->MoveDuration = 0.f;
			for (auto Direction : {ESokobanDirection::Up, ESokobanDirection::Down, ESokobanDirection::Right, ESokobanDirection::Right, ESokobanDirection::Up}) { Mode->RequestMove(Direction); }
			++Stage;
			return false; // 等待真实 GameMode Tick 自动提交结算。
		}
		if (Stage == 1)
		{
			if (Flow->GetState() != ESokobanFlowState::Results) { return false; }
			Test->TestTrue(TEXT("正式结算 UMG"), PC->UIManager->GetScreen()->IsA<USokobanResultsWidget>());
			Test->TestFalse(TEXT("结算后移动禁止"), Mode->CanAcceptPlayerInput());
			FSokobanLevelProgress Saved;
			Test->TestTrue(TEXT("成绩已记录"), Progress->GetProgress(TEXT("Level_001"), 1, Saved));
			Test->TestEqual(TEXT("正式成绩为 5 步"), Saved.Best.MoveCount, 5);
			if (!Request(PC, ESokobanUIAction::Next)) { return true; }
			Test->TestEqual(TEXT("下一关索引"), Flow->GetCurrentIndex(), 1);
			Test->TestEqual(TEXT("新关清空历史"), Mode->GetSession()->GetUndoCount(), 0);
			Test->TestTrue(TEXT("游戏 HUD 恢复"), PC->UIManager->GetScreen()->IsA<USokobanPlayWidget>());
			if (!Request(PC, ESokobanUIAction::Select)) { return true; }
			Test->TestTrue(TEXT("菜单期间棋盘隐藏"), Mode->GetBoard()->IsHidden());
			Progress->SetAutomationSlot(Slot);
			Test->TestTrue(TEXT("重新读盘保留通关"), Progress->GetProgress(TEXT("Level_001"), 1, Saved));
			return true;
		}
		return true;
	}
private:
	bool Request(ASokobanPlayerController* PC, ESokobanUIAction Action, int32 Index = -1)
	{
		auto* Screen = PC->UIManager->GetScreen();
		return Test->TestTrue(TEXT("蓝图可调用的 UI 命令成功"), Screen && Screen->RequestAction(Action, Index));
	}
	TArray<TStrongObjectPtr<UWidgetBlueprint>> Fixtures;
	FAutomationTestBase* Test;
	double Started;
	int32 Stage = 0;
	FString Slot;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanPIEJourneyTest, "Sokoban.Frontend.PIEJourney", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSokobanPIEJourneyTest::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("UIManager.MainMenuClass"), EAutomationExpectedErrorFlags::Contains, 1);
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(FString(TEXT("/Game/Sokoban/L_SokobanPlayerGround"))));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FSokobanPIEJourney>(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
