#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "Sokoban/Data/SokobanTypes.h"
#include "SokobanPlayerController.generated.h"

class UInputAction;
class USokobanUIManagerComponent;
class UInputMappingContext;

/** Enhanced Input 只负责把操作转换成请求，实际规则与动画锁由 GameMode / Session 处理。 */
UCLASS(Blueprintable)
class SOKOBAN_API ASokobanPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASokobanPlayerController();
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	void ResetDirectionalInput();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sokoban|UI")
	TObjectPtr<USokobanUIManagerComponent> UIManager;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sokoban|Input")
	TObjectPtr<UInputMappingContext> InputMappingContext;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sokoban|Input")
	TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sokoban|Input")
	TObjectPtr<UInputAction> UndoAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sokoban|Input")
	TObjectPtr<UInputAction> RedoAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sokoban|Input")
	TObjectPtr<UInputAction> RestartAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sokoban|Input", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float InputDeadZone = 0.5f;

	UFUNCTION(BlueprintPure, Category = "Sokoban|Input")
	FText GetInputConfigurationError() const { return InputConfigurationError; }

	/** Axis.X 右为正、Axis.Y 上为正；斜向取绝对值较大的轴，相等时优先水平。 */
	static ESokobanDirection ResolveGridDirection(FVector2D Axis, float DeadZone = 0.5f);
	bool ValidateInputConfiguration(FText& OutError) const;

private:
	void OnMoveTriggered(const FInputActionValue& Value);
	void OnMoveReleased(const FInputActionValue& Value);
	void OnUndoStarted();
	void OnRedoStarted();
	void OnRestartStarted();
	FText InputConfigurationError;
	ESokobanDirection HeldDirection = ESokobanDirection::None;
	bool bAddedMappingContext = false;
};
