#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"
#include "Sokoban/Gameplay/SokobanGameMode.h"
#include "Sokoban/Gameplay/SokobanPlayerController.h"
#include "Sokoban/Presentation/SokobanBoardActor.h"
#include "Sokoban/Presentation/SokobanCameraPawn.h"

namespace SokobanGameplayTests
{
	// 临时世界不加载用户地图，不创建或保存内容资产。析构时统一释放 Actor 和组件。
	struct FTestWorld
	{
		UWorld* World;
		FTestWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(true).ShouldSimulatePhysics(false)
				.CreateNavigation(false).CreateAISystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		}
		~FTestWorld() { if (World) { World->DestroyWorld(false); } }
	};

	bool Success(const FSokobanMoveResult& Result)
	{
		return Result.Outcome == ESokobanMoveOutcome::Walk || Result.Outcome == ESokobanMoveOutcome::Push;
	}
}

using namespace SokobanGameplayTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanGameplayFlowTest,
	"Sokoban.Gameplay.MoveAnimationAndCompletion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanGameplayFlowTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	ASokobanGameMode* Mode = TestWorld.World->SpawnActor<ASokobanGameMode>();
	if (!TestNotNull(TEXT("创建 GameMode"), Mode)) { return false; }
	TestFalse(TEXT("初始化前拒绝重开"), Mode->RequestRestart());
	if (!TestTrue(TEXT("示例关卡初始化"), Mode->InitializeLevel(ASokobanGameMode::MakeDemoLevel()))) { return false; }
	ASokobanBoardActor* Board = Mode->GetBoard();
	USokobanSession* Session = Mode->GetSession();
	// 验证实际绑定的材质参数，避免只有颜色属性、显示仍共用白材质的情况。
	TArray<UStaticMeshComponent*> Visuals;
	Board->GetComponents(Visuals);
	UStaticMeshComponent* FirstBox = nullptr;
	UStaticMeshComponent* GoalVisual = nullptr;
	for (UStaticMeshComponent* Visual : Visuals)
	{
		if (Visual->GetFName() == TEXT("Goals")) { GoalVisual = Visual; }
		if (Visual->GetComponentLocation().Equals(Board->GridToWorld(FIntPoint(2, 2), 45.f))) { FirstBox = Visual; }
	}
	auto HasColor = [](UStaticMeshComponent* Visual, const FLinearColor& Expected)
	{
		UMaterialInstanceDynamic* Material = Visual ? Cast<UMaterialInstanceDynamic>(Visual->GetMaterial(0)) : nullptr;
		return Material && Material->K2_GetVectorParameterValue(TEXT("BaseColor")).Equals(Expected);
	};
	TestTrue(TEXT("目标使用独立黄色材质"), HasColor(GoalVisual, Board->GoalColor));
	TestTrue(TEXT("普通箱子使用橙色材质"), HasColor(FirstBox, Board->BoxColor));
	const FVector Start = Board->GetPlayerVisualLocation();
	TestTrue(TEXT("第一次输入推动箱子"), Mode->RequestMove(ESokobanDirection::Up).Outcome == ESokobanMoveOutcome::Push);
	TestTrue(TEXT("逻辑位置先到终点"), Session->GetState().PlayerPosition == FIntPoint(2, 2));
	TestTrue(TEXT("画面从起点播放"), Board->GetPlayerVisualLocation().Equals(Start));
	TestTrue(TEXT("动画中锁定输入"), Mode->IsPresentationBusy());
	TestFalse(TEXT("动画中不能重复移动"), Success(Mode->RequestMove(ESokobanDirection::Down)));
	TestFalse(TEXT("动画中不能撤销"), Mode->RequestUndo());
	TestFalse(TEXT("动画中不能重做"), Mode->RequestRedo());
	TestFalse(TEXT("动画中不能重开"), Mode->RequestRestart());
	TestFalse(TEXT("动画中不能切换关卡"), Mode->InitializeLevel(ASokobanGameMode::MakeDemoLevel()));
	TestEqual(TEXT("被拒绝的操作不增加历史"), Session->GetUndoCount(), 1);
	Board->Tick(Board->MoveDuration * 0.5f);
	TestTrue(TEXT("动画中点位于两个格子之间"), Board->GetPlayerVisualLocation().Equals(
		FMath::Lerp(Start, Board->GridToWorld(FIntPoint(2, 2), 40.f), 0.5), 0.01));
	Board->Tick(Board->MoveDuration);
	TestFalse(TEXT("动画结束解锁"), Mode->IsPresentationBusy());
	TestTrue(TEXT("箱子到位后变绿"), HasColor(FirstBox, Board->BoxOnGoalColor));
	TestTrue(TEXT("精确对齐终点"), Board->GetPlayerVisualLocation().Equals(Board->GridToWorld(FIntPoint(2, 2), 40.f)));

	for (ESokobanDirection Direction : {ESokobanDirection::Down, ESokobanDirection::Right, ESokobanDirection::Right})
	{
		TestTrue(TEXT("继续完成解题路径"), Success(Mode->RequestMove(Direction)));
		Board->Tick(Board->MoveDuration + 0.01f);
	}
	TestTrue(TEXT("最后一步推动"), Mode->RequestMove(ESokobanDirection::Up).Outcome == ESokobanMoveOutcome::Push);
	TestTrue(TEXT("规则层已经通关"), Session->IsSolved());
	TestFalse(TEXT("动画未结束不显示通关"), Mode->IsCompletionVisible());
	Board->Tick(Board->MoveDuration + 0.01f);
	TestTrue(TEXT("动画结束才显示通关"), Mode->IsCompletionVisible());
	TestEqual(TEXT("解法移动五次"), Session->GetState().Counters.MoveCount, 5);
	TestEqual(TEXT("解法推动两次"), Session->GetState().Counters.PushCount, 2);
	TestFalse(TEXT("通关后禁止普通移动"), Success(Mode->RequestMove(ESokobanDirection::Down)));
	TestTrue(TEXT("通关后仍可撤销"), Mode->RequestUndo());
	TestFalse(TEXT("撤销立即撤回通关标志"), Mode->IsCompletionVisible());
	Board->Tick(Board->MoveDuration + 0.01f);
	FVector BoxLocation;
	TestTrue(TEXT("按 ID 查找箱子"), Board->GetBoxVisualLocation(1, BoxLocation));
	TestTrue(TEXT("箱子反向回到原位"), BoxLocation.Equals(Board->GridToWorld(FIntPoint(4, 2), 45.f)));
	TestTrue(TEXT("重做成功"), Mode->RequestRedo());
	Board->Tick(Board->MoveDuration + 0.01f);
	TestTrue(TEXT("重做恢复通关"), Mode->IsCompletionVisible());
	TestTrue(TEXT("重开成功"), Mode->RequestRestart());
	TestFalse(TEXT("重开立即取消通关"), Mode->IsCompletionVisible());
	TestTrue(TEXT("重开恢复画面出生点"), Board->GetPlayerVisualLocation().Equals(Start));
	TestEqual(TEXT("重开清除历史"), Session->GetUndoCount(), 0);
	TestTrue(TEXT("重开恢复普通箱子颜色"), HasColor(FirstBox, Board->BoxColor));
	Board->MoveDuration = 0.f;
	TestTrue(TEXT("零时长也能推动"), Success(Mode->RequestMove(ESokobanDirection::Up)));
	TestFalse(TEXT("零时长不会卡住输入锁"), Mode->IsPresentationBusy());
	TestTrue(TEXT("零时长到位也正确变绿"), HasColor(FirstBox, Board->BoxOnGoalColor));
	TestTrue(TEXT("撤销推动成功"), Mode->RequestUndo());
	TestTrue(TEXT("撤销离开目标恢复橙色"), HasColor(FirstBox, Board->BoxColor));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanBoardLifecycleTest,
	"Sokoban.Gameplay.BoardRebuildAndTransform", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanBoardLifecycleTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	ASokobanBoardActor* Board = TestWorld.World->SpawnActor<ASokobanBoardActor>();
	ASokobanGameMode* Mode = TestWorld.World->SpawnActor<ASokobanGameMode>();
	if (!TestNotNull(TEXT("创建棋盘"), Board) || !TestNotNull(TEXT("创建 GameMode"), Mode)) { return false; }
	Board->SetActorTransform(FTransform(FRotator(0.f, 37.f, 0.f), FVector(800.f, -500.f, 100.f), FVector(1.5f)));
	const FSokobanLevelDefinition Demo = ASokobanGameMode::MakeDemoLevel();
	if (!TestTrue(TEXT("使用地图中已有棋盘"), Mode->InitializeLevel(Demo))) { return false; }
	TestTrue(TEXT("复用而不另建棋盘"), Mode->GetBoard() == Board);
	TestTrue(TEXT("格子坐标随棋盘整体变换"), Board->GetPlayerVisualLocation().Equals(
		Board->GetActorTransform().TransformPosition(FVector(-450.f, 300.f, 40.f))));
	TArray<UStaticMeshComponent*> Before;
	Board->GetComponents(Before);
	TestEqual(TEXT("三个静态批次、玩家和两个箱子"), Before.Num(), 6);
	TestTrue(TEXT("第二次初始化"), Mode->InitializeLevel(Demo));
	TArray<UStaticMeshComponent*> After;
	Board->GetComponents(After);
	TestEqual(TEXT("重建不累积组件"), After.Num(), Before.Num());
	for (UStaticMeshComponent* Component : After)
	{
		if (UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component))
		{
			if (Component->GetFName() == TEXT("Floors")) { TestEqual(TEXT("地面实例不重复"), Instances->GetInstanceCount(), 35); }
			if (Component->GetFName() == TEXT("Walls")) { TestEqual(TEXT("墙体数量"), Instances->GetInstanceCount(), 20); }
			if (Component->GetFName() == TEXT("Goals")) { TestEqual(TEXT("目标数量"), Instances->GetInstanceCount(), 2); }
		}
	}
	const FVector BeforeInvalidSync = Board->GetPlayerVisualLocation();
	FSokobanBoardState InvalidState = Mode->GetSession()->GetState();
	InvalidState.PlayerPosition = FIntPoint(-1, -1);
	TestFalse(TEXT("拒绝错误状态"), Board->SynchronizeState(InvalidState));
	TestTrue(TEXT("错误状态不改变画面"), Board->GetPlayerVisualLocation().Equals(BeforeInvalidSync));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanCameraFitTest,
	"Sokoban.Gameplay.CameraFitsBoard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanCameraFitTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	ASokobanGameMode* Mode = TestWorld.World->SpawnActor<ASokobanGameMode>();
	ASokobanCameraPawn* Camera = TestWorld.World->SpawnActor<ASokobanCameraPawn>();
	if (!TestNotNull(TEXT("创建 GameMode"), Mode) || !TestNotNull(TEXT("创建镜头"), Camera)) { return false; }
	if (!TestTrue(TEXT("初始化棋盘"), Mode->InitializeLevel(ASokobanGameMode::MakeDemoLevel()))) { return false; }
	ASokobanBoardActor* Board = Mode->GetBoard();
	Board->SetActorTransform(FTransform(FRotator(0.f, 25.f, 0.f), FVector(1000.f, 200.f, 70.f), FVector(1.3f)));
	Camera->SetBoard(Board);
	for (const float Aspect : {16.f / 9.f, 9.f / 16.f})
	{
		Camera->FrameBoard(Aspect);
		const FBox Bounds = Board->GetBoardBounds();
		const double TanHalfHorizontal = FMath::Tan(FMath::DegreesToRadians(Camera->Camera->FieldOfView * 0.5));
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector WorldCorner((Corner & 1) ? Bounds.Max.X : Bounds.Min.X,
				(Corner & 2) ? Bounds.Max.Y : Bounds.Min.Y, (Corner & 4) ? Bounds.Max.Z : Bounds.Min.Z);
			const FVector View = Camera->GetActorTransform().InverseTransformPosition(WorldCorner);
			TestTrue(TEXT("棋盘在镜头前方"), View.X > 0.0);
			TestTrue(TEXT("水平范围未裁切"), FMath::Abs(View.Y) < View.X * TanHalfHorizontal);
			TestTrue(TEXT("垂直范围未裁切"), FMath::Abs(View.Z) < View.X * TanHalfHorizontal / Aspect);
		}
		const FVector Center = Camera->GetActorTransform().InverseTransformPosition(Board->GridToWorld(FIntPoint(2, 2)));
		const FVector Up = Camera->GetActorTransform().InverseTransformPosition(Board->GridToWorld(FIntPoint(2, 1)));
		const FVector Right = Camera->GetActorTransform().InverseTransformPosition(Board->GridToWorld(FIntPoint(3, 2)));
		TestTrue(TEXT("网格向上对应屏幕向上"), Up.Z / Up.X > Center.Z / Center.X);
		TestTrue(TEXT("网格向右对应屏幕向右"), Right.Y / Right.X > Center.Y / Center.X);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSokobanInputConfigurationTest,
	"Sokoban.Gameplay.InputDirectionsAndConfiguration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSokobanInputConfigurationTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("正 Y 为上"), ASokobanPlayerController::ResolveGridDirection(FVector2D(0, 1)) == ESokobanDirection::Up);
	TestTrue(TEXT("负 Y 为下"), ASokobanPlayerController::ResolveGridDirection(FVector2D(0, -1)) == ESokobanDirection::Down);
	TestTrue(TEXT("负 X 为左"), ASokobanPlayerController::ResolveGridDirection(FVector2D(-1, 0)) == ESokobanDirection::Left);
	TestTrue(TEXT("正 X 为右"), ASokobanPlayerController::ResolveGridDirection(FVector2D(1, 0)) == ESokobanDirection::Right);
	TestTrue(TEXT("摇杆微小偏移不移动"), ASokobanPlayerController::ResolveGridDirection(FVector2D(.2, .1)) == ESokobanDirection::None);
	TestTrue(TEXT("斜向取较大分量"), ASokobanPlayerController::ResolveGridDirection(FVector2D(.6, .9)) == ESokobanDirection::Up);
	TestTrue(TEXT("斜向相等时稳定选择水平"), ASokobanPlayerController::ResolveGridDirection(FVector2D(1, 1)) == ESokobanDirection::Right);
	FTestWorld TestWorld;
	ASokobanPlayerController* Controller = TestWorld.World->SpawnActor<ASokobanPlayerController>();
	if (!TestNotNull(TEXT("创建输入控制器"), Controller)) { return false; }
	FText Error;
	TestFalse(TEXT("遗漏资产时报错"), Controller->ValidateInputConfiguration(Error));
	TestFalse(TEXT("提供可读错误信息"), Error.IsEmpty());
	Controller->InputMappingContext = NewObject<UInputMappingContext>(Controller);
	Controller->MoveAction = NewObject<UInputAction>(Controller);
	Controller->MoveAction->ValueType = EInputActionValueType::Axis2D;
	Controller->UndoAction = NewObject<UInputAction>(Controller);
	Controller->RedoAction = NewObject<UInputAction>(Controller);
	Controller->RestartAction = NewObject<UInputAction>(Controller);
	Controller->InputMappingContext->MapKey(Controller->MoveAction, EKeys::D);
	Controller->InputMappingContext->MapKey(Controller->UndoAction, EKeys::Z);
	Controller->InputMappingContext->MapKey(Controller->RedoAction, EKeys::Y);
	TestFalse(TEXT("缺少按键映射时报错"), Controller->ValidateInputConfiguration(Error));
	Controller->InputMappingContext->MapKey(Controller->RestartAction, EKeys::R);
	TestTrue(TEXT("完整配置通过"), Controller->ValidateInputConfiguration(Error));
	TestTrue(TEXT("成功时清除旧错误"), Error.IsEmpty());
	Controller->MoveAction->ValueType = EInputActionValueType::Boolean;
	TestFalse(TEXT("移动资产类型错误时报错"), Controller->ValidateInputConfiguration(Error));
	Controller->MoveAction->ValueType = EInputActionValueType::Axis2D;
	Controller->RedoAction = Controller->UndoAction;
	TestFalse(TEXT("重复使用同一动作资产时报错"), Controller->ValidateInputConfiguration(Error));
	return true;
}

#endif
