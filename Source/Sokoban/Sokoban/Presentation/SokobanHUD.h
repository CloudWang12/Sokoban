#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SokobanHUD.generated.h"

/** 灰盒阶段的轻量 HUD，后续可用正式 UMG 界面替换。 */
UCLASS()
class SOKOBAN_API ASokobanHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
