#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Sokoban/UI/SokobanScreen.h"

/** 仅供测试的临时 WBP，不保存资产；不依赖策划布局、控件名称或按钮文字。 */
inline UWidgetBlueprint* MakeSokobanUITestBlueprint(UClass* Parent)
{
	auto* Blueprint = CastChecked<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
		Parent, GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UWidgetBlueprint::StaticClass(), TEXT("SokobanUITest")),
		BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
	Blueprint->WidgetTree->RootWidget = Blueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DesignerOwnsThisRoot"));
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	return Blueprint;
}
#endif
