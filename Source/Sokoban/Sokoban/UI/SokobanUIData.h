#pragma once

#include "CoreMinimal.h"
#include "Sokoban/Flow/SokobanFlowSubsystem.h"
#include "SokobanUIData.generated.h"

/** 选关列表的一行。只是数据，条目控件的创建、排序展示和样式由 WBP 决定。 */
USTRUCT(BlueprintType)
struct SOKOBAN_API FSokobanLevelListEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") int32 Index = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") FName LevelId;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") FText DisplayName;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") int32 ContentRevision = 1;
	/** 目录检查通过才允许尝试开始；布局仍由 Flow 在开始时校验。 */
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") bool bCanStart = false;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") bool bCompleted = false;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") FSokobanMoveCounters Best;
};

/** 界面只读快照。蓝图自行把数字、状态转换成文本、显隐、动画等表现。 */
USTRUCT(BlueprintType)
struct SOKOBAN_API FSokobanUIViewData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") ESokobanFlowState State = ESokobanFlowState::MainMenu;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") bool bAvailable = false;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") int32 CurrentIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") int32 LastPlayedIndex = INDEX_NONE;
	/** 开始时冻结身份；Results 时包含正式计数和新纪录标志。 */
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") FSokobanCompletion Completion;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") FSokobanMoveCounters Counters;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") int32 UndoCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") int32 RedoCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") bool bBusy = false;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") bool bCanUndo = false;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") bool bCanRedo = false;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") bool bCanRestart = false;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") bool bHasNextLevel = false;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") bool bHasBest = false;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") FSokobanMoveCounters Best;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") bool bUnsavedChanges = false;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") bool bWriteBlocked = false;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") FText Status;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") FText SaveStatus;
	UPROPERTY(BlueprintReadOnly, Category="Sokoban|UI") bool bConfirmationPending = false;
};
