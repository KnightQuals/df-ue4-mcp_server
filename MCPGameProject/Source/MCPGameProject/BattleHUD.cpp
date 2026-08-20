// Copyright Epic Games, Inc. All Rights Reserved.

#include "BattleHUD.h"
#include "BattleGameState.h"
#include "BattleSectorAnchor.h"
#include "BreakthroughCharacter.h"
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
	DrawMinimap();
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
	// Screen text is Chinese (Slate falls back to DroidSansFallback for CJK glyphs).
	const FString Left = FString::Printf(TEXT("进攻方  %d"), State->AttackerScore);
	const FString Mid = TEXT(" : ");
	const FString Right = FString::Printf(TEXT("%d  防守方"), State->DefenderScore);

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
	const FString TimeText = FString::Printf(TEXT("第 %d / %d 局    倒计时 %02d:%02d"),
		State->CurrentRound, State->TotalRounds, Seconds / 60, Seconds % 60);
	float TW, TH;
	Canvas->TextSize(Font, TimeText, TW, TH, SubScale, SubScale);
	const FLinearColor TimeColor = Seconds <= 10 ? FLinearColor(1.f, 0.85f, 0.2f) : FLinearColor::White;
	DrawText(TimeText, TimeColor, CX - TW * 0.5f, LineY + LH + 10.f, Font, SubScale);

	// State the scoring rule on-screen; it avoids the previous ambiguity of scores
	// ticking every second while a team was merely holding a sector.
	const FString RuleText = TEXT("局末结算：每持有一个据点 +1 分");
	float RuleW, RuleH;
	Canvas->TextSize(Font, RuleText, RuleW, RuleH, SmallScale, SmallScale);
	DrawText(RuleText, FLinearColor(1.f, 1.f, 1.f, 0.62f), CX - RuleW * 0.5f, LineY + LH + 10.f + TH + 8.f, Font, SmallScale);

	if (State->CurrentRound > 1 || State->bConquestOver)
	{
		const FString LastText = FString::Printf(TEXT("上局结算  进攻方 +%d ： 防守方 +%d"),
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
		Text = TEXT("进攻方获胜");
		Color = AttackerColor;
	}
	else if (State->ConquestWinner == 1)
	{
		Text = TEXT("防守方获胜");
		Color = DefenderColor;
	}
	else
	{
		Text = TEXT("平局");
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

	// Tier 1: an active anchor whose capture radius physically contains the player.
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
	if (Best)
	{
		return Best;
	}

	// Tier 2 (user request 2026-08-18): the OWNING team must still see the progress
	// bar when their point is being pushed by the enemy, even from across the map —
	// otherwise a defender retaking "your" captured point happens invisibly.
	const ABreakthroughCharacter* MyChar = Cast<ABreakthroughCharacter>(Pawn);
	if (!MyChar)
	{
		return nullptr;
	}
	const int32 MyTeam = MyChar->Team;
	for (TActorIterator<ABattleSectorAnchor> It(GetWorld()); It; ++It)
	{
		ABattleSectorAnchor* Anchor = *It;
		if (!Anchor || !Anchor->bIsActive || Anchor->OwningTeam != MyTeam)
		{
			continue;
		}
		// Enemies currently hold the numeric advantage inside OUR point.
		const bool bEnemiesPushing = (MyTeam == 0 && Anchor->DefendersInZone > Anchor->AttackersInZone)
			|| (MyTeam == 1 && Anchor->AttackersInZone > Anchor->DefendersInZone);
		if (bEnemiesPushing)
		{
			return Anchor;
		}
	}
	return nullptr;
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

	// Remote view: the bar refers to a point we own but are not standing in
	// (tier 2 above) — flag it so the player understands what they're looking at.
	const APawn* Pawn = GetOwningPawn();
	const bool bRemote = !Pawn
		|| FVector::DistSquared2D(Pawn->GetActorLocation(), Anchor->GetActorLocation())
			> Anchor->CaptureRadius * Anchor->CaptureRadius;

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

	// Status line above the bar. Wording is action-based so it reads correctly both
	// when standing in the zone and when watching your own point being pushed remotely.
	FString Status;
	FLinearColor StatusColor = FLinearColor::White;
	const int32 Atk = Anchor->AttackersInZone;
	const int32 Def = Anchor->DefendersInZone;
	const int32 SectorNo = Anchor->SectorIndex + 1;
	const TCHAR* Prefix = bRemote ? TEXT("【远处】") : TEXT("");
	if (Atk > 0 && Def > 0)
	{
		Status = FString::Printf(TEXT("%s据点 %d - 双方交火"), Prefix, SectorNo);
		StatusColor = FLinearColor(1.f, 0.85f, 0.2f);
	}
	else if (Atk > Def)
	{
		Status = FString::Printf(TEXT("%s据点 %d - 进攻方推进中..."), Prefix, SectorNo);
		StatusColor = AttackerColor;
	}
	else if (Def > Atk)
	{
		Status = FString::Printf(TEXT("%s据点 %d - 防守方回推中..."), Prefix, SectorNo);
		StatusColor = DefenderColor;
	}
	else
	{
		Status = FString::Printf(TEXT("%s据点 %d - 当前由%s控制"), Prefix, SectorNo,
			Anchor->OwningTeam == 0 ? TEXT("进攻方") : TEXT("防守方"));
	}
	float SW, SH;
	Canvas->TextSize(Font, Status, SW, SH, 1.1f, 1.1f);
	DrawText(Status, StatusColor, Canvas->SizeX * 0.5f - SW * 0.5f, BarY - SH - 8.f, Font, 1.1f);
}

void ABattleHUD::DrawMinimap()
{
	const APawn* Pawn = GetOwningPawn();
	if (!Pawn || !Canvas)
	{
		return;
	}

	// Step toggle (plan agreed with the user 2026-08-18): false = north-up, the
	// player dot moves across a fixed map (step 1); true = the map rotates with the
	// player's view so their facing always points up (step 2).
	static const bool bRotateWithView = false;

	// Panel geometry: top-right corner.
	const float MapW = 300.f;
	const float MapH = 260.f;
	const float MapX = Canvas->SizeX - MapW - 16.f;
	const float MapY = 16.f;
	const FVector2D Center(MapX + MapW * 0.5f, MapY + MapH * 0.5f);

	// Show ~110m across the panel width (battlefield is 90m wide) so the whole safe
	// area plus a hint of the forbidden ring stays visible.
	const float PxPerCm = MapW / 11000.f;

	const FVector PlayerLoc = Pawn->GetActorLocation();
	const FVector Fwd = Pawn->GetActorForwardVector();
	const float Ang = bRotateWithView ? FMath::Atan2(Fwd.Y, Fwd.X) : 0.f;
	const float C = FMath::Cos(Ang);
	const float S = FMath::Sin(Ang);

	// World -> panel: rotate the offset by -Ang so the view direction maps to
	// screen-up; world X (north) -> screen up, world Y (east) -> screen right.
	auto WorldToMap = [&](const FVector& WorldPos) -> FVector2D
	{
		const FVector Off = WorldPos - PlayerLoc;
		const float UpComp = Off.X * C + Off.Y * S;
		const float RightComp = -Off.X * S + Off.Y * C;
		return FVector2D(Center.X + RightComp * PxPerCm, Center.Y - UpComp * PxPerCm);
	};

	auto IsInsidePanel = [&](const FVector2D& P) -> bool
	{
		return P.X >= MapX + 1.f && P.X <= MapX + MapW - 1.f
			&& P.Y >= MapY + 1.f && P.Y <= MapY + MapH - 1.f;
	};

	// Axis-aligned-in-world rectangle drawn as four screen lines, so it stays
	// correct when the map rotates. Cheap clipping: skip fully-offscreen segments.
	auto DrawWorldRect = [&](const FVector2D& WorldCenter, const FVector2D& WorldHalf, const FLinearColor& Color, float Thickness)
	{
		const FVector Corners[4] =
		{
			FVector(WorldCenter.X - WorldHalf.X, WorldCenter.Y - WorldHalf.Y, 0.f),
			FVector(WorldCenter.X + WorldHalf.X, WorldCenter.Y - WorldHalf.Y, 0.f),
			FVector(WorldCenter.X + WorldHalf.X, WorldCenter.Y + WorldHalf.Y, 0.f),
			FVector(WorldCenter.X - WorldHalf.X, WorldCenter.Y + WorldHalf.Y, 0.f),
		};
		FVector2D Prev = WorldToMap(Corners[3]);
		for (int32 Idx = 0; Idx < 4; ++Idx)
		{
			const FVector2D Cur = WorldToMap(Corners[Idx]);
			if (IsInsidePanel(Prev) || IsInsidePanel(Cur))
			{
				DrawLine(Prev.X, Prev.Y, Cur.X, Cur.Y, Color, Thickness);
			}
			Prev = Cur;
		}
	};

	// ---- Background "floor plan" of the level: dark forbidden frame with a
	// safe-area "ground" fill on top. The non-safe border around the ground reads
	// as the "off-map" zone instead of an abstract rectangle.
	DrawRect(FLinearColor(0.18f, 0.05f, 0.05f, 0.85f), MapX, MapY, MapW, MapH);
	const FVector2D SafeHalf(4500.f, 3600.f);
	const float SafeW = SafeHalf.X * 2.f * PxPerCm;
	const float SafeH = SafeHalf.Y * 2.f * PxPerCm;
	const float SafeX = Center.X - SafeW * 0.5f;
	const float SafeY = Center.Y - SafeH * 0.5f;
	DrawRect(FLinearColor(0.18f, 0.24f, 0.16f, 0.95f), SafeX, SafeY, SafeW, SafeH);
	// Subtle inner grid to suggest terrain (every 1500cm -> ~33px at this scale).
	const FLinearColor GridColor(0.22f, 0.28f, 0.20f, 0.55f);
	for (float Gx = 1500.f; Gx < SafeHalf.X; Gx += 1500.f)
	{
		const float Px1 = Center.X + Gx * PxPerCm;
		const float Px2 = Center.X - Gx * PxPerCm;
		DrawLine(Px1, SafeY, Px1, SafeY + SafeH, GridColor, 1.f);
		DrawLine(Px2, SafeY, Px2, SafeY + SafeH, GridColor, 1.f);
	}
	for (float Gy = 1500.f; Gy < SafeHalf.Y; Gy += 1500.f)
	{
		const float Py1 = Center.Y + Gy * PxPerCm;
		const float Py2 = Center.Y - Gy * PxPerCm;
		DrawLine(SafeX, Py1, SafeX + SafeW, Py1, GridColor, 1.f);
		DrawLine(SafeX, Py2, SafeX + SafeW, Py2, GridColor, 1.f);
	}
	// Safe-area border (a single faint green outline — looks like a level boundary).
	const FLinearColor BorderColor(0.4f, 0.7f, 0.35f, 0.9f);
	DrawLine(SafeX, SafeY, SafeX + SafeW, SafeY, BorderColor, 1.5f);
	DrawLine(SafeX + SafeW, SafeY, SafeX + SafeW, SafeY + SafeH, BorderColor, 1.5f);
	DrawLine(SafeX + SafeW, SafeY + SafeH, SafeX, SafeY + SafeH, BorderColor, 1.5f);
	DrawLine(SafeX, SafeY + SafeH, SafeX, SafeY, BorderColor, 1.5f);

	// ---- Spawn hubs: small flag (pole + head) plus a Chinese label.
	// The hub world positions are fixed by the GameMode layout, hardcoded on client.
	UFont* Font = GEngine->GetMediumFont();
	if (Font)
	{
		auto DrawSpawnHub = [&](const FVector& WorldPos, const FString& Label, const FLinearColor& Color)
		{
			const FVector2D P = WorldToMap(WorldPos);
			if (!IsInsidePanel(P))
			{
				return;
			}
			// Flagpole.
			DrawLine(P.X, P.Y - 6.f, P.X, P.Y + 6.f, Color, 1.5f);
			// Flag head (small filled triangle via three lines).
			DrawLine(P.X, P.Y - 6.f, P.X + 5.f, P.Y - 3.f, Color, 1.5f);
			DrawLine(P.X + 5.f, P.Y - 3.f, P.X, P.Y, Color, 1.5f);
			// Base dot so the pole doesn't look like it floats.
			DrawRect(Color, P.X - 2.f, P.Y + 6.f, 5.f, 2.f);
			// Label below the flag.
			float LW, LH;
			Canvas->TextSize(Font, Label, LW, LH, 0.85f, 0.85f);
			DrawText(Label, FLinearColor(1.f, 1.f, 1.f, 0.9f), P.X - LW * 0.5f, P.Y + 10.f, Font, 0.85f);
		};
		DrawSpawnHub(FVector(-3375.f, 0.f, 0.f), TEXT("攻方"), AttackerColor);
		DrawSpawnHub(FVector(3375.f, 0.f, 0.f), TEXT("守方"), DefenderColor);
	}

	// ---- Sector markers: filled diamond + ①/② label, owner color, yellow ring
	// when the sector is in transition.
	for (TActorIterator<ABattleSectorAnchor> It(GetWorld()); It; ++It)
	{
		const ABattleSectorAnchor* Anchor = *It;
		if (!Anchor || !Anchor->bIsActive)
		{
			continue;
		}
		const FVector2D P = WorldToMap(Anchor->GetActorLocation());
		if (!IsInsidePanel(P))
		{
			continue;
		}
		const FLinearColor OwnerColor = Anchor->OwningTeam == 0 ? AttackerColor : DefenderColor;
		const float R = 7.f;
		// Filled diamond: 4 lines closing back to top.
		DrawLine(P.X, P.Y - R, P.X + R, P.Y, OwnerColor, 2.f);
		DrawLine(P.X + R, P.Y, P.X, P.Y + R, OwnerColor, 2.f);
		DrawLine(P.X, P.Y + R, P.X - R, P.Y, OwnerColor, 2.f);
		DrawLine(P.X - R, P.Y, P.X, P.Y - R, OwnerColor, 2.f);
		// Solid core dot so the marker reads even at small size.
		DrawRect(OwnerColor, P.X - 2.f, P.Y - 2.f, 4.f, 4.f);
		const bool bInTransition = FMath::Abs(Anchor->CaptureProgress) < 1.f;
		if (bInTransition)
		{
			const FLinearColor Ring(1.f, 0.85f, 0.2f, 0.95f);
			DrawLine(P.X - R - 3.f, P.Y - R - 3.f, P.X + R + 3.f, P.Y - R - 3.f, Ring, 1.5f);
			DrawLine(P.X + R + 3.f, P.Y - R - 3.f, P.X + R + 3.f, P.Y + R + 3.f, Ring, 1.5f);
			DrawLine(P.X + R + 3.f, P.Y + R + 3.f, P.X - R - 3.f, P.Y + R + 3.f, Ring, 1.5f);
			DrawLine(P.X - R - 3.f, P.Y + R + 3.f, P.X - R - 3.f, P.Y - R - 3.f, Ring, 1.5f);
		}
		// ① / ② label above the diamond (CJK circled digits; CJK font fallback covers them).
		if (Font)
		{
			const FString Label = Anchor->SectorIndex == 0 ? TEXT("①") : TEXT("②");
			float LW, LH;
			Canvas->TextSize(Font, Label, LW, LH, 0.9f, 0.9f);
			DrawText(Label, FLinearColor(1.f, 1.f, 1.f, 0.95f),
				P.X - LW * 0.5f, P.Y - R - 4.f - LH, Font, 0.9f);
		}
	}

	// ---- Other characters: 5px team dots ----
	for (TActorIterator<ABreakthroughCharacter> It(GetWorld()); It; ++It)
	{
		const ABreakthroughCharacter* Other = *It;
		if (!Other || Other == Pawn || Other->bIsEliminated)
		{
			continue;
		}
		const FVector2D P = WorldToMap(Other->GetActorLocation());
		if (!IsInsidePanel(P))
		{
			continue;
		}
		const FLinearColor DotColor = Other->Team == 0 ? AttackerColor : DefenderColor;
		DrawRect(DotColor, P.X - 2.5f, P.Y - 2.5f, 5.f, 5.f);
	}

	// ---- Local player: direction triangle at the panel center ----
	{
		FVector2D Dir(0.f, -1.f);
		if (!bRotateWithView)
		{
			const FVector2D WorldDir(Fwd.Y, -Fwd.X);
			if (WorldDir.SizeSquared() > KINDA_SMALL_NUMBER)
			{
				Dir = WorldDir.GetSafeNormal();
			}
		}
		const FVector2D Perp(-Dir.Y, Dir.X);
		const FVector2D TipPos = Center + Dir * 11.f;
		const FVector2D LeftPos = Center - Dir * 7.f + Perp * 5.f;
		const FVector2D RightPos = Center - Dir * 7.f - Perp * 5.f;
		const FLinearColor SelfColor(1.f, 1.f, 1.f, 0.95f);
		DrawLine(TipPos.X, TipPos.Y, LeftPos.X, LeftPos.Y, SelfColor, 2.f);
		DrawLine(LeftPos.X, LeftPos.Y, RightPos.X, RightPos.Y, SelfColor, 2.f);
		DrawLine(RightPos.X, RightPos.Y, TipPos.X, TipPos.Y, SelfColor, 2.f);
	}

	// ---- Title ----
	if (Font)
	{
		DrawText(TEXT("小地图"), FLinearColor(1.f, 1.f, 1.f, 0.6f), MapX + 6.f, MapY + 4.f, Font, 0.8f);
	}
}
