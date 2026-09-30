#include "SokobanFlowSubsystem.h"
#include "Engine/GameInstance.h"
#include "Sokoban/Data/SokobanLevelCatalog.h"
#include "Sokoban/Gameplay/SokobanGameMode.h"
#include "Sokoban/Presentation/SokobanBoardActor.h"
#include "Sokoban/Progress/SokobanProgressSubsystem.h"
#include "Sokoban/Rules/SokobanLevelValidator.h"

void USokobanFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<USokobanProgressSubsystem>();
}

USokobanProgressSubsystem* USokobanFlowSubsystem::GetProgress() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<USokobanProgressSubsystem>() : nullptr;
}

bool USokobanFlowSubsystem::CheckCatalog()
{
	TArray<FText> Errors;
	if (!Catalog) { Status = FText::FromString(TEXT("未配置关卡目录。请在 GameMode 中指定 Level Catalog。")); return false; }
	if (!Catalog->ValidateCatalog(Errors))
	{
		FString Message;
		for (const auto& Error : Errors) { Message += Error.ToString() + TEXT("\n"); }
		Status = FText::FromString(Message);
		return false;
	}
	Status = FText::GetEmpty();
	return true;
}

void USokobanFlowSubsystem::Attach(ASokobanGameMode* Mode, USokobanLevelCatalog* InCatalog)
{
	GameMode = Mode;
	Catalog = InCatalog;
	CurrentIndex = INDEX_NONE;
	Completion = FSokobanCompletion();
	bCompletionHandled = false;
	State = ESokobanFlowState::MainMenu;
	CheckCatalog();
	SetBoardVisible(false);
	Changed.Broadcast();
}

void USokobanFlowSubsystem::Detach(ASokobanGameMode* Mode)
{
	if (GameMode.Get() != Mode) { return; }
	GameMode.Reset();
	Catalog = nullptr;
	CurrentIndex = INDEX_NONE;
	State = ESokobanFlowState::MainMenu;
	Completion = FSokobanCompletion();
	// 世界销毁时不广播 UI 创建事件；控制器负责移除旧视口控件与订阅。
}

bool USokobanFlowSubsystem::IsAttachedTo(const ASokobanGameMode* Mode) const { return Mode && GameMode.Get() == Mode; }

bool USokobanFlowSubsystem::CanAcceptGameplay(const ASokobanGameMode* Mode) const
{
	return IsAttachedTo(Mode) && State == ESokobanFlowState::Playing && !bTransition && !bCompletionHandled;
}

void USokobanFlowSubsystem::SetBoardVisible(bool bVisible)
{
	if (GameMode.IsValid() && GameMode->GetBoard()) { GameMode->GetBoard()->SetActorHiddenInGame(!bVisible); }
}

bool USokobanFlowSubsystem::ShowPage(ESokobanFlowState Page)
{
	if (bTransition || !GameMode.IsValid() || GameMode->IsPresentationBusy()) { return false; }
	TGuardValue<bool> Guard(bTransition, true);
	State = Page;
	CheckCatalog();
	SetBoardVisible(false);
	Changed.Broadcast();
	return true;
}

bool USokobanFlowSubsystem::ShowMainMenu() { return ShowPage(ESokobanFlowState::MainMenu); }
bool USokobanFlowSubsystem::ShowLevelSelect() { return ShowPage(ESokobanFlowState::LevelSelect); }
bool USokobanFlowSubsystem::Replay() { return StartLevel(CurrentIndex); }
bool USokobanFlowSubsystem::HasNextLevel() const { return Catalog && Catalog->GetNextIndex(CurrentIndex) != INDEX_NONE; }
bool USokobanFlowSubsystem::NextLevel()
{
	return State == ESokobanFlowState::Results && HasNextLevel() && StartLevel(Catalog->GetNextIndex(CurrentIndex));
}

bool USokobanFlowSubsystem::StartLevel(int32 Index)
{
	if (bTransition || !GameMode.IsValid() || GameMode->IsPresentationBusy()) { return false; }
	TGuardValue<bool> Guard(bTransition, true);
	if (!CheckCatalog()) { Changed.Broadcast(); return false; }
	const auto* Level = Catalog->GetLevel(Index);
	if (!Level) { Status = FText::FromString(TEXT("所选关卡不存在。")); Changed.Broadcast(); return false; }
	for (const auto& Issue : FSokobanLevelValidator::Validate(Level->Definition))
	{
		if (Issue.Severity == ESokobanValidationSeverity::Error)
		{
			Status = FText::FromString(TEXT("关卡不能开始：") + Issue.Message.ToString());
			Changed.Broadcast();
			return false; // 校验前不更改当前索引或原 Session。
		}
	}
	bool bStarted = false;
	{
		TGuardValue<bool> Starting(bStartingLevel, true);
		bStarted = GameMode->InitializeLevel(Level->Definition);
	}
	if (!bStarted)
	{
		State = ESokobanFlowState::LevelSelect;
		CurrentIndex = INDEX_NONE;
		Status = GameMode->GetStatusMessage();
		SetBoardVisible(false); // 表现构建失败不能把旧棋盘当作新关卡继续游戏。
		Changed.Broadcast();
		return false;
	}
	CurrentIndex = Index;
	Completion = FSokobanCompletion();
	Completion.LevelId = Level->LevelId;
	Completion.DisplayName = Level->DisplayName.IsEmpty() ? FText::FromName(Level->LevelId) : Level->DisplayName;
	Completion.ContentRevision = Level->ContentRevision;
	bCompletionHandled = false;
	State = ESokobanFlowState::Playing;
	Status = FText::GetEmpty();
	SetBoardVisible(true);
	if (auto* Progress = GetProgress()) { Progress->SetLastPlayedLevel(Completion.LevelId); Progress->Save(); }
	Changed.Broadcast();
	return true;
}

void USokobanFlowSubsystem::TryComplete()
{
	if (bTransition || bCompletionHandled || State != ESokobanFlowState::Playing || !GameMode.IsValid() || !GameMode->IsCompletionVisible()) { return; }
	TGuardValue<bool> Guard(bTransition, true);
	bCompletionHandled = true; // 先封闭入口，通知或下一帧不能重复结算。
	Completion.Counters = GameMode->GetSession()->GetState().Counters;
	State = ESokobanFlowState::Results;
	if (auto* Progress = GetProgress())
	{
		Progress->RecordCompletion(Completion.LevelId, Completion.ContentRevision, Completion.Counters, Completion.bNewBest);
		Progress->Save(); // 失败也进入结算，内存成绩仍可查询并重试。
	}
	Changed.Broadcast();
}
