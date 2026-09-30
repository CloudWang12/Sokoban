#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sokoban/Data/SokobanTypes.h"
#include "SokobanBoardActor.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/** 棋盘表现：只显示 Session 的结果，不使用物理碰撞判断推动规则。 */
UCLASS(Blueprintable)
class SOKOBAN_API ASokobanBoardActor : public AActor
{
	GENERATED_BODY()

public:
	ASokobanBoardActor();
	virtual void Tick(float DeltaSeconds) override;

	bool BuildBoard(const FSokobanLevelDefinition& Level, const FSokobanBoardState& State);
	bool SynchronizeState(const FSokobanBoardState& State);
	bool PresentMove(const FSokobanMoveRecord& Record, bool bReverse, const FSokobanBoardState& FinalState);

	UFUNCTION(BlueprintPure, Category = "Sokoban|Board")
	bool IsAnimating() const { return bAnimating; }

	/** 网格向右对应局部 +Y，网格向上对应局部 +X，与默认俯视镜头方向一致。 */
	UFUNCTION(BlueprintPure, Category = "Sokoban|Board")
	FVector GridToWorld(FIntPoint Cell, float Height = 0.0f) const;

	FBox GetBoardBounds() const;
	FVector GetPlayerVisualLocation() const;
	bool GetBoxVisualLocation(int32 BoxId, FVector& OutLocation) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sokoban|Geometry", meta = (ClampMin = "10.0"))
	float CellSize = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sokoban|Animation", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MoveDuration = 0.14f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sokoban|Materials")
	TObjectPtr<UMaterialInterface> FloorMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sokoban|Materials")
	TObjectPtr<UMaterialInterface> WallMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sokoban|Materials")
	TObjectPtr<UMaterialInterface> GoalMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sokoban|Materials")
	TObjectPtr<UMaterialInterface> BoxMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sokoban|Materials")
	TObjectPtr<UMaterialInterface> BoxOnGoalMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sokoban|Materials")
	TObjectPtr<UMaterialInterface> PlayerMaterial;

	/** 未指定对应材质时使用这些颜色；自定义材质优先，不会被强行改色。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sokoban|Colors")
	FLinearColor FloorColor = FLinearColor(0.18f, 0.21f, 0.27f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sokoban|Colors")
	FLinearColor WallColor = FLinearColor(0.035f, 0.055f, 0.09f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sokoban|Colors")
	FLinearColor GoalColor = FLinearColor(1.0f, 0.65f, 0.025f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sokoban|Colors")
	FLinearColor BoxColor = FLinearColor(0.72f, 0.23f, 0.045f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sokoban|Colors")
	FLinearColor BoxOnGoalColor = FLinearColor(0.06f, 0.65f, 0.18f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sokoban|Colors")
	FLinearColor PlayerColor = FLinearColor(0.035f, 0.4f, 1.0f);

private:
	FVector GridToLocal(FIntPoint Cell, float Height) const;
	bool CanDisplayState(const FSokobanLevelDefinition& Level, const FSokobanBoardState& State) const;
	void ClearBoard();
	void UpdateBoxMaterials(const FSokobanBoardState& State);
	UMaterialInterface* ResolveMaterial(UMaterialInterface* Override, const FLinearColor& Color, float EmissiveStrength = 0.0f);

	UPROPERTY(VisibleAnywhere, Category = "Sokoban|Components")
	TObjectPtr<USceneComponent> BoardRoot;
	UPROPERTY(VisibleAnywhere, Category = "Sokoban|Components")
	TObjectPtr<UInstancedStaticMeshComponent> Floors;
	UPROPERTY(VisibleAnywhere, Category = "Sokoban|Components")
	TObjectPtr<UInstancedStaticMeshComponent> Walls;
	UPROPERTY(VisibleAnywhere, Category = "Sokoban|Components")
	TObjectPtr<UInstancedStaticMeshComponent> Goals;
	UPROPERTY(VisibleAnywhere, Category = "Sokoban|Components")
	TObjectPtr<UStaticMeshComponent> PlayerVisual;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> DefaultMaterial;
	/** 两种箱子材质均需被持有；切换状态时不创建新实例，也不会丢失未使用的一种。 */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ResolvedBoxMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ResolvedBoxOnGoalMaterial;
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UStaticMeshComponent>> BoxVisuals;
	UPROPERTY(Transient)
	FSokobanLevelDefinition Definition;
	UPROPERTY(Transient)
	FSokobanBoardState PendingState;

	bool bHasBoard = false;
	bool bAnimating = false;
	float AnimationElapsed = 0.0f;
	float ActiveDuration = 0.0f;
	int32 MovingBoxId = INDEX_NONE;
	FVector PlayerStart;
	FVector PlayerEnd;
	FVector BoxStart;
	FVector BoxEnd;
	static constexpr float FloorThickness = 12.0f;
	static constexpr float WallHeight = 100.0f;
	static constexpr float BoxHeight = 90.0f;
	static constexpr float PlayerHeight = 80.0f;
};
