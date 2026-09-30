#include "SokobanUIManagerComponent.h"
#include "SokobanScreen.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Sokoban/Gameplay/SokobanGameMode.h"
#include "Sokoban/Gameplay/SokobanPlayerController.h"

USokobanUIManagerComponent::USokobanUIManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USokobanUIManagerComponent::BeginPlay()
{
	Super::BeginPlay();
	auto* PC = Cast<ASokobanPlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController() || !PC->GetGameInstance()) { return; }
	Flow = PC->GetGameInstance()->GetSubsystem<USokobanFlowSubsystem>();
	if (Flow.IsValid()) { ChangedHandle = Flow->OnChanged().AddUObject(this, &USokobanUIManagerComponent::Refresh); }
	Refresh();
}

void USokobanUIManagerComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Flow.IsValid()) { Flow->OnChanged().Remove(ChangedHandle); }
	if (Screen) { Screen->RemoveFromParent(); Screen = nullptr; }
	Flow.Reset();
	Super::EndPlay(Reason);
}

void USokobanUIManagerComponent::Refresh()
{
	if (bRefreshing) { return; }
	TGuardValue<bool> Guard(bRefreshing, true);
	auto* PC = Cast<ASokobanPlayerController>(GetOwner());
	const auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ASokobanGameMode>() : nullptr;
	if (!PC || !PC->IsLocalController() || !Flow.IsValid() || !Mode || !Mode->bEnableFrontend) { return; }
	UClass* Class = nullptr;
	UClass* Parent = nullptr;
	const TCHAR* Field = TEXT("");
	switch (Flow->GetState())
	{
	case ESokobanFlowState::MainMenu: Class = MainMenuClass; Parent = USokobanMainMenuWidget::StaticClass(); Field = TEXT("MainMenuClass"); break;
	case ESokobanFlowState::LevelSelect: Class = LevelSelectClass; Parent = USokobanLevelSelectWidget::StaticClass(); Field = TEXT("LevelSelectClass"); break;
	case ESokobanFlowState::Playing: Class = PlayClass; Parent = USokobanPlayWidget::StaticClass(); Field = TEXT("PlayClass"); break;
	case ESokobanFlowState::Results: Class = ResultsClass; Parent = USokobanResultsWidget::StaticClass(); Field = TEXT("ResultsClass"); break;
	}
	// 同页通知只同步数据，不重建 WBP，避免丢失蓝图里的焦点、弹窗和动画状态。
	if (Screen && ScreenState == Flow->GetState() && Screen->GetClass() == Class)
	{
		Screen->RefreshViewData();
		return;
	}
	if (Screen) { Screen->RemoveFromParent(); Screen = nullptr; }
	PC->ResetDirectionalInput();
	PC->bShowMouseCursor = true;
	FText Error;
	if (!Class || !Parent || !Class->IsChildOf(Parent) || Class->HasAnyClassFlags(CLASS_Abstract) || Class->HasAnyClassFlags(CLASS_Native))
	{
		Error = FText::FromString(FString::Printf(TEXT("UIManager.%s 需要配置继承 %s 的 Widget Blueprint。"), Field, Parent ? *Parent->GetName() : TEXT("SokobanScreen")));
	}
	else
	{
		Screen = CreateWidget<USokobanScreen>(PC, Class);
		if (Screen)
		{
			ScreenState = Flow->GetState();
			Screen->AddToViewport(20);
		}
		else { Error = FText::FromString(FString::Printf(TEXT("UIManager.%s 创建失败。"), Field)); }
	}
	if (!Error.IsEmpty() && !Error.EqualTo(ConfigurationError)) { UE_LOG(LogTemp, Warning, TEXT("%s"), *Error.ToString()); }
	ConfigurationError = Error;
	// 即使缺少 WBP，菜单状态也不能让输入穿透到棋盘。
	if (Flow->GetState() == ESokobanFlowState::Playing)
	{
		FInputModeGameAndUI Input;
		Input.SetHideCursorDuringCapture(false);
		Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Input);
		UWidgetBlueprintLibrary::SetFocusToGameViewport();
	}
	else
	{
		FInputModeUIOnly Input;
		if (Screen && Screen->IsFocusable()) { Input.SetWidgetToFocus(Screen->TakeWidget()); }
		Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Input);
	}
}
