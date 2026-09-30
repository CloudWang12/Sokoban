#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Sokoban/Data/SokobanTypes.h"
#include "SokobanFlowSubsystem.generated.h"

class ASokobanGameMode;
class USokobanLevelCatalog;
class USokobanProgressSubsystem;

UENUM(BlueprintType)
enum class ESokobanFlowState : uint8 { MainMenu, LevelSelect, Playing, Results };

USTRUCT(BlueprintType)
struct SOKOBAN_API FSokobanCompletion
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName LevelId;
	UPROPERTY(BlueprintReadOnly) FText DisplayName;
	UPROPERTY(BlueprintReadOnly) int32 ContentRevision = 1;
	UPROPERTY(BlueprintReadOnly) FSokobanMoveCounters Counters;
	UPROPERTY(BlueprintReadOnly) bool bNewBest = false;
};

/** 跨世界保留的流程协调器。仅弱引用 GameMode，离开世界必须 Detach。 */
UCLASS()
class SOKOBAN_API USokobanFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	void Attach(ASokobanGameMode* Mode, USokobanLevelCatalog* InCatalog);
	void Detach(ASokobanGameMode* Mode);
	UFUNCTION(BlueprintCallable, Category="Sokoban|Flow") bool ShowMainMenu();
	UFUNCTION(BlueprintCallable, Category="Sokoban|Flow") bool ShowLevelSelect();
	UFUNCTION(BlueprintCallable, Category="Sokoban|Flow") bool StartLevel(int32 Index);
	UFUNCTION(BlueprintCallable, Category="Sokoban|Flow") bool Replay();
	UFUNCTION(BlueprintCallable, Category="Sokoban|Flow") bool NextLevel();
	/** GameMode 在动画完成后调用；同一局最多提交一次成绩。 */
	void TryComplete();
	UFUNCTION(BlueprintPure, Category="Sokoban|Flow") ESokobanFlowState GetState() const { return State; }
	UFUNCTION(BlueprintPure, Category="Sokoban|Flow") USokobanLevelCatalog* GetCatalog() const { return Catalog; }
	UFUNCTION(BlueprintPure, Category="Sokoban|Flow") int32 GetCurrentIndex() const { return CurrentIndex; }
	UFUNCTION(BlueprintPure, Category="Sokoban|Flow") FSokobanCompletion GetCompletion() const { return Completion; }
	UFUNCTION(BlueprintPure, Category="Sokoban|Flow") FText GetStatus() const { return Status; }
	bool IsAttachedTo(const ASokobanGameMode* Mode) const;
	bool CanAcceptGameplay(const ASokobanGameMode* Mode) const;
	bool IsStartingLevel() const { return bStartingLevel; }
	UFUNCTION(BlueprintPure, Category="Sokoban|Flow") bool HasNextLevel() const;
	UFUNCTION(BlueprintPure, Category="Sokoban|Flow") USokobanProgressSubsystem* GetProgress() const;
	DECLARE_EVENT(USokobanFlowSubsystem, FChanged);
	FChanged& OnChanged() { return Changed; }

private:
	bool ShowPage(ESokobanFlowState Page);
	bool CheckCatalog();
	void SetBoardVisible(bool bVisible);
	UPROPERTY(Transient) TObjectPtr<USokobanLevelCatalog> Catalog;
	TWeakObjectPtr<ASokobanGameMode> GameMode;
	ESokobanFlowState State = ESokobanFlowState::MainMenu;
	int32 CurrentIndex = INDEX_NONE;
	FSokobanCompletion Completion; // 开始时冻结 ID、修订号和名称，结束时填写计数。
	FText Status;
	bool bTransition = false;
	bool bStartingLevel = false;
	bool bCompletionHandled = false;
	FChanged Changed;
};
