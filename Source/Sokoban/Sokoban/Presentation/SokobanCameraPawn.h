#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "SokobanCameraPawn.generated.h"

class UCameraComponent;
class ASokobanBoardActor;

/** 镜头 Pawn 与棋盘上的玩家标记分开，棋盘玩家的位置仍由 Session 控制。 */
UCLASS(Blueprintable)
class SOKOBAN_API ASokobanCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ASokobanCameraPawn();
	virtual void Tick(float DeltaSeconds) override;
	void SetBoard(ASokobanBoardActor* InBoard);
	void FrameBoard(float AspectRatio);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sokoban|Camera")
	TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sokoban|Camera", meta = (ClampMin = "30.0", ClampMax = "89.0"))
	float ViewPitch = 60.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sokoban|Camera", meta = (ClampMin = "1.0", ClampMax = "3.0"))
	float FrameMargin = 1.1f;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<ASokobanBoardActor> Board;
	FIntPoint LastViewportSize = FIntPoint::ZeroValue;
	FTransform LastBoardTransform;
};
