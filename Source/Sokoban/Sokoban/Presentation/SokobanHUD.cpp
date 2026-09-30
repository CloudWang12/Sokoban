#include "SokobanHUD.h"

#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Sokoban/Gameplay/SokobanGameMode.h"
#include "Sokoban/Gameplay/SokobanPlayerController.h"

void ASokobanHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	const ASokobanGameMode* Mode = GetWorld()->GetAuthGameMode<ASokobanGameMode>();
	if (!Mode || Mode->IsFlowManaged())
	{
		return;
	}
	// 暂用引擎默认字体与英文短句，避免灰盒阶段依赖额外中文字体资产。
	const float Scale = FMath::Clamp(Canvas->SizeX / 1000.f, 0.7f, 1.2f);
	const float X = 20.f;
	float Y = 20.f;
	DrawRect(FLinearColor(0.02f, 0.03f, 0.05f, 0.85f), 10.f, 10.f, Canvas->SizeX - 20.f, 112.f * Scale);
	auto Line = [&](const FString& Text, const FLinearColor& Color)
	{
		DrawText(Text, Color, X, Y, nullptr, Scale);
		Y += 22.f * Scale;
	};
	Line(TEXT("WASD / Arrows: Move | Z: Undo | Y: Redo | R: Restart"), FLinearColor::White);
	Line(TEXT("One press = one step. Input during movement is ignored."), FLinearColor(0.7f, 0.75f, 0.8f));
	if (const USokobanSession* Session = Mode->GetSession())
	{
		const FSokobanMoveCounters Counts = Session->GetState().Counters;
		Line(FString::Printf(TEXT("Moves: %d    Pushes: %d    Undo: %d    Redo: %d"),
			Counts.MoveCount, Counts.PushCount, Session->GetUndoCount(), Session->GetRedoCount()), FLinearColor::White);
	}
	const ASokobanPlayerController* Controller = Cast<ASokobanPlayerController>(GetOwningPlayerController());
	if (Controller && !Controller->GetInputConfigurationError().IsEmpty())
	{
		Line(Controller->GetInputConfigurationError().ToString(), FLinearColor(1.f, 0.4f, 0.25f));
	}
	else
	{
		Line(Mode->GetStatusMessage().ToString(), Mode->IsCompletionVisible() ? FLinearColor::Green : FLinearColor::White);
	}
}
