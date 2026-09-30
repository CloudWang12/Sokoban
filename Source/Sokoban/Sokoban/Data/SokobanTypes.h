#pragma once

#include "CoreMinimal.h"
#include "SokobanTypes.generated.h"

/** 仅描述静态地形；目标点和格子上的占用物分别保存。 */
UENUM(BlueprintType)
enum class ESokobanTerrain : uint8
{
	Void,
	Floor,
	Wall
};

/** 网格坐标约定：X 向右增加，Y 向下增加；向上对应 (0, -1)。 */
UENUM(BlueprintType)
enum class ESokobanDirection : uint8
{
	None,
	Up,
	Right,
	Down,
	Left
};

/** None 表示尚未初始化的结果，不代表移动成功。 */
UENUM(BlueprintType)
enum class ESokobanMoveOutcome : uint8
{
	None,
	Walk,
	Push,
	InvalidDirection,
	OutOfBounds,
	BlockedByTerrain,
	BlockedByBox,
	InvalidState
};

UENUM(BlueprintType)
enum class ESokobanValidationSeverity : uint8
{
	Warning,
	Error
};

/** 单个箱子的初始数据。箱子 ID 必须为非负数，且在关卡内唯一。 */
USTRUCT(BlueprintType)
struct SOKOBAN_API FSokobanBoxDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sokoban", meta = (ClampMin = "0"))
	int32 BoxId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sokoban")
	FIntPoint Position = FIntPoint(-1, -1);
};

/**
 * 关卡初始布局，在游玩过程中保持不变；默认的空布局表示尚未完成的草稿。
 * 地形按行存储：Index = Y * Width + X，数组长度必须等于 Width * Height。
 * 目标点、出生点和箱子必须位于地板格上；箱子初始位置可以与目标点重合。
 * 属性元数据中的范围限制仅用于辅助编辑，不能替代加载数据时的校验。
 */
USTRUCT(BlueprintType)
struct SOKOBAN_API FSokobanLevelDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sokoban", meta = (ClampMin = "1"))
	int32 Width = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sokoban", meta = (ClampMin = "1"))
	int32 Height = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sokoban")
	TArray<ESokobanTerrain> Terrain;

	/** 目标点坐标必须唯一；使用数组保存，以便校验时发现并报告重复坐标。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sokoban")
	TArray<FIntPoint> Goals;

	/** 仅保存一个玩家出生点；(-1, -1) 表示尚未放置。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sokoban")
	FIntPoint PlayerSpawn = FIntPoint(-1, -1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sokoban", meta = (TitleProperty = "BoxId"))
	TArray<FSokobanBoxDefinition> Boxes;
};

/** 一次推动同时计为一次移动和一次推动；无效输入不增加任何计数。 */
USTRUCT(BlueprintType)
struct SOKOBAN_API FSokobanMoveCounters
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Sokoban")
	int32 MoveCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Sokoban")
	int32 PushCount = 0;
};

/** 游玩过程中可变化的逻辑状态；不保存 Actor 变换或格子占用缓存。 */
USTRUCT(BlueprintType)
struct SOKOBAN_API FSokobanBoardState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Sokoban")
	FIntPoint PlayerPosition = FIntPoint(-1, -1);

	/** 稳定的箱子 ID 映射到当前格子坐标；映射表的遍历顺序不影响玩法规则。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Sokoban")
	TMap<int32, FIntPoint> BoxPositions;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Sokoban")
	FSokobanMoveCounters Counters;
};

/**
 * 一次不可拆分的行动记录，包含可能发生的推动，以及操作前后的计数。
 * BoxId == INDEX_NONE 表示普通移动，此时不使用 BoxFrom 和 BoxTo。
 * 移动方向由 PlayerTo - PlayerFrom 推导，不重复保存。
 * 执行或撤销操作前，必须检查记录是否与当前棋盘状态匹配。
 */
USTRUCT(BlueprintType)
struct SOKOBAN_API FSokobanMoveRecord
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Sokoban")
	FIntPoint PlayerFrom = FIntPoint(-1, -1);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Sokoban")
	FIntPoint PlayerTo = FIntPoint(-1, -1);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Sokoban")
	int32 BoxId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Sokoban")
	FIntPoint BoxFrom = FIntPoint(-1, -1);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Sokoban")
	FIntPoint BoxTo = FIntPoint(-1, -1);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Sokoban")
	FSokobanMoveCounters CountersBefore;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Sokoban")
	FSokobanMoveCounters CountersAfter;
};

/** 仅当 Outcome 为 Walk 或 Push 时，Record 才表示有效的行动记录。 */
USTRUCT(BlueprintType)
struct SOKOBAN_API FSokobanMoveResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sokoban")
	ESokobanMoveOutcome Outcome = ESokobanMoveOutcome::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sokoban")
	FSokobanMoveRecord Record;
};

/** 供后续校验器和关卡编辑器共用的诊断数据。 */
USTRUCT(BlueprintType)
struct SOKOBAN_API FSokobanValidationIssue
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sokoban")
	ESokobanValidationSeverity Severity = ESokobanValidationSeverity::Error;

	/** 供程序判断的稳定问题标识；不要根据本地化后的 Message 文本编写分支逻辑。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sokoban")
	FName Code = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sokoban")
	FText Message;

	/** 关卡整体问题对应空数组；格子相关问题则记录坐标，用于高亮受影响的格子。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sokoban")
	TArray<FIntPoint> Cells;
};
