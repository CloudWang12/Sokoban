#include "SokobanGameMode.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Sokoban/Flow/SokobanFlowSubsystem.h"
#include "EngineUtils.h"
#include "SokobanPlayerController.h"
#include "Sokoban/Data/SokobanLevelData.h"
#include "Sokoban/Presentation/SokobanBoardActor.h"
#include "Sokoban/Presentation/SokobanCameraPawn.h"
#include "Sokoban/Presentation/SokobanHUD.h"

DEFINE_LOG_CATEGORY_STATIC(LogSokobanGameplay, Log, All);

ASokobanGameMode::ASokobanGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PlayerControllerClass = ASokobanPlayerController::StaticClass();
	DefaultPawnClass = ASokobanCameraPawn::StaticClass();
	HUDClass = ASokobanHUD::StaticClass();
	BoardClass = ASokobanBoardActor::StaticClass();
}

void ASokobanGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (bEnableFrontend && GetGameInstance())
	{
		Flow = GetGameInstance()->GetSubsystem<USokobanFlowSubsystem>();
		if (Flow.IsValid())
		{
			bFlowAttached = true;
			Flow->Attach(this, LevelCatalog);
			return;
		}
	}
	if (StartupLevel)
	{
		InitializeLevel(StartupLevel->Definition);
	}
	else if (bUseDemoLevelIfUnset)
	{
		InitializeLevel(MakeDemoLevel());
	}
	else
	{
		SetFailure(FText::FromString(TEXT("No Startup Level assigned.")));
	}
}

void ASokobanGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bFlowAttached && Flow.IsValid()) { Flow->TryComplete(); }
}

bool ASokobanGameMode::CanAcceptPlayerInput() const
{
	return !bFlowAttached || (Flow.IsValid() && Flow->CanAcceptGameplay(this));
}

void ASokobanGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Flow.IsValid()) { Flow->Detach(this); }
	Flow.Reset();
	bFlowAttached = false;
	if (Session)
	{
		Session->OnSessionChangedNative().Remove(SessionHandle);
	}
	Super::EndPlay(EndPlayReason);
}

bool ASokobanGameMode::EnsureBoard()
{
	if (IsValid(Board))
	{
		return true;
	}
	ASokobanBoardActor* Found = nullptr;
	for (TActorIterator<ASokobanBoardActor> It(GetWorld()); It; ++It)
	{
		if (Found)
		{
			SetFailure(FText::FromString(TEXT("Keep only one Sokoban Board Actor in the map.")));
			return false;
		}
		Found = *It;
	}
	if (!Found && BoardClass)
	{
		FActorSpawnParameters Parameters;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Found = GetWorld()->SpawnActor<ASokobanBoardActor>(BoardClass, FTransform::Identity, Parameters);
	}
	Board = Found;
	if (!IsValid(Board))
	{
		SetFailure(FText::FromString(TEXT("Unable to create the board. Check Board Class.")));
		return false;
	}
	return true;
}

bool ASokobanGameMode::InitializeLevel(const FSokobanLevelDefinition& Level)
{
	if (bFlowAttached && (!Flow.IsValid() || !Flow->IsStartingLevel())) { return false; }
	if (IsPresentationBusy() || !EnsureBoard())
	{
		return false;
	}
	if (!Session)
	{
		Session = NewObject<USokobanSession>(this);
		SessionHandle = Session->OnSessionChangedNative().AddUObject(this, &ASokobanGameMode::HandleSessionChanged);
	}

	// FSokobanValidationIssue 保存具体的关卡错误；校验失败时 Session 保留旧对局。
	TArray<FSokobanValidationIssue> Issues;
	if (!Session->Initialize(Level, Issues))
	{
		FString Details = TEXT("Level initialization failed.");
		for (const FSokobanValidationIssue& Issue : Issues)
		{
			Details += TEXT("\n") + Issue.Message.ToString();
		}
		StatusMessage = FText::FromString(Details);
		UE_LOG(LogSokobanGameplay, Warning, TEXT("%s"), *Details);
		return false;
	}
	// Initialize 同步广播事件；棋盘构建结果已经由 HandleSessionChanged 写入 bReady。
	if (bReady)
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* Controller = It->Get())
			{
				if (ASokobanCameraPawn* Camera = Cast<ASokobanCameraPawn>(Controller->GetPawn()))
				{
					Camera->SetBoard(Board);
				}
			}
		}
	}
	return bReady;
}

void ASokobanGameMode::HandleSessionChanged(const FSokobanSessionUpdate& Update)
{
	// FSokobanSessionUpdate 说明本次变化的来源；FSokobanBoardState 是已提交的最终状态。
	// 表现层只读取它们，不通过 Actor 位置反推游戏规则。
	if (!IsValid(Board))
	{
		SetFailure(FText::FromString(TEXT("The board is missing.")));
		return;
	}
	const FSokobanBoardState State = Session->GetState();
	bool bPresented = false;
	switch (Update.Change)
	{
	case ESokobanSessionChange::Initialized:
		bPresented = Board->BuildBoard(Session->GetLevelDefinition(), State);
		break;
	case ESokobanSessionChange::Restart:
		bPresented = Board->SynchronizeState(State);
		break;
	case ESokobanSessionChange::Move:
	case ESokobanSessionChange::Undo:
	case ESokobanSessionChange::Redo:
		// Undo 的记录仍是原来的正向记录，只在此处选择反向播放。
		bPresented = Board->PresentMove(Update.Record, Update.Change == ESokobanSessionChange::Undo, State);
		if (!bPresented)
		{
			bPresented = Board->SynchronizeState(State);
		}
		break;
	default:
		break;
	}
	bReady = bPresented;
	if (!bPresented)
	{
		SetFailure(FText::FromString(TEXT("Unable to display the board. Check its settings.")));
	}
	else
	{
		StatusMessage = FText::GetEmpty();
	}
}

FSokobanMoveResult ASokobanGameMode::RequestMove(ESokobanDirection Direction)
{
	FSokobanMoveResult Result;
	Result.Outcome = ESokobanMoveOutcome::InvalidState;
	// Session 已到达逻辑终点，但画面可能仍在插值中；这里统一限制所有玩家操作。
	if (!CanAcceptPlayerInput() || !bReady || !Session || !IsValid(Board) || IsPresentationBusy())
	{
		return Result;
	}
	Result = Session->RequestMove(Direction);
	switch (Result.Outcome)
	{
	case ESokobanMoveOutcome::BlockedByTerrain:
	case ESokobanMoveOutcome::OutOfBounds:
		StatusMessage = FText::FromString(TEXT("Blocked by terrain."));
		break;
	case ESokobanMoveOutcome::BlockedByBox:
		StatusMessage = FText::FromString(TEXT("Cannot push two boxes or push into an occupied cell."));
		break;
	default:
		break;
	}
	return Result;
}

bool ASokobanGameMode::RequestUndo()
{
	return CanAcceptPlayerInput() && bReady && Session && IsValid(Board) && !IsPresentationBusy() && Session->Undo();
}

bool ASokobanGameMode::RequestRedo()
{
	return CanAcceptPlayerInput() && bReady && Session && IsValid(Board) && !IsPresentationBusy() && Session->Redo();
}

bool ASokobanGameMode::RequestRestart()
{
	return CanAcceptPlayerInput() && bReady && Session && IsValid(Board) && !IsPresentationBusy() && Session->Restart();
}

bool ASokobanGameMode::IsPresentationBusy() const
{
	return IsValid(Board) && Board->IsAnimating();
}

bool ASokobanGameMode::IsCompletionVisible() const
{
	return bReady && Session && IsValid(Board) && Session->IsSolved() && !IsPresentationBusy();
}

FText ASokobanGameMode::GetStatusMessage() const
{
	if (!bReady)
	{
		return StatusMessage;
	}
	if (IsPresentationBusy())
	{
		return FText::FromString(TEXT("Moving..."));
	}
	if (IsCompletionVisible())
	{
		return bFlowAttached ? FText::FromString(TEXT("关卡完成")) : FText::FromString(TEXT("Solved! Z: undo | R: restart"));
	}
	return StatusMessage.IsEmpty() ? FText::FromString(TEXT("Ready")) : StatusMessage;
}

void ASokobanGameMode::SetFailure(const FText& Message)
{
	bReady = false;
	StatusMessage = Message;
	UE_LOG(LogSokobanGameplay, Warning, TEXT("%s"), *Message.ToString());
}

FSokobanLevelDefinition ASokobanGameMode::MakeDemoLevel()
{
	FSokobanLevelDefinition Level;
	Level.Width = 7;
	Level.Height = 5;
	Level.Terrain.Init(ESokobanTerrain::Floor, Level.Width * Level.Height);
	for (int32 Y = 0; Y < Level.Height; ++Y)
	{
		for (int32 X = 0; X < Level.Width; ++X)
		{
			if (X == 0 || Y == 0 || X == Level.Width - 1 || Y == Level.Height - 1)
			{
				Level.Terrain[Y * Level.Width + X] = ESokobanTerrain::Wall;
			}
		}
	}
	Level.PlayerSpawn = FIntPoint(2, 3);
	Level.Goals = {FIntPoint(2, 1), FIntPoint(4, 1)};
	FSokobanBoxDefinition FirstBox;
	FirstBox.BoxId = 0;
	FirstBox.Position = FIntPoint(2, 2);
	FSokobanBoxDefinition SecondBox;
	SecondBox.BoxId = 1;
	SecondBox.Position = FIntPoint(4, 2);
	Level.Boxes = {FirstBox, SecondBox};
	return Level;
}
