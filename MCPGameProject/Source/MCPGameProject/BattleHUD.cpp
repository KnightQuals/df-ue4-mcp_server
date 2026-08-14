// Copyright Epic Games, Inc. All Rights Reserved.

#include "BattleHUD.h"
#include "BattleGameState.h"
#include "BattleSectorAnchor.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"

const FLinearColor ABattleHUD::AttackerColor(1.f, 0.32f, 0.27f);  // red, Chinese convention
const FLinearColor ABattleHUD::DefenderColor(0.31f, 0.55f, 1.f);  // blue

void ABattleHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine)
	{
		return;
	}

	if (ABattleGameState* State = GetWorld() ? GetWorld()->GetGameState<ABattleGameState>() : nullptr)
	{
		DrawConquestPanel(State);
		DrawResultBanner(State);
	}
	DrawCaptureBar();
}

void ABattleHUD::DrawConquestPanel(ABattleGameState* State)
{
	UFont* Font = GEngine->GetMediumFont();
	if (!Font)
	{
		return;
	}

	const float CX = Canvas->SizeX * 0.5f;
	// Demo playtest feedback: the 1.6/1.2/0.72 stack was unreadably small at PIE
	// window size. Bump everything ~1.5x.
	const float Scale = 2.4f;
	const float SubScale = 1.7f;
	const float SmallScale = 0.95f;

	// Cumulative score across the completed bell rounds, in colored segments.
	const FString Left = FString::Printf(TEXT("ATTACKERS  %d"), State->AttackerScore);
	const FString Mid = TEXT(" : ");
	const FString Right = FString::Printf(TEXT("%d  DEFENDERS"), State->DefenderScore);

	float LW, LH, MW, MH, RW, RH;
	Canvas->TextSize(Font, Left, LW, LH, Scale, Scale);
	Canvas->TextSize(Font, Mid, MW, MH, Scale, Scale);
	Canvas->TextSize(Font, Right, RW, RH, Scale, Scale);
	const float TotalW = LW + MW + RW;
	const float LineY = 18.f;

	// Translucent backing strip so the four status lines stay readable over bright terrain.
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.42f), CX - TotalW * 0.5f - 22.f, LineY - 8.f,
		TotalW + 44.f, LH + 150.f);

	DrawText(Left, AttackerColor, CX - TotalW * 0.5f, LineY, Font, Scale);
	DrawText(Mid, FLinearColor::White, CX - TotalW * 0.5f + LW, LineY, Font, Scale);
	DrawText(Right, DefenderColor, CX - TotalW * 0.5f + LW + MW, LineY, Font, Scale);

	// Current-round countdown: ownership is sampled only when this reaches zero.
	const int32 Seconds = FMath::Max(0, State->RemainingMatchSeconds);
	const FString TimeText = FString::Printf(TEXT("ROUND %d / %d    TIME %02d:%02d"),
		State->CurrentRound, State->TotalRounds, Seconds / 60, Seconds % 60);
	float TW, TH;
	Canvas->TextSize(Font, TimeText, TW, TH, SubScale, SubScale);
	const FLinearColor TimeColor = Seconds <= 10 ? FLinearColor(1.f, 0.85f, 0.2f) : FLinearColor::White;
	DrawText(TimeText, TimeColor, CX - TW * 0.5f, LineY + LH + 10.f, Font, SubScale);

	// State the scoring rule on-screen; it avoids the previous ambiguity of scores
	// ticking every second while a team was merely holding a sector.
	const FString RuleText = TEXT("BELL SCORE: +1 FOR EACH HELD SECTOR AT ROUND END");
	float RuleW, RuleH;
	Canvas->TextSize(Font, RuleText, RuleW, RuleH, SmallScale, SmallScale);
	DrawText(RuleText, FLinearColor(1.f, 1.f, 1.f, 0.62f), CX - RuleW * 0.5f, LineY + LH + 10.f + TH + 8.f, Font, SmallScale);

	if (State->CurrentRound > 1 || State->bConquestOver)
	{
		const FString LastText = FString::Printf(TEXT("LAST BELL  +%d ATTACKERS  :  +%d DEFENDERS"),
			State->LastRoundAtkGain, State->LastRoundDefGain);
		float BW, BH;
		Canvas->TextSize(Font, LastText, BW, BH, SmallScale, SmallScale);
		DrawText(LastText, FLinearColor(1.f, 1.f, 1.f, 0.78f), CX - BW * 0.5f,
			LineY + LH + 10.f + TH + RuleH + 16.f, Font, SmallScale);
	}
}

void ABattleHUD::DrawResultBanner(ABattleGameState* State)
{
	if (!State->bConquestOver)
	{
		return;
	}
	UFont* Font = GEngine->GetLargeFont();
	if (!Font)
	{
		return;
	}

	FString Text;
	FLinearColor Color = FLinearColor::White;
	if (State->ConquestWinner == 0)
	{
		Text = TEXT("ATTACKERS WIN");
		Color = AttackerColor;
	}
	else if (State->ConquestWinner == 1)
	{
		Text = TEXT("DEFENDERS WIN");
		Color = DefenderColor;
	}
	else
	{
		Text = TEXT("DRAW");
	}

	const float Scale = 2.6f;
	float TW, TH;
	Canvas->TextSize(Font, Text, TW, TH, Scale, Scale);
	const float X = Canvas->SizeX * 0.5f - TW * 0.5f;
	const float Y = Canvas->SizeY * 0.34f - TH * 0.5f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), X - 24.f, Y - 14.f, TW + 48.f, TH + 28.f);
	DrawText(Text, Color, X, Y, Font, Scale);
}

ABattleSectorAnchor* ABattleHUD::FindRelevantAnchor() const
{
	const APawn* Pawn = GetOwningPawn();
	if (!Pawn)
	{
		return nullptr;
	}
	const FVector Loc = Pawn->GetActorLocation();
	ABattleSectorAnchor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<ABattleSectorAnchor> It(GetWorld()); It; ++It)
	{
		ABattleSectorAnchor* Anchor = *It;
		if (!Anchor || !Anchor->bIsActive)
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(Loc, Anchor->GetActorLocation());
		if (DistSq <= Anchor->CaptureRadius * Anchor->CaptureRadius && DistSq < BestDistSq)
		{
			Best = Anchor;
			BestDistSq = DistSq;
		}
	}
	return Best;
}

void ABattleHUD::DrawCaptureBar()
{
	ABattleSectorAnchor* Anchor = FindRelevantAnchor();
	if (!Anchor)
	{
		return;
	}
	UFont* Font = GEngine->GetMediumFont();
	if (!Font)
	{
		return;
	}

	// Tug-of-war bar: CaptureProgress in [-1,1] maps to a red/blue split.
	const float Frac = FMath::Clamp((Anchor->CaptureProgress + 1.f) * 0.5f, 0.f, 1.f);
	const float BarW = 420.f;
	const float BarH = 16.f;
	const float BarX = Canvas->SizeX * 0.5f - BarW * 0.5f;
	const float BarY = Canvas->SizeY * 0.80f;

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), BarX - 3.f, BarY - 3.f, BarW + 6.f, BarH + 6.f);
	DrawRect(AttackerColor, BarX, BarY, BarW * Frac, BarH);
	DrawRect(DefenderColor, BarX + BarW * Frac, BarY, BarW * (1.f - Frac), BarH);
	// Centre tick marks the neutral point.
	DrawRect(FLinearColor::White, BarX + BarW * 0.5f - 1.f, BarY - 3.f, 2.f, BarH + 6.f);

	// Status line above the bar, driven by who is actually in the zone right now.
	FString Status;
	FLinearColor StatusColor = FLinearColor::White;
	const int32 Atk = Anchor->AttackersInZone;
	const int32 Def = Anchor->DefendersInZone;
	const int32 SectorNo = Anchor->SectorIndex + 1;
	if (Atk > 0 && Def > 0)
	{
		Status = FString::Printf(TEXT("SECTOR %d - CONTESTED"), SectorNo);
		StatusColor = FLinearColor(1.f, 0.85f, 0.2f);
	}
	else if (Atk > Def)
	{
		Status = FString::Printf(TEXT("SECTOR %d - CAPTURING..."), SectorNo);
		StatusColor = AttackerColor;
	}
	else if (Def > Atk)
	{
		Status = FString::Printf(TEXT("SECTOR %d - DEFENDING..."), SectorNo);
		StatusColor = DefenderColor;
	}
	else
	{
		Status = FString::Printf(TEXT("SECTOR %d - HELD BY %s"), SectorNo,
			Anchor->OwningTeam == 0 ? TEXT("ATTACKERS") : TEXT("DEFENDERS"));
	}
	float SW, SH;
	Canvas->TextSize(Font, Status, SW, SH, 1.1f, 1.1f);
	DrawText(Status, StatusColor, Canvas->SizeX * 0.5f - SW * 0.5f, BarY - SH - 8.f, Font, 1.1f);
}
