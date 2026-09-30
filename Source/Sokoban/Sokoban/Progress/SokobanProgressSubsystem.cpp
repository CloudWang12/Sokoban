#include "SokobanProgressSubsystem.h"
#include "Kismet/GameplayStatics.h"

void USokobanProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Load();
}

void USokobanProgressSubsystem::Load()
{
	Data = nullptr;
	bWriteBlocked = false;
	bDirty = false;
	Status = FText::GetEmpty();
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		Data = Cast<USokobanSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
		if (!Data || !Data->IsValidPayload())
		{
			bWriteBlocked = true;
			Data = nullptr;
			Status = FText::FromString(TEXT("存档损坏或版本不兼容，原文件未覆盖。可继续游玩，并在菜单确认备份修复。"));
		}
	}
	if (!Data) { Data = NewObject<USokobanSaveGame>(this); }
}

#if WITH_DEV_AUTOMATION_TESTS
void USokobanProgressSubsystem::SetAutomationSlot(const FString& InSlot)
{
	check(InSlot.StartsWith(TEXT("SokobanAutomation_")));
	SlotName = InSlot;
	Load();
}
#endif

bool USokobanProgressSubsystem::GetProgress(FName Id, int32 Revision, FSokobanLevelProgress& Out) const
{
	Out = FSokobanLevelProgress();
	return Data && Data->GetProgress(Id, Revision, Out);
}

FName USokobanProgressSubsystem::GetLastPlayedLevel() const { return Data ? Data->LastPlayedLevel : NAME_None; }

void USokobanProgressSubsystem::SetLastPlayedLevel(FName Id)
{
	if (Data && !Id.IsNone() && Data->LastPlayedLevel != Id) { Data->LastPlayedLevel = Id; bDirty = true; }
}

bool USokobanProgressSubsystem::RecordCompletion(FName Id, int32 Revision, const FSokobanMoveCounters& Counters, bool& bOutNewBest)
{
	bOutNewBest = false;
	if (!Data || !Data->RecordCompletion(Id, Revision, Counters, bOutNewBest)) { return false; }
	bDirty = true;
	return true;
}

bool USokobanProgressSubsystem::Save()
{
	if (bWriteBlocked || !Data) { return false; }
	if (!bDirty) { return true; }
	if (!Data->IsValidPayload() || !UGameplayStatics::SaveGameToSlot(Data, SlotName, 0))
	{
		Status = FText::FromString(TEXT("进度写入失败，成绩仍保留在本次运行中。请检查存储权限并重试保存。"));
		return false;
	}
	bDirty = false;
	Status = FText::FromString(TEXT("进度已保存。"));
	return true;
}

bool USokobanProgressSubsystem::RecoverSaving()
{
	if (!bWriteBlocked) { return Save(); }
	// 先保全原始字节。备份失败时仍保持写保护，不删除或覆盖问题存档。
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		TArray<uint8> Bytes;
		const FString Backup = SlotName + TEXT("_backup_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (!UGameplayStatics::LoadDataFromSlot(Bytes, SlotName, 0) || !UGameplayStatics::SaveDataToSlot(Bytes, Backup, 0))
		{
			Status = FText::FromString(TEXT("原存档备份失败，未覆盖原文件。请检查存储权限。"));
			return false;
		}
	}
	bWriteBlocked = false;
	bDirty = true;
	return Save();
}
