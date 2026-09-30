#include "SokobanCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SokobanBoardActor.h"
#include "Sokoban/Gameplay/SokobanGameMode.h"

ASokobanCameraPawn::ASokobanCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("CameraRoot")));
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	Camera->FieldOfView = 50.0f;
	Camera->bConstrainAspectRatio = false;
	Camera->bOverrideAspectRatioAxisConstraint = true;
	Camera->AspectRatioAxisConstraint = AspectRatio_MaintainXFOV;
	Camera->bUsePawnControlRotation = false;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
}

void ASokobanCameraPawn::SetBoard(ASokobanBoardActor* InBoard)
{
	Board = InBoard;
	LastViewportSize = FIntPoint::ZeroValue;
	FrameBoard(16.0f / 9.0f);
}

void ASokobanCameraPawn::FrameBoard(float AspectRatio)
{
	if (!Board.IsValid()) { return; }
	AspectRatio = FMath::IsFinite(AspectRatio) && AspectRatio > 0.0f ? AspectRatio : 16.0f / 9.0f;
	Camera->SetAspectRatio(AspectRatio);
	const FBox Bounds = Board->GetBoardBounds();
	// 采用包围球保守取景，横屏、竖屏和棋盘旋转后都能容纳整张棋盘。
	const double Radius = FMath::Max(1.0, Bounds.GetExtent().Size());
	const double HalfHorizontal = FMath::DegreesToRadians(FMath::Clamp(Camera->FieldOfView, 10.0f, 120.0f) * 0.5);
	const double HalfVertical = FMath::Atan(FMath::Tan(HalfHorizontal) / AspectRatio);
	const double Distance = Radius * FMath::Clamp(FrameMargin, 1.0f, 3.0f) / FMath::Sin(FMath::Min(HalfHorizontal, HalfVertical));
	const FQuat Rotation = Board->GetActorQuat() * FRotator(-FMath::Clamp(ViewPitch, 30.0f, 89.0f), 0.0f, 0.0f).Quaternion();
	SetActorLocationAndRotation(Bounds.GetCenter() - Rotation.GetForwardVector() * Distance, Rotation);
	LastBoardTransform = Board->GetActorTransform();
}

void ASokobanCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// 兼容 Pawn 与 GameMode 初始化顺序不同的情况。
	if (!Board.IsValid())
	{
		if (const ASokobanGameMode* Mode = Cast<ASokobanGameMode>(GetWorld()->GetAuthGameMode()))
		{
			if (Mode->GetBoard()) { SetBoard(Mode->GetBoard()); }
		}
	}
	if (!Board.IsValid()) { return; }
	int32 Width = 0;
	int32 Height = 0;
	if (APlayerController* PC = Cast<APlayerController>(GetController())) { PC->GetViewportSize(Width, Height); }
	const FIntPoint Size(Width, Height);
	if (Size != LastViewportSize || !LastBoardTransform.Equals(Board->GetActorTransform()))
	{
		FrameBoard(Height > 0 ? static_cast<float>(Width) / Height : 16.0f / 9.0f);
		LastViewportSize = Size;
	}
}
