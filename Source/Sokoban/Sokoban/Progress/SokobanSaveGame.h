#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Sokoban/Data/SokobanTypes.h"
#include "SokobanSaveGame.generated.h"

/** 一个内容修订的完整最佳成绩；步数优先，推动数只用于同分比较。 */
USTRUCT(BlueprintType)
struct SOKOBAN_API FSokobanLevelProgress
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly) int32 ContentRevision = 1;
	UPROPERTY(SaveGame, BlueprintReadOnly) bool bCompleted = false;
	UPROPERTY(SaveGame, BlueprintReadOnly) FSokobanMoveCounters Best;
};

UCLASS()
class SOKOBAN_API USokobanSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) int32 SaveVersion = 1;
	UPROPERTY(SaveGame) FName LastPlayedLevel;
	UPROPERTY(SaveGame) TMap<FName, FSokobanLevelProgress> Levels;
	bool IsValidPayload() const;
	bool GetProgress(FName Id, int32 Revision, FSokobanLevelProgress& Out) const;
	bool RecordCompletion(FName Id, int32 Revision, const FSokobanMoveCounters& Counters, bool& bOutNewBest);
	static bool IsBetter(const FSokobanMoveCounters& Candidate, const FSokobanMoveCounters& Previous);
};
