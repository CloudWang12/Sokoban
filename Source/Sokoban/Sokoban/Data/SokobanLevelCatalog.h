#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SokobanLevelData.h"
#include "SokobanLevelCatalog.generated.h"

/** 显式关卡目录。顺序决定选关展示和下一关；全部关卡始终开放。硬引用保证烘焙可达。 */
UCLASS(BlueprintType)
class SOKOBAN_API USokobanLevelCatalog : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sokoban")
	TArray<TObjectPtr<USokobanLevelData>> Levels;
	UFUNCTION(BlueprintPure, Category="Sokoban")
	USokobanLevelData* GetLevel(int32 Index) const;
	UFUNCTION(BlueprintPure, Category="Sokoban")
	int32 FindLevelIndex(FName LevelId) const;
	UFUNCTION(BlueprintPure, Category="Sokoban")
	int32 GetNextIndex(int32 Index) const;
	/** 检查目录身份与引用；布局是否合法在开始该关时另行检查。 */
	bool ValidateCatalog(TArray<FText>& OutErrors) const;
};
