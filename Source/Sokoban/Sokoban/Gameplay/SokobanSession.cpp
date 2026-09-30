#include "SokobanSession.h"

#include "Templates/UnrealTemplate.h"
#include "Sokoban/Rules/SokobanLevelValidator.h"
#include "Sokoban/Rules/SokobanRules.h"

#define LOCTEXT_NAMESPACE "SokobanSession"

bool USokobanSession::Initialize(const FSokobanLevelDefinition& InLevel, TArray<FSokobanValidationIssue>& OutIssues)
{
	OutIssues.Reset();
	if (bOperationInProgress)
	{
		FSokobanValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
		Issue.Severity = ESokobanValidationSeverity::Error;
		Issue.Code = TEXT("SessionBusy");
		Issue.Message = LOCTEXT("SessionBusy", "当前操作尚未完成，请在状态通知结束后再初始化关卡。");
		return false;
	}
	TGuardValue<bool> OperationGuard(bOperationInProgress, true);

	// 先校验输入。新关卡无效时保留原对局，避免一次加载失败丢掉玩家进度。
	OutIssues = FSokobanLevelValidator::Validate(InLevel);
	for (const FSokobanValidationIssue& Issue : OutIssues)
	{
		if (Issue.Severity == ESokobanValidationSeverity::Error)
		{
			return false;
		}
	}

	FSokobanBoardState NewState;
	NewState.PlayerPosition = InLevel.PlayerSpawn;
	for (const FSokobanBoxDefinition& Box : InLevel.Boxes)
	{
		NewState.BoxPositions.Add(Box.BoxId, Box.Position);
	}

	// 保存各自独立的副本：外部配置、初始快照和当前棋盘不会相互污染。
	LevelDefinition = InLevel;
	InitialState = NewState;
	CurrentState = MoveTemp(NewState);
	UndoStack.Reset();
	RedoStack.Reset();
	bInitialized = true;
	NotifyChange(ESokobanSessionChange::Initialized);
	return true;
}

FSokobanMoveResult USokobanSession::RequestMove(ESokobanDirection Direction)
{
	FSokobanMoveResult Result;
	if (bOperationInProgress || !bInitialized || bSolved)
	{
		Result.Outcome = ESokobanMoveOutcome::InvalidState;
		return Result;
	}
	TGuardValue<bool> OperationGuard(bOperationInProgress, true);
	Result = FSokobanRules::TryBuildMove(LevelDefinition, CurrentState, Direction);
	if (Result.Outcome != ESokobanMoveOutcome::Walk && Result.Outcome != ESokobanMoveOutcome::Push)
	{
		// 撞墙、无效方向等失败输入不会清空重做分支，也不广播状态变化。
		return Result;
	}
	if (!FSokobanRules::ApplyMove(LevelDefinition, CurrentState, Result.Record))
	{
		Result = FSokobanMoveResult();
		Result.Outcome = ESokobanMoveOutcome::InvalidState;
		return Result;
	}

	// 先确认 Rules 已经成功修改棋盘，再更新历史；新的成功操作会建立新分支。
	UndoStack.Add(Result.Record);
	RedoStack.Reset();
	NotifyChange(ESokobanSessionChange::Move, Result.Record);
	return Result;
}

bool USokobanSession::Undo()
{
	if (bOperationInProgress || !CanUndo())
	{
		return false;
	}
	TGuardValue<bool> OperationGuard(bOperationInProgress, true);
	// 复制栈顶记录，避免弹出或调整容器后还持有无效引用。
	const FSokobanMoveRecord Record = UndoStack.Last();
	if (!FSokobanRules::RevertMove(LevelDefinition, CurrentState, Record))
	{
		return false;
	}
	UndoStack.Pop(EAllowShrinking::No);
	RedoStack.Add(Record);
	NotifyChange(ESokobanSessionChange::Undo, Record);
	return true;
}

bool USokobanSession::Redo()
{
	if (bOperationInProgress || !CanRedo())
	{
		return false;
	}
	TGuardValue<bool> OperationGuard(bOperationInProgress, true);
	const FSokobanMoveRecord Record = RedoStack.Last();
	if (!FSokobanRules::ApplyMove(LevelDefinition, CurrentState, Record))
	{
		return false;
	}
	RedoStack.Pop(EAllowShrinking::No);
	UndoStack.Add(Record);
	NotifyChange(ESokobanSessionChange::Redo, Record);
	return true;
}

bool USokobanSession::Restart()
{
	if (bOperationInProgress || !bInitialized)
	{
		return false;
	}
	TGuardValue<bool> OperationGuard(bOperationInProgress, true);
	CurrentState = InitialState;
	UndoStack.Reset();
	RedoStack.Reset();
	NotifyChange(ESokobanSessionChange::Restart);
	return true;
}

void USokobanSession::NotifyChange(ESokobanSessionChange Change, const FSokobanMoveRecord& Record)
{
	FSokobanSessionUpdate Update;
	Update.Change = Change;
	Update.Record = Record;
	Update.bIsSolved = FSokobanRules::IsSolved(LevelDefinition, CurrentState);
	Update.bSolvedChanged = bSolved != Update.bIsSolved;
	bSolved = Update.bIsSolved;

	// 两种绑定方式接收同一份通知；广播期间所有查询都能读取一致的最新状态。
	// OperationGuard 此时仍然有效，监听者应在回调结束后再提交下一次操作。
	SessionChangedNative.Broadcast(Update);
	OnSessionChanged.Broadcast(Update);
}

#undef LOCTEXT_NAMESPACE
