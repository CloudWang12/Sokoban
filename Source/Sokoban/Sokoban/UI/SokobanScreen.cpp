#include "SokobanScreen.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sokoban/Data/SokobanLevelCatalog.h"
#include "Sokoban/Gameplay/SokobanGameMode.h"
#include "Sokoban/Gameplay/SokobanPlayerController.h"
#include "Sokoban/Progress/SokobanProgressSubsystem.h"

USokobanFlowSubsystem* USokobanScreen::Flow() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<USokobanFlowSubsystem>() : nullptr;
}

FSokobanUIViewData USokobanScreen::GetViewData() const
{
	FSokobanUIViewData Data;
	const auto* CurrentFlow = Flow();
	const auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ASokobanGameMode>() : nullptr;
	if (!CurrentFlow || !Mode || !CurrentFlow->IsAttachedTo(Mode)) { return Data; }
	Data.bAvailable = true;
	Data.State = CurrentFlow->GetState();
	Data.CurrentIndex = CurrentFlow->GetCurrentIndex();
	Data.Completion = CurrentFlow->GetCompletion();
	Data.Status = CurrentFlow->GetStatus();
	Data.bHasNextLevel = Data.State == ESokobanFlowState::Results && CurrentFlow->HasNextLevel();
	Data.bConfirmationPending = bConfirmationPending;
	if (Data.State == ESokobanFlowState::Playing)
	{
		Data.bBusy = Mode->IsPresentationBusy();
		if (Data.Status.IsEmpty()) { Data.Status = Mode->GetStatusMessage(); }
		if (const auto* PC = Cast<ASokobanPlayerController>(GetOwningPlayer()))
		{
			if (!PC->GetInputConfigurationError().IsEmpty()) { Data.Status = PC->GetInputConfigurationError(); }
		}
		if (const auto* Session = Mode->GetSession())
		{
			Data.Counters = Session->GetState().Counters;
			Data.UndoCount = Session->GetUndoCount();
			Data.RedoCount = Session->GetRedoCount();
			const bool bCanOperate = Mode->IsReady() && Mode->CanAcceptPlayerInput() && !Data.bBusy;
			Data.bCanUndo = bCanOperate && Session->CanUndo();
			Data.bCanRedo = bCanOperate && Session->CanRedo();
			Data.bCanRestart = bCanOperate;
		}
	}
	else if (Data.State == ESokobanFlowState::Results) { Data.Counters = Data.Completion.Counters; }
	if (const auto* Progress = CurrentFlow->GetProgress())
	{
		Data.bUnsavedChanges = Progress->HasUnsavedChanges();
		Data.bWriteBlocked = Progress->IsWriteBlocked();
		Data.SaveStatus = Progress->GetStatus();
		if (const auto* Catalog = CurrentFlow->GetCatalog())
		{
			Data.LastPlayedIndex = Catalog->FindLevelIndex(Progress->GetLastPlayedLevel());
		}
		FSokobanLevelProgress Saved;
		Data.bHasBest = Progress->GetProgress(Data.Completion.LevelId, Data.Completion.ContentRevision, Saved);
		if (Data.bHasBest) { Data.Best = Saved.Best; }
	}
	return Data;
}

TArray<FSokobanLevelListEntry> USokobanScreen::GetLevelEntries() const
{
	TArray<FSokobanLevelListEntry> Entries;
	const auto* CurrentFlow = Flow();
	const auto* Catalog = CurrentFlow ? CurrentFlow->GetCatalog() : nullptr;
	if (!Catalog) { return Entries; }
	TArray<FText> Errors;
	const bool bValidCatalog = Catalog->ValidateCatalog(Errors);
	const auto* Progress = CurrentFlow->GetProgress();
	for (int32 Index = 0; Index < Catalog->Levels.Num(); ++Index)
	{
		FSokobanLevelListEntry Entry;
		Entry.Index = Index;
		if (const auto* Level = Catalog->GetLevel(Index))
		{
			Entry.LevelId = Level->LevelId;
			Entry.DisplayName = Level->DisplayName.IsEmpty() ? FText::FromName(Level->LevelId) : Level->DisplayName;
			Entry.ContentRevision = Level->ContentRevision;
			Entry.bCanStart = bValidCatalog;
			FSokobanLevelProgress Saved;
			Entry.bCompleted = Progress && Progress->GetProgress(Level->LevelId, Level->ContentRevision, Saved);
			if (Entry.bCompleted) { Entry.Best = Saved.Best; }
		}
		// 空引用保留原索引，避免 UI 过滤后把关卡编号当成目录索引。
		Entries.Add(Entry);
	}
	return Entries;
}

void USokobanScreen::NativeConstruct()
{
	bActive = true;
	Super::NativeConstruct();
	RefreshViewData(true);
}

void USokobanScreen::NativeDestruct()
{
	bActive = false;
	bConfirmationPending = false;
	bHasData = false;
	Super::NativeDestruct();
}

void USokobanScreen::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
	Super::NativeTick(Geometry, DeltaSeconds);
	// 同步动画锁释放、快捷键操作与保存结果；仅比较小型快照，不逐帧重建列表或布局。
	RefreshViewData();
}

void USokobanScreen::RefreshViewData(bool bForce)
{
	if (!bActive || bNotifying) { return; }
	const FSokobanUIViewData Data = GetViewData();
	if (!bForce && bHasData && FSokobanUIViewData::StaticStruct()->CompareScriptStruct(&LastData, &Data, 0)) { return; }
	LastData = Data;
	bHasData = true;
	TGuardValue<bool> Guard(bNotifying, true);
	ViewDataChanged.Broadcast(Data);
	OnViewDataChanged(Data);
}

bool USokobanScreen::CanIssueAction() const
{
	const auto* CurrentFlow = Flow();
	const auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ASokobanGameMode>() : nullptr;
	return bActive && !bNotifying && !bHandlingAction && CurrentFlow && Mode && CurrentFlow->IsAttachedTo(Mode);
}

void USokobanScreen::Execute(ESokobanUIAction Action, int32 Index, USokobanActionButton* Source)
{
	RequestAction(Action, Index);
}

void USokobanScreen::RequestConfirmation(ESokobanUIAction Action, const FText& Reason)
{
	bConfirmationPending = true;
	PendingAction = Action;
	RefreshViewData();
	OnConfirmationRequested(Action, Reason);
}

bool USokobanScreen::RequestAction(ESokobanUIAction Action, int32 Index)
{
	if (!CanIssueAction()) { return false; }
	TGuardValue<bool> Guard(bHandlingAction, true);
	auto* CurrentFlow = Flow();
	auto* Mode = GetWorld()->GetAuthGameMode<ASokobanGameMode>();
	auto* Progress = CurrentFlow->GetProgress();
	// 已有确认框时，重复点击不能当作同意，也不允许另一个命令替换待确认操作。
	if (bConfirmationPending) { return false; }
	bool bAccepted = false;
	switch (Action)
	{
	case ESokobanUIAction::Select: bAccepted = CurrentFlow->ShowLevelSelect(); break;
	case ESokobanUIAction::MainMenu: bAccepted = CurrentFlow->ShowMainMenu(); break;
	case ESokobanUIAction::StartLevel: bAccepted = CurrentFlow->StartLevel(Index); break;
	case ESokobanUIAction::Replay: bAccepted = CurrentFlow->Replay(); break;
	case ESokobanUIAction::Next: bAccepted = CurrentFlow->NextLevel(); break;
	case ESokobanUIAction::Undo: bAccepted = Mode->RequestUndo(); break;
	case ESokobanUIAction::Redo: bAccepted = Mode->RequestRedo(); break;
	case ESokobanUIAction::Restart: bAccepted = Mode->RequestRestart(); break;
	case ESokobanUIAction::Save: bAccepted = Progress && Progress->Save(); break;
	case ESokobanUIAction::Repair:
		if (Progress && Progress->IsWriteBlocked())
		{
			RequestConfirmation(Action, FText::FromString(TEXT("备份原始存档后，以本次内存进度恢复保存。是否继续？")));
		}
		break;
	case ESokobanUIAction::Quit:
		if (Progress && (Progress->HasUnsavedChanges() || Progress->IsWriteBlocked()))
		{
			Progress->Save();
			if (Progress->HasUnsavedChanges() || Progress->IsWriteBlocked())
			{
				RequestConfirmation(Action, FText::FromString(TEXT("部分进度尚未保存，仍然退出游戏？")));
				break;
			}
		}
		UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
		bAccepted = true;
		break;
	}
	RefreshViewData();
	return bAccepted;
}

bool USokobanScreen::ResolveConfirmation(bool bConfirmed)
{
	if (!CanIssueAction() || !bConfirmationPending) { return false; }
	TGuardValue<bool> Guard(bHandlingAction, true);
	const ESokobanUIAction Action = PendingAction;
	bConfirmationPending = false; // 先消费，再执行，重复回调不会二次写盘或退出。
	bool bAccepted = !bConfirmed;
	if (bConfirmed)
	{
		if (Action == ESokobanUIAction::Repair)
		{
			auto* Progress = Flow()->GetProgress();
			bAccepted = Progress && Progress->RecoverSaving();
		}
		else if (Action == ESokobanUIAction::Quit)
		{
			UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
			bAccepted = true;
		}
	}
	RefreshViewData();
	return bAccepted;
}
