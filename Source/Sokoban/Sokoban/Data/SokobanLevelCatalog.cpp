#include "SokobanLevelCatalog.h"

USokobanLevelData* USokobanLevelCatalog::GetLevel(int32 Index) const
{
	return Levels.IsValidIndex(Index) ? Levels[Index].Get() : nullptr;
}

int32 USokobanLevelCatalog::FindLevelIndex(FName LevelId) const
{
	return LevelId.IsNone() ? INDEX_NONE : Levels.IndexOfByPredicate([LevelId](const auto& Level) { return Level && Level->LevelId == LevelId; });
}

int32 USokobanLevelCatalog::GetNextIndex(int32 Index) const
{
	return Levels.IsValidIndex(Index) && Index < Levels.Num() - 1 ? Index + 1 : INDEX_NONE;
}

bool USokobanLevelCatalog::ValidateCatalog(TArray<FText>& OutErrors) const
{
	OutErrors.Reset();
	if (Levels.IsEmpty()) { OutErrors.Add(FText::FromString(TEXT("关卡目录为空，请配置 Levels。"))); }
	TSet<FName> Ids;
	for (int32 Index = 0; Index < Levels.Num(); ++Index)
	{
		const auto* Level = GetLevel(Index);
		if (!Level || Level->LevelId.IsNone() || Level->DataVersion != 1 || Level->ContentRevision < 1)
		{
			OutErrors.Add(FText::FromString(FString::Printf(TEXT("关卡 %d 的引用、ID 或版本无效。"), Index + 1)));
			continue;
		}
		if (Ids.Contains(Level->LevelId)) { OutErrors.Add(FText::FromString(TEXT("关卡 ID 重复：") + Level->LevelId.ToString())); }
		Ids.Add(Level->LevelId);
	}
	return OutErrors.IsEmpty();
}
