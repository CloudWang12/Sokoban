#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SokobanSaveGame.h"
#include "SokobanProgressSubsystem.generated.h"

/** 单机小型存档同步保存。失败保留内存数据；读取异常时禁止自动覆盖原文件。 */
UCLASS()
class SOKOBAN_API USokobanProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UFUNCTION(BlueprintPure, Category="Sokoban|Progress") bool GetProgress(FName Id, int32 Revision, FSokobanLevelProgress& Out) const;
	UFUNCTION(BlueprintPure, Category="Sokoban|Progress") FName GetLastPlayedLevel() const;
	void SetLastPlayedLevel(FName Id);
	bool RecordCompletion(FName Id, int32 Revision, const FSokobanMoveCounters& Counters, bool& bOutNewBest);
	UFUNCTION(BlueprintCallable, Category="Sokoban|Progress") bool Save();
	/** 仅在用户明确确认后调用：先备份异常原文件，再保存本次内存进度。 */
	UFUNCTION(BlueprintCallable, Category="Sokoban|Progress") bool RecoverSaving();
	UFUNCTION(BlueprintPure, Category="Sokoban|Progress") bool IsWriteBlocked() const { return bWriteBlocked; }
	UFUNCTION(BlueprintPure, Category="Sokoban|Progress") bool HasUnsavedChanges() const { return bDirty; }
	UFUNCTION(BlueprintPure, Category="Sokoban|Progress") FText GetStatus() const { return Status; }
#if WITH_DEV_AUTOMATION_TESTS
	/** 自动化只允许专用槽位，防止测试改写玩家存档。 */
	void SetAutomationSlot(const FString& InSlot);
#endif

private:
	UPROPERTY(Transient) TObjectPtr<USokobanSaveGame> Data;
	bool bWriteBlocked = false;
	bool bDirty = false;
	FText Status;
	FString SlotName = TEXT("SokobanProgress");
	void Load();
};
