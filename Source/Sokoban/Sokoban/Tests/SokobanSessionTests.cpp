#include "Sokoban/Gameplay/SokobanSession.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

namespace SokobanSessionTests
{
	FSokobanLevelDefinition MakeLevel()
	{
		FSokobanLevelDefinition Level;
		Level.Width = 6;
		Level.Height = 5;
		Level.Terrain.Init(ESokobanTerrain::Floor, 30);
		Level.Terrain[4 * Level.Width + 2] = ESokobanTerrain::Wall;
		Level.PlayerSpawn = FIntPoint(1, 2);
		FSokobanBoxDefinition Box;
		Box.BoxId = 7;
		Box.Position = FIntPoint(2, 2);
		Level.Boxes.Add(Box);
		Level.Goals.Add(FIntPoint(4, 2));
		return Level;
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
			if (!Other || *Other != Box.Value) { return false; }
		}
		return true;
	}

	bool IsSuccessful(const FSokobanMoveResult& Result)
	{
		return Result.Outcome == ESokobanMoveOutcome::Walk || Result.Outcome == ESokobanMoveOutcome::Push;
	}
}

using namespace SokobanSessionTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanSessionUninitializedTest,
	"Sokoban.Session.Uninitialized", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanSessionUninitializedTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<USokobanSession> Session(NewObject<USokobanSession>());
	const FSokobanBoardState Before = Session->GetState();
	TestFalse(TEXT("新对象尚未初始化"), Session->IsInitialized());
	TestFalse(TEXT("未初始化不能通关"), Session->IsSolved());
	TestFalse(TEXT("未初始化不能撤销"), Session->Undo());
	TestFalse(TEXT("未初始化不能重做"), Session->Redo());
	TestFalse(TEXT("未初始化不能重开"), Session->Restart());
	TestTrue(TEXT("未初始化移动返回无效状态"), Session->RequestMove(ESokobanDirection::Right).Outcome == ESokobanMoveOutcome::InvalidState);
	TestTrue(TEXT("失败不改变默认棋盘"), StatesEqual(Before, Session->GetState()));
	TestFalse(TEXT("没有撤销历史"), Session->CanUndo());
	TestFalse(TEXT("没有重做历史"), Session->CanRedo());
	TArray<FSokobanValidationIssue> Issues;
	TestFalse(TEXT("空草稿不能初始化"), Session->Initialize(FSokobanLevelDefinition(), Issues));
	TestTrue(TEXT("返回配置问题"), !Issues.IsEmpty());
	TestFalse(TEXT("初始化失败后仍未初始化"), Session->IsInitialized());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanSessionInitializationTest,
	"Sokoban.Session.InitializationAndCopies", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanSessionInitializationTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<USokobanSession> Session(NewObject<USokobanSession>());
	FSokobanLevelDefinition Level = MakeLevel();
	TArray<FSokobanValidationIssue> Issues;
	Issues.AddDefaulted();
	TestTrue(TEXT("合法关卡初始化成功"), Session->Initialize(Level, Issues));
	TestTrue(TEXT("成功后清除调用者遗留的错误列表"), Issues.IsEmpty());
	TestTrue(TEXT("初始化标志正确"), Session->IsInitialized());
	TestFalse(TEXT("初始棋盘未通关"), Session->IsSolved());
	const FSokobanBoardState Initial = Session->GetState();
	TestTrue(TEXT("玩家位于出生点"), Initial.PlayerPosition == Level.PlayerSpawn);
	TestTrue(TEXT("箱子位置按 ID 初始化"), Initial.BoxPositions.FindChecked(7) == Level.Boxes[0].Position);
	TestEqual(TEXT("初始移动计数为零"), Initial.Counters.MoveCount, 0);
	TestEqual(TEXT("初始推动计数为零"), Initial.Counters.PushCount, 0);

	// 外部修改输入或查询得到的副本，不能绕过规则改写内部数据。
	Level.Terrain.Reset();
	Level.Boxes.Reset();
	FSokobanBoardState StateCopy = Session->GetState();
	StateCopy.PlayerPosition = FIntPoint(999, 999);
	StateCopy.BoxPositions.Reset();
	FSokobanLevelDefinition LevelCopy = Session->GetLevelDefinition();
	LevelCopy.Goals.Reset();
	TestTrue(TEXT("状态查询返回独立副本"), StatesEqual(Initial, Session->GetState()));
	TestEqual(TEXT("关卡查询返回独立副本"), Session->GetLevelDefinition().Goals.Num(), 1);
	TestTrue(TEXT("输入数据后来被修改仍不影响移动"), Session->RequestMove(ESokobanDirection::Right).Outcome == ESokobanMoveOutcome::Push);
	TestTrue(TEXT("重开保留最初快照"), Session->Restart());
	TestTrue(TEXT("初始状态没有被游玩修改"), StatesEqual(Initial, Session->GetState()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanSessionHistoryTest,
	"Sokoban.Session.HistoryRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanSessionHistoryTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<USokobanSession> Session(NewObject<USokobanSession>());
	TArray<FSokobanValidationIssue> Issues;
	TestTrue(TEXT("初始化测试关卡"), Session->Initialize(MakeLevel(), Issues));
	TArray<FSokobanBoardState> Snapshots;
	Snapshots.Add(Session->GetState());
	for (const ESokobanDirection Direction : { ESokobanDirection::Right, ESokobanDirection::Down, ESokobanDirection::Right, ESokobanDirection::Up })
	{
		if (!TestTrue(TEXT("执行连续操作"), IsSuccessful(Session->RequestMove(Direction)))) { return false; }
		Snapshots.Add(Session->GetState());
	}
	TestEqual(TEXT("四次行动进入撤销栈"), Session->GetUndoCount(), 4);
	TestEqual(TEXT("推动次数准确"), Session->GetState().Counters.PushCount, 2);
	for (int32 Index = 3; Index >= 0; --Index)
	{
		TestTrue(TEXT("逐步撤销成功"), Session->Undo());
		TestTrue(TEXT("撤销恢复对应快照"), StatesEqual(Session->GetState(), Snapshots[Index]));
		TestEqual(TEXT("撤销栈同步减少"), Session->GetUndoCount(), Index);
		TestEqual(TEXT("重做栈同步增加"), Session->GetRedoCount(), 4 - Index);
	}
	TestFalse(TEXT("撤销栈空后拒绝继续撤销"), Session->Undo());
	for (int32 Index = 1; Index <= 4; ++Index)
	{
		TestTrue(TEXT("逐步重做成功"), Session->Redo());
		TestTrue(TEXT("重做恢复对应快照"), StatesEqual(Session->GetState(), Snapshots[Index]));
		TestEqual(TEXT("重做栈同步减少"), Session->GetRedoCount(), 4 - Index);
	}
	TestFalse(TEXT("重做栈空后拒绝继续重做"), Session->Redo());
	TestTrue(TEXT("额外失败操作不改变最终状态"), StatesEqual(Session->GetState(), Snapshots.Last()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanSessionBranchTest,
	"Sokoban.Session.BranchesAndFailedInputs", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanSessionBranchTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<USokobanSession> Session(NewObject<USokobanSession>());
	TArray<FSokobanValidationIssue> Issues;
	Session->Initialize(MakeLevel(), Issues);
	Session->RequestMove(ESokobanDirection::Right);
	Session->RequestMove(ESokobanDirection::Down);
	Session->RequestMove(ESokobanDirection::Right);
	TestTrue(TEXT("撤销最后一步建立重做分支"), Session->Undo());
	const FSokobanBoardState Before = Session->GetState();
	TestTrue(TEXT("无效方向被拒绝"), Session->RequestMove(ESokobanDirection::None).Outcome == ESokobanMoveOutcome::InvalidDirection);
	TestTrue(TEXT("撞墙输入被拒绝"), Session->RequestMove(ESokobanDirection::Down).Outcome == ESokobanMoveOutcome::BlockedByTerrain);
	TestTrue(TEXT("失败输入不修改状态"), StatesEqual(Before, Session->GetState()));
	TestEqual(TEXT("失败输入不修改撤销栈"), Session->GetUndoCount(), 2);
	TestEqual(TEXT("失败输入保留重做分支"), Session->GetRedoCount(), 1);
	TestTrue(TEXT("另一个方向建立新分支"), IsSuccessful(Session->RequestMove(ESokobanDirection::Up)));
	TestEqual(TEXT("新行动加入撤销栈"), Session->GetUndoCount(), 3);
	TestEqual(TEXT("成功的新行动清除旧重做分支"), Session->GetRedoCount(), 0);
	TestFalse(TEXT("旧分支不能再重做"), Session->Redo());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanSessionRestartTest,
	"Sokoban.Session.RestartClearsHistory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanSessionRestartTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<USokobanSession> Session(NewObject<USokobanSession>());
	TArray<FSokobanValidationIssue> Issues;
	Session->Initialize(MakeLevel(), Issues);
	const FSokobanBoardState Initial = Session->GetState();
	Session->RequestMove(ESokobanDirection::Right);
	Session->RequestMove(ESokobanDirection::Down);
	Session->Undo();
	TestTrue(TEXT("重开前两栈都有内容"), Session->CanUndo() && Session->CanRedo());
	TestTrue(TEXT("重开成功"), Session->Restart());
	TestTrue(TEXT("重开恢复初始棋盘与零计数"), StatesEqual(Initial, Session->GetState()));
	TestFalse(TEXT("重开清空撤销栈"), Session->CanUndo());
	TestFalse(TEXT("重开清空重做栈"), Session->CanRedo());
	TestTrue(TEXT("连续重开也合法"), Session->Restart());
	TestTrue(TEXT("重开之后仍可正常行动"), IsSuccessful(Session->RequestMove(ESokobanDirection::Right)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanSessionReinitializeTest,
	"Sokoban.Session.ReinitializationIsAtomic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanSessionReinitializeTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<USokobanSession> Session(NewObject<USokobanSession>());
	TArray<FSokobanValidationIssue> Issues;
	Session->Initialize(MakeLevel(), Issues);
	Session->RequestMove(ESokobanDirection::Right);
	Session->RequestMove(ESokobanDirection::Down);
	Session->Undo();
	const FSokobanBoardState Before = Session->GetState();
	TestFalse(TEXT("非法新关卡初始化失败"), Session->Initialize(FSokobanLevelDefinition(), Issues));
	TestTrue(TEXT("失败初始化保留原来的有效对局"), Session->IsInitialized());
	TestTrue(TEXT("失败初始化不改变棋盘"), StatesEqual(Before, Session->GetState()));
	TestEqual(TEXT("失败初始化保留撤销栈"), Session->GetUndoCount(), 1);
	TestEqual(TEXT("失败初始化保留重做栈"), Session->GetRedoCount(), 1);
	TestTrue(TEXT("失败后还能重做旧对局的操作"), Session->Redo());

	FSokobanLevelDefinition NewLevel = MakeLevel();
	NewLevel.PlayerSpawn = FIntPoint(0, 0);
	NewLevel.Boxes[0].BoxId = 42;
	NewLevel.Boxes[0].Position = FIntPoint(4, 3);
	NewLevel.Goals[0] = FIntPoint(5, 4);
	TestTrue(TEXT("合法新关卡替换成功"), Session->Initialize(NewLevel, Issues));
	TestTrue(TEXT("采用新出生点"), Session->GetState().PlayerPosition == NewLevel.PlayerSpawn);
	TestTrue(TEXT("采用新箱子 ID"), Session->GetState().BoxPositions.Contains(42));
	TestFalse(TEXT("没有残留旧箱子"), Session->GetState().BoxPositions.Contains(7));
	TestEqual(TEXT("新关卡清空撤销历史"), Session->GetUndoCount(), 0);
	TestEqual(TEXT("新关卡清空重做历史"), Session->GetRedoCount(), 0);
	TestFalse(TEXT("新关卡不能撤销旧关卡的操作"), Session->Undo());
	TestTrue(TEXT("新关卡可移动"), IsSuccessful(Session->RequestMove(ESokobanDirection::Down)));
	TestTrue(TEXT("重开使用新关卡初始快照"), Session->Restart());
	TestTrue(TEXT("新初始快照正确"), Session->GetState().PlayerPosition == NewLevel.PlayerSpawn);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanSessionCompletionTest,
	"Sokoban.Session.CompletionTransitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanSessionCompletionTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<USokobanSession> Session(NewObject<USokobanSession>());
	TArray<FSokobanSessionUpdate> Updates;
	const FDelegateHandle Handle = Session->OnSessionChangedNative().AddLambda([&Updates](const FSokobanSessionUpdate& Update) { Updates.Add(Update); });
	FSokobanLevelDefinition Level = MakeLevel();
	Level.Goals[0] = FIntPoint(3, 2);
	TArray<FSokobanValidationIssue> Issues;
	Session->Initialize(Level, Issues);
	TestFalse(TEXT("初始没有通关状态变化"), Updates.Last().bSolvedChanged);
	Session->RequestMove(ESokobanDirection::Right);
	TestTrue(TEXT("推动到位后缓存通关状态"), Session->IsSolved());
	TestTrue(TEXT("通知报告进入通关状态"), Updates.Last().bSolvedChanged && Updates.Last().bIsSolved);
	const FSokobanBoardState Solved = Session->GetState();
	TestTrue(TEXT("通关后普通移动被锁定"), Session->RequestMove(ESokobanDirection::Right).Outcome == ESokobanMoveOutcome::InvalidState);
	TestTrue(TEXT("通关锁定不改变棋盘"), StatesEqual(Solved, Session->GetState()));
	TestEqual(TEXT("失败行动不发事件"), Updates.Num(), 2);
	TestTrue(TEXT("通关后仍可撤销"), Session->Undo());
	TestTrue(TEXT("撤销通知离开通关状态"), Updates.Last().bSolvedChanged && !Updates.Last().bIsSolved);
	TestTrue(TEXT("可以重做最后一步"), Session->Redo());
	TestTrue(TEXT("重做再次通关"), Session->IsSolved());
	TestTrue(TEXT("重做通知再次进入通关状态"), Updates.Last().bSolvedChanged && Updates.Last().bIsSolved);
	TestTrue(TEXT("通关后仍可重开"), Session->Restart());
	TestTrue(TEXT("重开通知离开通关状态"), Updates.Last().bSolvedChanged && !Updates.Last().bIsSolved);
	Level.Boxes[0].Position = Level.Goals[0];
	TestTrue(TEXT("允许初始化所有箱子已到位的合法关卡"), Session->Initialize(Level, Issues));
	TestTrue(TEXT("初始到位也会判断通关"), Session->IsSolved());
	TestTrue(TEXT("初始到位通知通关变化"), Updates.Last().bSolvedChanged);
	Session->Restart();
	TestFalse(TEXT("重开后仍通关不重复报告状态变化"), Updates.Last().bSolvedChanged);
	Session->OnSessionChangedNative().Remove(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanSessionNotificationsTest,
	"Sokoban.Session.NotificationConsistency", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanSessionNotificationsTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<USokobanSession> Session(NewObject<USokobanSession>());
	TArray<FSokobanSessionUpdate> Updates;
	const FDelegateHandle Handle = Session->OnSessionChangedNative().AddLambda([this, &Session, &Updates](const FSokobanSessionUpdate& Update)
	{
		Updates.Add(Update);
		TestTrue(TEXT("回调时已初始化"), Session->IsInitialized());
		TestEqual(TEXT("回调时计数和撤销栈已同步"), Session->GetState().Counters.MoveCount, Session->GetUndoCount());
		TestTrue(TEXT("回调时通关缓存已更新"), Session->IsSolved() == Update.bIsSolved);
		TestTrue(TEXT("回调时可以查询撤销可用性"), Session->CanUndo() == (Session->GetUndoCount() > 0));
		TestTrue(TEXT("回调时可以查询重做可用性"), Session->CanRedo() == (Session->GetRedoCount() > 0));
	});
	TArray<FSokobanValidationIssue> Issues;
	Session->Initialize(MakeLevel(), Issues);
	const FSokobanMoveResult Result = Session->RequestMove(ESokobanDirection::Right);
	Session->Undo();
	Session->Redo();
	Session->Restart();
	const ESokobanSessionChange Expected[] = { ESokobanSessionChange::Initialized, ESokobanSessionChange::Move, ESokobanSessionChange::Undo, ESokobanSessionChange::Redo, ESokobanSessionChange::Restart };
	TestEqual(TEXT("每个成功操作恰好通知一次"), Updates.Num(), 5);
	for (int32 Index = 0; Index < Updates.Num() && Index < 5; ++Index)
	{
		TestTrue(TEXT("通知操作类型正确"), Updates[Index].Change == Expected[Index]);
		if (Index >= 1 && Index <= 3)
		{
			TestTrue(TEXT("行动通知保留原记录方向，撤销由接收方反向播放"), Updates[Index].Record.PlayerFrom == Result.Record.PlayerFrom && Updates[Index].Record.PlayerTo == Result.Record.PlayerTo);
		}
		else
		{
			TestTrue(TEXT("初始化与重开不携带单步记录"), Updates[Index].Record.PlayerFrom == FIntPoint(-1, -1));
		}
	}
	Session->Undo();
	Session->Redo();
	Session->RequestMove(ESokobanDirection::None);
	Session->Initialize(FSokobanLevelDefinition(), Issues);
	TestEqual(TEXT("失败操作不发送通知"), Updates.Num(), 5);
	Session->OnSessionChangedNative().Remove(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanSessionReentrancyTest,
	"Sokoban.Session.ReentrantMutationsRejected", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanSessionReentrancyTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<USokobanSession> Session(NewObject<USokobanSession>());
	const FSokobanLevelDefinition Level = MakeLevel();
	int32 NotificationCount = 0;
	const FDelegateHandle Handle = Session->OnSessionChangedNative().AddLambda([this, &Session, &Level, &NotificationCount](const FSokobanSessionUpdate& Update)
	{
		++NotificationCount;
		const FSokobanBoardState Before = Session->GetState();
		const int32 UndoCount = Session->GetUndoCount();
		const int32 RedoCount = Session->GetRedoCount();
		TestTrue(TEXT("回调中拒绝嵌套移动"), Session->RequestMove(ESokobanDirection::Down).Outcome == ESokobanMoveOutcome::InvalidState);
		TestFalse(TEXT("回调中拒绝嵌套撤销"), Session->Undo());
		TestFalse(TEXT("回调中拒绝嵌套重做"), Session->Redo());
		TestFalse(TEXT("回调中拒绝嵌套重开"), Session->Restart());
		TArray<FSokobanValidationIssue> Issues;
		TestFalse(TEXT("回调中拒绝切换关卡"), Session->Initialize(Level, Issues));
		TestTrue(TEXT("嵌套初始化给出明确原因"), Issues.Num() == 1 && Issues[0].Code == FName(TEXT("SessionBusy")));
		TestTrue(TEXT("回调期间棋盘稳定"), StatesEqual(Before, Session->GetState()));
		TestEqual(TEXT("回调期间撤销栈稳定"), Session->GetUndoCount(), UndoCount);
		TestEqual(TEXT("回调期间重做栈稳定"), Session->GetRedoCount(), RedoCount);
	});
	TArray<FSokobanValidationIssue> Issues;
	TestTrue(TEXT("外部初始化成功"), Session->Initialize(Level, Issues));
	TestTrue(TEXT("通知结束后能正常行动"), IsSuccessful(Session->RequestMove(ESokobanDirection::Right)));
	TestTrue(TEXT("通知结束后能正常撤销"), Session->Undo());
	TestTrue(TEXT("通知结束后能正常重做"), Session->Redo());
	TestTrue(TEXT("通知结束后能正常重开"), Session->Restart());
	TestEqual(TEXT("嵌套请求未造成额外通知"), NotificationCount, 5);
	Session->OnSessionChangedNative().Remove(Handle);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
