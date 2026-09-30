#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SokobanSession.h"
#include "SokobanGameMode.generated.h"

class USokobanLevelData;
class USokobanLevelCatalog;
class USokobanFlowSubsystem;
class ASokobanBoardActor;

/** 连接输入、Session 和棋盘表现。玩家操作统一经此入口，避免绕过动画锁。 */
UCLASS(Blueprintable)
class SOKOBAN_API ASokobanGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASokobanGameMode();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	bool CanAcceptPlayerInput() const;
	bool IsFlowManaged() const { return bFlowAttached; }

	/** 正式流程默认开启；关闭后保留原有单关调试入口。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sokoban|Flow")
	bool bEnableFrontend = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sokoban|Flow")
	TObjectPtr<USokobanLevelCatalog> LevelCatalog;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sokoban|Level")
	TObjectPtr<USokobanLevelData> StartupLevel;

	/** 尚未制作关卡资产时，用内置的两箱示例完成首次试玩。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sokoban|Level")
	bool bUseDemoLevelIfUnset = true;

	/** 地图中已有一个 BoardActor 时直接使用；没有时才生成此类。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sokoban|Board")
	TSubclassOf<ASokobanBoardActor> BoardClass;

	UFUNCTION(BlueprintCallable, Category = "Sokoban|Game")
	bool InitializeLevel(const FSokobanLevelDefinition& Level);
	UFUNCTION(BlueprintCallable, Category = "Sokoban|Game")
	FSokobanMoveResult RequestMove(ESokobanDirection Direction);
	UFUNCTION(BlueprintCallable, Category = "Sokoban|Game")
	bool RequestUndo();
	UFUNCTION(BlueprintCallable, Category = "Sokoban|Game")
	bool RequestRedo();
	UFUNCTION(BlueprintCallable, Category = "Sokoban|Game")
	bool RequestRestart();
	UFUNCTION(BlueprintPure, Category = "Sokoban|Game")
	bool IsReady() const { return bReady; }
	UFUNCTION(BlueprintPure, Category = "Sokoban|Game")
	bool IsPresentationBusy() const;
	UFUNCTION(BlueprintPure, Category = "Sokoban|Game")
	bool IsCompletionVisible() const;
	UFUNCTION(BlueprintPure, Category = "Sokoban|Game")
	FText GetStatusMessage() const;
	UFUNCTION(BlueprintPure, Category = "Sokoban|Game")
	USokobanSession* GetSession() const { return Session; }
	UFUNCTION(BlueprintPure, Category = "Sokoban|Game")
	ASokobanBoardActor* GetBoard() const { return Board; }

	static FSokobanLevelDefinition MakeDemoLevel();

private:
	bool EnsureBoard();
	void HandleSessionChanged(const FSokobanSessionUpdate& Update);
	void SetFailure(const FText& Message);

	UPROPERTY(Transient)
	TObjectPtr<USokobanSession> Session;
	UPROPERTY(Transient)
	TObjectPtr<ASokobanBoardActor> Board;
	FDelegateHandle SessionHandle;
	FText StatusMessage;
	bool bReady = false;
	bool bFlowAttached = false;
	TWeakObjectPtr<USokobanFlowSubsystem> Flow;
};
