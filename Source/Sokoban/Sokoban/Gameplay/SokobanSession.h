#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Sokoban/Data/SokobanTypes.h"
#include "SokobanSession.generated.h"

/** 告诉界面和表现层：此次数据变化来自哪一种操作。 */
UENUM(BlueprintType)
enum class ESokobanSessionChange : uint8
{
	None,
	Initialized,
	Move,
	Undo,
	Redo,
	Restart
};

/** 一次成功操作的通知；事件发出时，棋盘、历史和通关状态已全部更新。 */
USTRUCT(BlueprintType)
struct SOKOBAN_API FSokobanSessionUpdate
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sokoban")
	ESokobanSessionChange Change = ESokobanSessionChange::None;

	/** 仅在 Move / Undo / Redo 时有效；Undo 保留原记录，由表现层反向播放。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sokoban")
	FSokobanMoveRecord Record;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sokoban")
	bool bIsSolved = false;

	/** 与本次操作前相比，通关条件是否发生变化；初始化或重开也可能改变它。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sokoban")
	bool bSolvedChanged = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSokobanSessionChanged, const FSokobanSessionUpdate&, Update);

/**
 * 管理一局推箱子的状态和操作历史。所有操作同步执行，只应在游戏线程调用。
 * 拥有者使用 NewObject 创建，并用 UPROPERTY 持有；本类不创建 Actor，也不播放动画。
 */
UCLASS(BlueprintType)
class SOKOBAN_API USokobanSession : public UObject
{
	GENERATED_BODY()

public:
	/** 校验并复制关卡。失败会返回问题列表，并完整保留原来的对局与历史。 */
	UFUNCTION(BlueprintCallable, Category = "Sokoban|Session")
	bool Initialize(const FSokobanLevelDefinition& InLevel, TArray<FSokobanValidationIssue>& OutIssues);

	/** 未初始化、已通关或正在通知监听者时拒绝行动，返回 InvalidState。 */
	UFUNCTION(BlueprintCallable, Category = "Sokoban|Session")
	FSokobanMoveResult RequestMove(ESokobanDirection Direction);

	/** 撤销最近一步；成功后将记录转移到重做栈。 */
	UFUNCTION(BlueprintCallable, Category = "Sokoban|Session")
	bool Undo();

	/** 重做最近撤销的一步；成功后将记录转移回撤销栈。 */
	UFUNCTION(BlueprintCallable, Category = "Sokoban|Session")
	bool Redo();

	/** 恢复初始化时的棋盘，并清空两份历史。 */
	UFUNCTION(BlueprintCallable, Category = "Sokoban|Session")
	bool Restart();

	UFUNCTION(BlueprintPure, Category = "Sokoban|Session")
	bool IsInitialized() const { return bInitialized; }

	UFUNCTION(BlueprintPure, Category = "Sokoban|Session")
	bool IsSolved() const { return bInitialized && bSolved; }

	/** 表示历史是否可用；通知回调期间仍能查询，但不能立即嵌套执行操作。 */
	UFUNCTION(BlueprintPure, Category = "Sokoban|Session")
	bool CanUndo() const { return bInitialized && !UndoStack.IsEmpty(); }

	UFUNCTION(BlueprintPure, Category = "Sokoban|Session")
	bool CanRedo() const { return bInitialized && !RedoStack.IsEmpty(); }

	UFUNCTION(BlueprintPure, Category = "Sokoban|Session")
	int32 GetUndoCount() const { return UndoStack.Num(); }

	UFUNCTION(BlueprintPure, Category = "Sokoban|Session")
	int32 GetRedoCount() const { return RedoStack.Num(); }

	/** 返回副本，外部修改返回值不会绕过 Rules 改写当前棋盘。 */
	UFUNCTION(BlueprintPure, Category = "Sokoban|Session")
	FSokobanBoardState GetState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category = "Sokoban|Session")
	FSokobanLevelDefinition GetLevelDefinition() const { return LevelDefinition; }

	/** 蓝图通知：按 Change 区分全棋盘重置与单步变化，按 bSolvedChanged 更新结算状态。 */
	UPROPERTY(BlueprintAssignable, Category = "Sokoban|Session")
	FSokobanSessionChanged OnSessionChanged;

	/** 同一份通知的 C++ 入口，便于 C++ 控制器、表现层和测试绑定回调。 */
	DECLARE_EVENT_OneParam(USokobanSession, FSessionChangedNative, const FSokobanSessionUpdate&);
	FSessionChangedNative& OnSessionChangedNative() { return SessionChangedNative; }

private:
	/** 调用时状态和历史已经提交；重新计算通关状态后再广播。 */
	void NotifyChange(ESokobanSessionChange Change, const FSokobanMoveRecord& Record = FSokobanMoveRecord());

	// 这些属性是运行时数据，不允许在属性面板或蓝图中直接写入。
	UPROPERTY(Transient)
	FSokobanLevelDefinition LevelDefinition;

	UPROPERTY(Transient)
	FSokobanBoardState InitialState;

	UPROPERTY(Transient)
	FSokobanBoardState CurrentState;

	UPROPERTY(Transient)
	TArray<FSokobanMoveRecord> UndoStack;

	UPROPERTY(Transient)
	TArray<FSokobanMoveRecord> RedoStack;

	UPROPERTY(Transient)
	bool bInitialized = false;

	UPROPERTY(Transient)
	bool bSolved = false;

	// 保护一次同步操作直到所有通知完成，防止监听者嵌套修改同一局。
	bool bOperationInProgress = false;
	FSessionChangedNative SessionChangedNative;
};
