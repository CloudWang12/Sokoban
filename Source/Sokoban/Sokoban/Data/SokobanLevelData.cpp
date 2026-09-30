#include "SokobanLevelData.h"

FPrimaryAssetId USokobanLevelData::GetPrimaryAssetId() const
{
	if (IsTemplate() || LevelId.IsNone())
	{
		return FPrimaryAssetId();
	}

	static const FPrimaryAssetType LevelAssetType(TEXT("SokobanLevel"));
	return FPrimaryAssetId(LevelAssetType, LevelId);
}
