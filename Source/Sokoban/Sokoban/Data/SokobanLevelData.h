#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SokobanTypes.h"
#include "SokobanLevelData.generated.h"

/** Authoring asset. Copy Definition into a session; never mutate this asset during play. */
UCLASS(BlueprintType)
class SOKOBAN_API USokobanLevelData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Unique project-wide ID, independent of asset name/path. Set before using the level.
	 * Keep it stable after release; assign a new ID when duplicating a level as new content.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category = "Sokoban|Identity")
	FName LevelId = NAME_None;

	/** Serialized schema version, reserved for future migrations. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sokoban|Version")
	int32 DataVersion = 1;

	/** Increment when changing puzzle content to invalidate old solutions or resumed states. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sokoban|Version", meta = (ClampMin = "1"))
	int32 ContentRevision = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sokoban|Description")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sokoban|Board")
	FSokobanLevelDefinition Definition;

	/** Returns SokobanLevel:<LevelId>, or an invalid ID for an unconfigured draft/template. */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
