#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sokoban/Data/SokobanLevelCatalog.h"
#include "Sokoban/Flow/SokobanFlowSubsystem.h"
#include "Sokoban/Gameplay/SokobanGameMode.h"
#include "Sokoban/Presentation/SokobanBoardActor.h"
#include "Sokoban/Progress/SokobanProgressSubsystem.h"

namespace SokobanFrontendTests
{
	USokobanLevelData* MakeLevel(UObject* Outer, FName Id)
	{
		auto* Level = NewObject<USokobanLevelData>(Outer);
		Level->LevelId = Id;
		Level->Definition = ASokobanGameMode::MakeDemoLevel();
		return Level;
	}
	struct FContext
	{
		UGameInstance* Instance;
		UWorld* World;
		USokobanProgressSubsystem* Progress;
		FString Slot = TEXT("SokobanAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		FContext()
		{
			Instance = NewObject<UGameInstance>(GEngine);
			Instance->AddToRoot();
			Instance->InitializeStandalone();
			World = Instance->GetWorld();
			Progress = Instance->GetSubsystem<USokobanProgressSubsystem>();
			Progress->SetAutomationSlot(Slot);
		}
		~FContext()
		{
			UGameplayStatics::DeleteGameInSlot(Slot, 0); // 仅清理本次生成的随机测试槽。
			Instance->Shutdown();
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			Instance->RemoveFromRoot();
		}
	};
}
using namespace SokobanFrontendTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanCatalogTest, "Sokoban.Frontend.CatalogBoundaries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSokobanCatalogTest::RunTest(const FString& Parameters)
{
	auto* Catalog = NewObject<USokobanLevelCatalog>();
	TArray<FText> Errors;
	TestFalse(TEXT("空目录拒绝"), Catalog->ValidateCatalog(Errors));
	Catalog->Levels = { MakeLevel(Catalog, TEXT("A")), MakeLevel(Catalog, TEXT("B")) };
	TestTrue(TEXT("合法目录"), Catalog->ValidateCatalog(Errors));
	TestEqual(TEXT("按 ID 查询不依赖下标"), Catalog->FindLevelIndex(TEXT("B")), 1);
	TestEqual(TEXT("最后一关没有下一关"), Catalog->GetNextIndex(1), INDEX_NONE);
	TestEqual(TEXT("负下标不会成为第一关"), Catalog->GetNextIndex(-1), INDEX_NONE);
	Catalog->Levels[1]->LevelId = TEXT("A");
	TestFalse(TEXT("重复 ID 拒绝"), Catalog->ValidateCatalog(Errors));
	Catalog->Levels[1] = nullptr;
	TestFalse(TEXT("空引用拒绝"), Catalog->ValidateCatalog(Errors));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanScoreTest, "Sokoban.Frontend.ScoreAndRevision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSokobanScoreTest::RunTest(const FString& Parameters)
{
	auto* Save = NewObject<USokobanSaveGame>();
	bool bBest = false;
	FSokobanMoveCounters Counts;
	Counts.MoveCount = 10; Counts.PushCount = 5;
	TestTrue(TEXT("首次成绩"), Save->RecordCompletion(TEXT("A"), 1, Counts, bBest) && bBest);
	Counts.MoveCount = 11; Counts.PushCount = 2;
	Save->RecordCompletion(TEXT("A"), 1, Counts, bBest);
	TestFalse(TEXT("推动更少但步数更多不刷新"), bBest);
	FSokobanLevelProgress Best;
	Save->GetProgress(TEXT("A"), 1, Best);
	TestEqual(TEXT("不会拼接两次解法"), Best.Best.PushCount, 5);
	Counts.MoveCount = 10; Counts.PushCount = 4;
	Save->RecordCompletion(TEXT("A"), 1, Counts, bBest);
	TestTrue(TEXT("同样步数比较推动"), bBest);
	TestFalse(TEXT("新修订不显示旧成绩"), Save->GetProgress(TEXT("A"), 2, Best));
	Save->RecordCompletion(TEXT("A"), 2, Counts, bBest);
	TestTrue(TEXT("新修订首次成绩"), bBest);
	Counts.PushCount = 20;
	TestFalse(TEXT("非法计数拒绝"), Save->RecordCompletion(TEXT("A"), 2, Counts, bBest));
	TestTrue(TEXT("拒绝后原数据有效"), Save->IsValidPayload());
	Save->SaveVersion = 999;
	TestFalse(TEXT("未来版本拒绝"), Save->IsValidPayload());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanDiskProgressTest, "Sokoban.Frontend.DiskProgressRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSokobanDiskProgressTest::RunTest(const FString& Parameters)
{
	FContext Context;
	bool bBest = false;
	FSokobanMoveCounters Counts; Counts.MoveCount = 5; Counts.PushCount = 2;
	TestTrue(TEXT("记录完成"), Context.Progress->RecordCompletion(TEXT("DiskLevel"), 1, Counts, bBest));
	TestTrue(TEXT("真实写入独立槽位"), Context.Progress->Save());
	TestFalse(TEXT("保存后清除脏标记"), Context.Progress->HasUnsavedChanges());
	Context.Progress->SetAutomationSlot(Context.Slot);
	FSokobanLevelProgress Loaded;
	TestTrue(TEXT("重新读盘恢复成绩"), Context.Progress->GetProgress(TEXT("DiskLevel"), 1, Loaded));
	TestEqual(TEXT("成绩保持完整"), Loaded.Best.PushCount, 2);
	TestEqual(TEXT("最近关卡恢复"), Context.Progress->GetLastPlayedLevel(), FName(TEXT("DiskLevel")));
	// 用合法序列化、未知版本的文件验证保护分支，避免向引擎输入任意坏字节。
	auto* Future = NewObject<USokobanSaveGame>(); Future->SaveVersion = 99;
	TestTrue(TEXT("写入未来版本测试数据"), UGameplayStatics::SaveGameToSlot(Future, Context.Slot, 0));
	Context.Progress->SetAutomationSlot(Context.Slot);
	TestTrue(TEXT("不兼容文件写保护"), Context.Progress->IsWriteBlocked());
	Context.Progress->RecordCompletion(TEXT("NewMemory"), 1, Counts, bBest);
	TestFalse(TEXT("不静默覆盖"), Context.Progress->Save());
	const auto* Retained = Cast<USokobanSaveGame>(UGameplayStatics::LoadGameFromSlot(Context.Slot, 0));
	TestTrue(TEXT("原版本仍在磁盘"), Retained && Retained->SaveVersion == 99);
	TestTrue(TEXT("失败保留内存成绩"), Context.Progress->GetProgress(TEXT("NewMemory"), 1, Loaded));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanFrontendFlowTest, "Sokoban.Frontend.FlowAndSettlement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSokobanFrontendFlowTest::RunTest(const FString& Parameters)
{
	FContext Context;
	auto* Catalog = NewObject<USokobanLevelCatalog>(Context.Instance);
	Catalog->Levels = {MakeLevel(Catalog, TEXT("A")), MakeLevel(Catalog, TEXT("B"))};
	auto* Mode = Context.World->SpawnActor<ASokobanGameMode>();
	Mode->LevelCatalog = Catalog;
	Mode->DispatchBeginPlay();
	auto* Flow = Context.Instance->GetSubsystem<USokobanFlowSubsystem>();
	TestTrue(TEXT("开始主菜单"), Flow->GetState() == ESokobanFlowState::MainMenu);
	TestFalse(TEXT("菜单不允许玩法"), Mode->CanAcceptPlayerInput());
	TestTrue(TEXT("选关"), Flow->ShowLevelSelect());
	TestTrue(TEXT("开始第一关"), Flow->StartLevel(0));
	auto* Board = Mode->GetBoard();
	if (!TestNotNull(TEXT("棋盘"), Board)) { return false; }
	Board->MoveDuration = 0.f;
	for (auto Direction : {ESokobanDirection::Up, ESokobanDirection::Down, ESokobanDirection::Right, ESokobanDirection::Right}) { Mode->RequestMove(Direction); }
	Board->MoveDuration = .2f;
	Mode->RequestMove(ESokobanDirection::Up);
	Flow->TryComplete();
	TestTrue(TEXT("最后动画期间不结算"), Flow->GetState() == ESokobanFlowState::Playing);
	TestFalse(TEXT("动画期间不能跳关"), Flow->StartLevel(1));
	Board->Tick(1.f);
	Flow->TryComplete();
	TestTrue(TEXT("动画结束进入结算"), Flow->GetState() == ESokobanFlowState::Results);
	TestEqual(TEXT("结算步数"), Flow->GetCompletion().Counters.MoveCount, 5);
	TestFalse(TEXT("正式结算禁止撤销"), Mode->RequestUndo());
	TestFalse(TEXT("正式结算禁止原入口重开"), Mode->RequestRestart());
	const auto Handle = Flow->OnChanged().AddLambda([this] { AddError(TEXT("重复完成不应广播")); });
	Flow->TryComplete();
	Flow->OnChanged().Remove(Handle);
	TestTrue(TEXT("下一关"), Flow->NextLevel());
	TestEqual(TEXT("新关计数清空"), Mode->GetSession()->GetState().Counters.MoveCount, 0);
	TestFalse(TEXT("最后一关无下一关"), Flow->HasNextLevel());
	TestTrue(TEXT("返回选关"), Flow->ShowLevelSelect());
	TestTrue(TEXT("棋盘隐藏"), Board->IsHidden());
	Catalog->Levels[0]->Definition.PlayerSpawn = FIntPoint(-1,-1);
	TestFalse(TEXT("非法布局拒绝"), Flow->StartLevel(0));
	TestEqual(TEXT("失败不改索引"), Flow->GetCurrentIndex(), 1);
	TestTrue(TEXT("失败保留选关状态"), Flow->GetState() == ESokobanFlowState::LevelSelect);
	Mode->EndPlay(EEndPlayReason::RemovedFromWorld);
	TestFalse(TEXT("世界移除解绑"), Flow->IsAttachedTo(Mode));
	return true;
}
#endif
