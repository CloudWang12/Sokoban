#include "SokobanPlayerController.h"

#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerInput.h"
#include "Sokoban/UI/SokobanUIManagerComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "SokobanGameMode.h"
#include "Sokoban/Presentation/SokobanCameraPawn.h"

#define LOCTEXT_NAMESPACE "SokobanPlayerController"

ASokobanPlayerController::ASokobanPlayerController()
{
	bShowMouseCursor = false;
	UIManager = CreateDefaultSubobject<USokobanUIManagerComponent>(TEXT("SokobanUIManager"));
}

void ASokobanPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		const auto* Mode = GetWorld()->GetAuthGameMode<ASokobanGameMode>();
		if (Mode && Mode->bEnableFrontend) { UIManager->Refresh(); }
		else { SetInputMode(FInputModeGameOnly()); }
	}
}

bool ASokobanPlayerController::ValidateInputConfiguration(FText& OutError) const
{
	OutError = FText::GetEmpty();
	if (!InputMappingContext || !MoveAction || !UndoAction || !RedoAction || !RestartAction)
	{
		OutError = LOCTEXT("MissingAssets", "Configure Input Mapping Context and all four Input Actions in BP_SokobanPlayerController.");
		return false;
	}
	if (MoveAction->ValueType != EInputActionValueType::Axis2D || UndoAction->ValueType != EInputActionValueType::Boolean ||
		RedoAction->ValueType != EInputActionValueType::Boolean || RestartAction->ValueType != EInputActionValueType::Boolean)
	{
		OutError = LOCTEXT("WrongTypes", "Move Action must be Axis2D; Undo / Redo / Restart must be Boolean.");
		return false;
	}
	TSet<const UInputAction*> UniqueActions;
	for (const UInputAction* Action : { MoveAction.Get(), UndoAction.Get(), RedoAction.Get(), RestartAction.Get() })
	{
		UniqueActions.Add(Action);
		const bool bMapped = InputMappingContext->GetMappings().ContainsByPredicate([Action](const FEnhancedActionKeyMapping& Mapping) { return Mapping.Action == Action; });
		if (!bMapped)
		{
			OutError = LOCTEXT("Unmapped", "Every assigned Input Action must have a key mapping in the Input Mapping Context.");
			return false;
		}
	}
	if (UniqueActions.Num() != 4)
	{
		OutError = LOCTEXT("DuplicateActions", "Use four distinct Input Action assets.");
		return false;
	}
	return true;
}

void ASokobanPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!IsLocalController()) { return; }
	UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Enhanced)
	{
		InputConfigurationError = LOCTEXT("WrongComponent", "Set Default Input Component Class to EnhancedInputComponent in Project Settings.");
		return;
	}
	if (!ValidateInputConfiguration(InputConfigurationError)) { return; }
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Subsystem)
	{
		InputConfigurationError = LOCTEXT("NoSubsystem", "Enhanced Input local player subsystem is unavailable.");
		return;
	}
	Subsystem->AddMappingContext(InputMappingContext, 0);
	bAddedMappingContext = true;
	Enhanced->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASokobanPlayerController::OnMoveTriggered);
	Enhanced->BindAction(MoveAction, ETriggerEvent::Completed, this, &ASokobanPlayerController::OnMoveReleased);
	Enhanced->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ASokobanPlayerController::OnMoveReleased);
	Enhanced->BindAction(UndoAction, ETriggerEvent::Started, this, &ASokobanPlayerController::OnUndoStarted);
	Enhanced->BindAction(RedoAction, ETriggerEvent::Started, this, &ASokobanPlayerController::OnRedoStarted);
	Enhanced->BindAction(RestartAction, ETriggerEvent::Started, this, &ASokobanPlayerController::OnRestartStarted);
}

void ASokobanPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	HeldDirection = ESokobanDirection::None;
	if (ASokobanCameraPawn* Camera = Cast<ASokobanCameraPawn>(InPawn))
	{
		if (const ASokobanGameMode* Mode = Cast<ASokobanGameMode>(GetWorld()->GetAuthGameMode())) { Camera->SetBoard(Mode->GetBoard()); }
	}
}

void ASokobanPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bAddedMappingContext)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(InputMappingContext);
		}
	}
	Super::EndPlay(EndPlayReason);
}

ESokobanDirection ASokobanPlayerController::ResolveGridDirection(FVector2D Axis, float DeadZone)
{
	if (!FMath::IsFinite(Axis.X) || !FMath::IsFinite(Axis.Y)) { return ESokobanDirection::None; }
	const double Threshold = FMath::IsFinite(DeadZone) ? FMath::Clamp(DeadZone, 0.05f, 1.0f) : 0.5f;
	if (FMath::Max(FMath::Abs(Axis.X), FMath::Abs(Axis.Y)) < Threshold) { return ESokobanDirection::None; }
	if (FMath::Abs(Axis.X) >= FMath::Abs(Axis.Y)) { return Axis.X > 0.0 ? ESokobanDirection::Right : ESokobanDirection::Left; }
	return Axis.Y > 0.0 ? ESokobanDirection::Up : ESokobanDirection::Down;
}

void ASokobanPlayerController::ResetDirectionalInput()
{
	HeldDirection = ESokobanDirection::None;
	if (PlayerInput) { PlayerInput->FlushPressedKeys(); }
}

void ASokobanPlayerController::OnMoveTriggered(const FInputActionValue& Value)
{
	const ESokobanDirection Direction = ResolveGridDirection(Value.Get<FVector2D>(), InputDeadZone);
	// 第一版每次按下／改变方向只走一步。Triggered 每帧触发，但同一持续输入不会重复移动。
	if (Direction == HeldDirection) { return; }
	HeldDirection = Direction;
	if (Direction == ESokobanDirection::None) { return; }
	if (ASokobanGameMode* Mode = Cast<ASokobanGameMode>(GetWorld()->GetAuthGameMode())) { Mode->RequestMove(Direction); }
}

void ASokobanPlayerController::OnMoveReleased(const FInputActionValue& Value)
{
	HeldDirection = ESokobanDirection::None;
}

void ASokobanPlayerController::OnUndoStarted()
{
	if (ASokobanGameMode* Mode = Cast<ASokobanGameMode>(GetWorld()->GetAuthGameMode())) { Mode->RequestUndo(); }
}

void ASokobanPlayerController::OnRedoStarted()
{
	if (ASokobanGameMode* Mode = Cast<ASokobanGameMode>(GetWorld()->GetAuthGameMode())) { Mode->RequestRedo(); }
}

void ASokobanPlayerController::OnRestartStarted()
{
	if (ASokobanGameMode* Mode = Cast<ASokobanGameMode>(GetWorld()->GetAuthGameMode())) { Mode->RequestRestart(); }
}

#undef LOCTEXT_NAMESPACE
