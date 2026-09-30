#include "SokobanBoardActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sokoban/Rules/SokobanLevelValidator.h"
#include "UObject/ConstructorHelpers.h"

ASokobanBoardActor::ASokobanBoardActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	BoardRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BoardRoot"));
	SetRootComponent(BoardRoot);
	Floors = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Floors"));
	Walls = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Walls"));
	Goals = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Goals"));
	PlayerVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlayerVisual"));
	for (UStaticMeshComponent* Component : { static_cast<UStaticMeshComponent*>(Floors.Get()), static_cast<UStaticMeshComponent*>(Walls.Get()), static_cast<UStaticMeshComponent*>(Goals.Get()), PlayerVisual.Get() })
	{
		Component->SetupAttachment(BoardRoot);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
	}
	PlayerVisual->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/Sokoban/Materials/M_SokobanDefault.M_SokobanDefault"));
	CubeMesh = Cube.Object;
	SphereMesh = Sphere.Object;
	CylinderMesh = Cylinder.Object;
	DefaultMaterial = Material.Object;
}

UMaterialInterface* ASokobanBoardActor::ResolveMaterial(UMaterialInterface* Override, const FLinearColor& Color, float EmissiveStrength)
{
	if (Override) { return Override; }
	// UMaterialInstanceDynamic 是运行时材质实例。同一类元素共享一份颜色，
	// 不修改 Content 中的基础材质，也不要求用户提前制作六份材质资产。
	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(DefaultMaterial, this);
	Instance->SetVectorParameterValue(TEXT("BaseColor"), Color);
	Instance->SetScalarParameterValue(TEXT("EmissiveStrength"), EmissiveStrength);
	return Instance;
}

FVector ASokobanBoardActor::GridToLocal(FIntPoint Cell, float Height) const
{
	return FVector(-static_cast<double>(Cell.Y) * CellSize, static_cast<double>(Cell.X) * CellSize, Height);
}

FVector ASokobanBoardActor::GridToWorld(FIntPoint Cell, float Height) const
{
	return GetActorTransform().TransformPosition(GridToLocal(Cell, Height));
}

bool ASokobanBoardActor::CanDisplayState(const FSokobanLevelDefinition& Level, const FSokobanBoardState& State) const
{
	// 这是显示入口的防御性检查。完整的玩法规则仍由 Validator 和 Rules 负责。
	if (State.BoxPositions.Num() != Level.Boxes.Num()) { return false; }
	const auto IsFloor = [&Level](FIntPoint Cell)
	{
		return Cell.X >= 0 && Cell.X < Level.Width && Cell.Y >= 0 && Cell.Y < Level.Height &&
			Level.Terrain[Cell.Y * Level.Width + Cell.X] == ESokobanTerrain::Floor;
	};
	if (!IsFloor(State.PlayerPosition)) { return false; }
	TSet<FIntPoint> Occupied;
	Occupied.Add(State.PlayerPosition);
	for (const FSokobanBoxDefinition& Box : Level.Boxes)
	{
		const FIntPoint* Cell = State.BoxPositions.Find(Box.BoxId);
		if (!Cell || !IsFloor(*Cell) || Occupied.Contains(*Cell)) { return false; }
		Occupied.Add(*Cell);
	}
	return true;
}

void ASokobanBoardActor::ClearBoard()
{
	bAnimating = false;
	bHasBoard = false;
	SetActorTickEnabled(false);
	Floors->ClearInstances();
	Walls->ClearInstances();
	Goals->ClearInstances();
	PlayerVisual->SetVisibility(false);
	for (const auto& Pair : BoxVisuals)
	{
		if (IsValid(Pair.Value))
		{
			RemoveInstanceComponent(Pair.Value);
			Pair.Value->DestroyComponent();
		}
	}
	BoxVisuals.Reset();
}

bool ASokobanBoardActor::BuildBoard(const FSokobanLevelDefinition& Level, const FSokobanBoardState& State)
{
	for (const FSokobanValidationIssue& Issue : FSokobanLevelValidator::Validate(Level))
	{
		if (Issue.Severity == ESokobanValidationSeverity::Error) { return false; }
	}
	if (!FMath::IsFinite(CellSize) || CellSize < 10.0f || !CubeMesh || !SphereMesh || !CylinderMesh || !DefaultMaterial || !CanDisplayState(Level, State))
	{
		return false;
	}
	ClearBoard();
	Definition = Level;
	Floors->SetStaticMesh(CubeMesh);
	Walls->SetStaticMesh(CubeMesh);
	Goals->SetStaticMesh(CylinderMesh);
	PlayerVisual->SetStaticMesh(SphereMesh);
	Floors->SetMaterial(0, ResolveMaterial(FloorMaterial, FloorColor));
	Walls->SetMaterial(0, ResolveMaterial(WallMaterial, WallColor));
	// 黄色目标带少量自发光，在墙体阴影中也能辨认；圆盘仍保留为独立形状标记。
	Goals->SetMaterial(0, ResolveMaterial(GoalMaterial, GoalColor, 0.35f));
	PlayerVisual->SetMaterial(0, ResolveMaterial(PlayerMaterial, PlayerColor, 0.08f));
	ResolvedBoxMaterial = ResolveMaterial(BoxMaterial, BoxColor);
	ResolvedBoxOnGoalMaterial = ResolveMaterial(BoxOnGoalMaterial, BoxOnGoalColor, 0.12f);

	// UE 基础几何体尺寸为 100 cm。所有变换都使用棋盘局部坐标，允许整体平移、旋转和缩放。
	for (int32 Index = 0; Index < Level.Terrain.Num(); ++Index)
	{
		if (Level.Terrain[Index] == ESokobanTerrain::Void) { continue; }
		const FIntPoint Cell(Index % Level.Width, Index / Level.Width);
		Floors->AddInstance(FTransform(FRotator::ZeroRotator, GridToLocal(Cell, -FloorThickness * 0.5f), FVector(CellSize * 0.98f / 100.0f, CellSize * 0.98f / 100.0f, FloorThickness / 100.0f)));
		if (Level.Terrain[Index] == ESokobanTerrain::Wall)
		{
			Walls->AddInstance(FTransform(FRotator::ZeroRotator, GridToLocal(Cell, WallHeight * 0.5f), FVector(CellSize * 0.98f / 100.0f, CellSize * 0.98f / 100.0f, WallHeight / 100.0f)));
		}
	}
	for (const FIntPoint Cell : Level.Goals)
	{
		// 目标圆盘比箱子宽，到位后仍能看到外缘。
		Goals->AddInstance(FTransform(FRotator::ZeroRotator, GridToLocal(Cell, 3.0f), FVector(CellSize * 0.9f / 100.0f, CellSize * 0.9f / 100.0f, 0.02f)));
	}
	PlayerVisual->SetRelativeScale3D(FVector(CellSize * 0.45f / 100.0f, CellSize * 0.45f / 100.0f, PlayerHeight / 100.0f));
	PlayerVisual->SetVisibility(true);
	for (const FSokobanBoxDefinition& Box : Level.Boxes)
	{
		UStaticMeshComponent* Visual = NewObject<UStaticMeshComponent>(this);
		AddInstanceComponent(Visual);
		Visual->SetupAttachment(BoardRoot);
		Visual->SetMobility(EComponentMobility::Movable);
		Visual->SetStaticMesh(CubeMesh);
		Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Visual->SetCanEverAffectNavigation(false);
		Visual->SetRelativeScale3D(FVector(CellSize * 0.62f / 100.0f, CellSize * 0.62f / 100.0f, BoxHeight / 100.0f));
		Visual->RegisterComponent();
		BoxVisuals.Add(Box.BoxId, Visual);
	}
	bHasBoard = true;
	return SynchronizeState(State);
}

void ASokobanBoardActor::UpdateBoxMaterials(const FSokobanBoardState& State)
{
	for (const auto& Pair : BoxVisuals)
	{
		const bool bOnGoal = Definition.Goals.Contains(State.BoxPositions.FindChecked(Pair.Key));
		Pair.Value->SetMaterial(0, bOnGoal ? ResolvedBoxOnGoalMaterial : ResolvedBoxMaterial);
	}
}

bool ASokobanBoardActor::SynchronizeState(const FSokobanBoardState& State)
{
	if (!bHasBoard || !CanDisplayState(Definition, State) || BoxVisuals.Num() != State.BoxPositions.Num()) { return false; }
	for (const auto& Pair : State.BoxPositions)
	{
		const auto* Visual = BoxVisuals.Find(Pair.Key);
		if (!Visual || !IsValid(Visual->Get())) { return false; }
	}
	// 初始化、重开和动画结束都精确同步，避免累计插值误差。
	bAnimating = false;
	SetActorTickEnabled(false);
	PlayerVisual->SetRelativeLocation(GridToLocal(State.PlayerPosition, PlayerHeight * 0.5f));
	for (const auto& Pair : State.BoxPositions)
	{
		BoxVisuals.FindChecked(Pair.Key)->SetRelativeLocation(GridToLocal(Pair.Value, BoxHeight * 0.5f));
	}
	UpdateBoxMaterials(State);
	return true;
}

bool ASokobanBoardActor::PresentMove(const FSokobanMoveRecord& Record, bool bReverse, const FSokobanBoardState& FinalState)
{
	if (!bHasBoard || bAnimating || !CanDisplayState(Definition, FinalState)) { return false; }
	if (Record.BoxId != INDEX_NONE && !BoxVisuals.Contains(Record.BoxId)) { return false; }
	PendingState = FinalState;
	PlayerStart = PlayerVisual->GetRelativeLocation();
	PlayerEnd = GridToLocal(bReverse ? Record.PlayerFrom : Record.PlayerTo, PlayerHeight * 0.5f);
	MovingBoxId = Record.BoxId;
	if (MovingBoxId != INDEX_NONE)
	{
		UStaticMeshComponent* Box = BoxVisuals.FindChecked(MovingBoxId);
		BoxStart = Box->GetRelativeLocation();
		BoxEnd = GridToLocal(bReverse ? Record.BoxFrom : Record.BoxTo, BoxHeight * 0.5f);
		Box->SetMaterial(0, ResolvedBoxMaterial);
	}
	ActiveDuration = FMath::IsFinite(MoveDuration) ? FMath::Clamp(MoveDuration, 0.0f, 2.0f) : 0.0f;
	if (ActiveDuration <= KINDA_SMALL_NUMBER) { return SynchronizeState(FinalState); }
	AnimationElapsed = 0.0f;
	bAnimating = true;
	SetActorTickEnabled(true);
	return true;
}

void ASokobanBoardActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bAnimating) { return; }
	AnimationElapsed += FMath::Max(0.0f, DeltaSeconds);
	const float Alpha = FMath::Clamp(AnimationElapsed / ActiveDuration, 0.0f, 1.0f);
	const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
	PlayerVisual->SetRelativeLocation(FMath::Lerp(PlayerStart, PlayerEnd, SmoothAlpha));
	if (MovingBoxId != INDEX_NONE)
	{
		BoxVisuals.FindChecked(MovingBoxId)->SetRelativeLocation(FMath::Lerp(BoxStart, BoxEnd, SmoothAlpha));
	}
	if (Alpha >= 1.0f) { SynchronizeState(PendingState); }
}

FBox ASokobanBoardActor::GetBoardBounds() const
{
	if (!bHasBoard) { return FBox(GetActorLocation(), GetActorLocation()); }
	const FVector Min(-(Definition.Height - 0.5f) * CellSize, -CellSize * 0.5f, -FloorThickness);
	const FVector Max(CellSize * 0.5f, (Definition.Width - 0.5f) * CellSize, WallHeight);
	return FBox(Min, Max).TransformBy(GetActorTransform());
}

FVector ASokobanBoardActor::GetPlayerVisualLocation() const
{
	return PlayerVisual->GetComponentLocation();
}

bool ASokobanBoardActor::GetBoxVisualLocation(int32 BoxId, FVector& OutLocation) const
{
	const auto* Visual = BoxVisuals.Find(BoxId);
	if (!Visual || !IsValid(Visual->Get())) { return false; }
	OutLocation = (*Visual)->GetComponentLocation();
	return true;
}
