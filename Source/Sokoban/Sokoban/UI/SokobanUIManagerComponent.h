#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sokoban/Flow/SokobanFlowSubsystem.h"
#include "SokobanUIManagerComponent.generated.h"

class USokobanScreen;

/** 仅负责页面生命周期与输入模式；所有界面布局由配置的 WBP 提供。 */
UCLASS(ClassGroup=(Sokoban), meta=(BlueprintSpawnableComponent))
class SOKOBAN_API USokobanUIManagerComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USokobanUIManagerComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UFUNCTION(BlueprintCallable, Category="Sokoban|UI") void Refresh();
	UFUNCTION(BlueprintPure, Category="Sokoban|UI") bool HasScreen() const { return Screen != nullptr; }
	UFUNCTION(BlueprintPure, Category="Sokoban|UI") USokobanScreen* GetScreen() const { return Screen; }
	UFUNCTION(BlueprintPure, Category="Sokoban|UI") FText GetConfigurationError() const { return ConfigurationError; }
	UPROPERTY(EditAnywhere, Category="Sokoban|UI") TSubclassOf<USokobanScreen> MainMenuClass;
	UPROPERTY(EditAnywhere, Category="Sokoban|UI") TSubclassOf<USokobanScreen> LevelSelectClass;
	UPROPERTY(EditAnywhere, Category="Sokoban|UI") TSubclassOf<USokobanScreen> PlayClass;
	UPROPERTY(EditAnywhere, Category="Sokoban|UI") TSubclassOf<USokobanScreen> ResultsClass;
private:
	UPROPERTY(Transient) TObjectPtr<USokobanScreen> Screen;
	UPROPERTY(Transient) FText ConfigurationError;
	TWeakObjectPtr<USokobanFlowSubsystem> Flow;
	FDelegateHandle ChangedHandle;
	ESokobanFlowState ScreenState = ESokobanFlowState::MainMenu;
	bool bRefreshing = false;
};
