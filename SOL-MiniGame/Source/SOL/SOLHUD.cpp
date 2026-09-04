// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOLHUD.h"
#include "SOLBackpackComponent.h"
#include "SOLCharacter.h"
#include "SOLContainer.h"
#include "SOLExtractionZone.h"
#include "SOLHealthComponent.h"
#include "SOLPlayerState.h"
#include "SOLGameState.h"
#include "SOLScavenger.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"

// Panel geometry (enlarged 2026-08-25 per user feedback: the original 340x34
// rows were unreadable). Rows are hot-rect clickable, so every geometry
// change must keep DrawRect/DrawText/Hot.Box on the same numbers.
namespace
{
	constexpr float PanelWidth = 560.f;
	constexpr float PanelTitleHeight = 56.f;
	constexpr float PanelRowHeight = 52.f;
	constexpr float PanelRowStep = 54.f;
	constexpr float RowTextScale = 1.35f;
	constexpr float TitleTextScale = 1.5f;
	constexpr float PanelSideMargin = 40.f;
	constexpr float PanelTextPaddingX = 18.f;
}

void ASOLHUD::DrawHUD()
{
	Super::DrawHUD();

	ContainerHotRects.Reset();
	BackpackHotRects.Reset();

	if (!Canvas)
	{
		return;
	}
	UFont* Font = GEngine->GetMediumFont();
	if (!Font)
	{
		return;
	}

	if (FlashMessageTime > 0.f)
	{
		FlashMessageTime -= GetWorld()->GetDeltaSeconds();
		if (FlashMessageTime <= 0.f)
		{
			FlashMessage.Reset();
		}
	}

	// Once this player has extracted, the round is over for them: the pawn is
	// gone, so the loot UI has nothing to point at. Show the settlement board
	// only — mixing a live HUD with a finished run reads as a bug.
	//
	// The second condition covers the one-frame race at extraction time: the
	// server destroys the pawn and sets bExtracted in the same call, but those
	// arrive on the client as two separate replication events. Without it the
	// screen goes briefly blank (pawn gone, flag not yet here). A missing pawn
	// with a banked payout is unambiguous, so treat it as extracted.
	const ASOLPlayerState* MyState = GetOwningPlayerController()
		? GetOwningPlayerController()->GetPlayerState<ASOLPlayerState>()
		: nullptr;
	const bool bPawnGoneAfterPayout = MyState && !GetOwningPawn() && MyState->ExtractedValue > 0;
	if (MyState && (MyState->bExtracted || bPawnGoneAfterPayout || MyState->bFailedExtraction))
	{
		DrawSettlementBoard();
		return;
	}

	// Match clock is always visible: "extract before the timer" is the
	// constraint the whole round is built on, so hiding it would remove the
	// pressure that makes greed a decision.
	DrawMatchClock();

	// Dead but not out of the round: death screen replaces the live HUD until
	// the respawn lands. Drawn before everything else and returns, for the
	// same reason as the settlement board — a live HUD on a corpse reads as a
	// bug.
	ASOLCharacter* MyChar = Cast<ASOLCharacter>(GetOwningPawn());

	// A live pawn always wins over the overlay: the respawn is server-driven,
	// so the new pawn can arrive before the local countdown finishes. Without
	// this the player would be walking around behind a death screen.
	if (MyChar && !MyChar->IsDead())
	{
		bShowingDeath = false;
	}
	if (bShowingDeath || (MyChar && MyChar->IsDead()))
	{
		DrawDeathScreen();
		DrawKillFeed();
		return;
	}

	// Chinese container nameplates are always on (they replace the hidden
	// ASCII world labels) — independent of the loot panels.
	DrawContainerNameplates();

	// Extraction guidance is always on too: knowing where the exits are is
	// part of the core loop ("搜集 -> 带出去"), not an optional overlay.
	DrawExtractionNameplates();
	DrawExtractionProgress();

	// Combat layer: enemies, vitals, crosshair, feedback.
	DrawEnemyNameplates();
	DrawVitals(MyChar);
	DrawKillFeed();
	DrawDamageVignette(MyChar);

	if (OpenedContainer.IsValid())
	{
		if (USOLBackpackComponent* Backpack = GetOwningPawn() ? GetOwningPawn()->FindComponentByClass<USOLBackpackComponent>() : nullptr)
		{
			DrawBackpackPanel(Backpack);
			DrawContainerPanel(OpenedContainer.Get());
		}

		// Bottom hint bar.
		const FString Hint = TEXT("双击容器物品 → 放入背包 ｜ 双击背包物品 → 丢到地上 ｜ 按 F 关闭");
		float HW = 0.f, HH = 0.f;
		Canvas->TextSize(Font, Hint, HW, HH, 1.2f, 1.2f);
		const float HintY = Canvas->SizeY - HH * 1.2f - 24.f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), Canvas->SizeX * 0.5f - HW * 0.5f - 18.f, HintY - 10.f, HW + 36.f, HH + 20.f);
		DrawText(Hint, FLinearColor::White, Canvas->SizeX * 0.5f - HW * 0.5f, HintY, Font, 1.2f);
	}
	else
	{
		// Idle hint at screen center-bottom.
		const FString Hint = TEXT("靠近容器按 F 打开 ｜ 左键射击 · R 装填 ｜ 站进撤离点带走物资");
		float HW = 0.f, HH = 0.f;
		Canvas->TextSize(Font, Hint, HW, HH, 1.2f, 1.2f);
		DrawText(Hint, FLinearColor(1.f, 1.f, 1.f, 0.55f), Canvas->SizeX * 0.5f - HW * 0.5f, Canvas->SizeY - 64.f, Font, 1.2f);

		// Crosshair only outside the panels: while looting, the mouse is a
		// cursor and a crosshair in the middle of the screen would be a lie.
		DrawCrosshair();
	}

	// Carried value strip (top-left): the run's current worth. Drawn outside
	// the panel branch because it matters most while walking to an exit.
	if (USOLBackpackComponent* MyBackpack = GetOwningPawn() ? GetOwningPawn()->FindComponentByClass<USOLBackpackComponent>() : nullptr)
	{
		DrawCarriedValue(MyBackpack);
	}

	if (!FlashMessage.IsEmpty() && FlashMessageTime > 0.f)
	{
		float MW = 0.f, MH = 0.f;
		Canvas->TextSize(Font, FlashMessage, MW, MH, 1.4f, 1.4f);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.7f), Canvas->SizeX * 0.5f - MW * 0.5f - 16.f, Canvas->SizeY * 0.68f - 10.f, MW + 32.f, MH + 20.f);
		DrawText(FlashMessage, FLinearColor(1.f, 0.9f, 0.4f), Canvas->SizeX * 0.5f - MW * 0.5f, Canvas->SizeY * 0.68f, Font, 1.4f);
	}
}

void ASOLHUD::DrawBackpackPanel(USOLBackpackComponent* Backpack)
{
	UFont* Font = GEngine->GetMediumFont();
	const float PanelX = PanelSideMargin;
	const float PanelY = Canvas->SizeY * 0.18f;
	float CursorY = PanelY;

	DrawRect(FLinearColor(0.02f, 0.03f, 0.05f, 0.85f), PanelX, PanelY, PanelWidth, PanelTitleHeight);
	// The panel title carries the encumbrance state too: this is the screen the
	// player is looking at while deciding whether to take one more heavy stack.
	const FString Title = FString::Printf(TEXT("背包  %.1f / %.1f kg  ·  %s %.0f%%"),
		Backpack->CurrentWeightKg(), Backpack->MaxWeightKg,
		*Backpack->GetEncumbranceLabel(), Backpack->GetSpeedMultiplier() * 100.f);
	DrawText(Title, FLinearColor(1.f, 1.f, 1.f, 0.95f), PanelX + PanelTextPaddingX, CursorY + 14.f, Font, TitleTextScale);
	CursorY += PanelTitleHeight;

	if (Backpack->Items.Num() == 0)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), PanelX, CursorY, PanelWidth, PanelRowHeight);
		DrawText(TEXT("（空）"), FLinearColor(1.f, 1.f, 1.f, 0.4f), PanelX + PanelTextPaddingX, CursorY + 12.f, Font, RowTextScale);
		return;
	}

	for (int32 Idx = 0; Idx < Backpack->Items.Num(); ++Idx)
	{
		const FSOLItemInstance& Inst = Backpack->Items[Idx];
		const FString Row = FString::Printf(TEXT("%d. %s  ×%d  (%.1fkg)"),
			Idx + 1, *Inst.Def.DisplayName, Inst.Count, Inst.TotalWeightKg());
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), PanelX, CursorY, PanelWidth, PanelRowHeight);
		DrawText(Row, FLinearColor::White, PanelX + PanelTextPaddingX, CursorY + 12.f, Font, RowTextScale);

		FSOLHotRect Hot;
		Hot.Box = FBox2D(FVector2D(PanelX, CursorY), FVector2D(PanelX + PanelWidth, CursorY + PanelRowHeight));
		Hot.ItemIndex = Idx;
		BackpackHotRects.Add(Hot);

		CursorY += PanelRowStep;
	}
}

void ASOLHUD::DrawContainerPanel(ASOLContainer* Container)
{
	UFont* Font = GEngine->GetMediumFont();
	const float PanelX = Canvas->SizeX - PanelWidth - PanelSideMargin;
	const float PanelY = Canvas->SizeY * 0.18f;
	float CursorY = PanelY;

	DrawRect(FLinearColor(0.05f, 0.03f, 0.02f, 0.85f), PanelX, PanelY, PanelWidth, PanelTitleHeight);
	const FString Title = FString::Printf(TEXT("容器 · %s"), *Container->GetDisplayNameZh());
	DrawText(Title, FLinearColor(1.f, 0.85f, 0.6f), PanelX + PanelTextPaddingX, CursorY + 14.f, Font, TitleTextScale);
	CursorY += PanelTitleHeight;

	if (!Container->HasItems())
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), PanelX, CursorY, PanelWidth, PanelRowHeight);
		DrawText(TEXT("（空）"), FLinearColor(1.f, 1.f, 1.f, 0.4f), PanelX + PanelTextPaddingX, CursorY + 12.f, Font, RowTextScale);
		return;
	}

	for (int32 Idx = 0; Idx < Container->Contents.Num(); ++Idx)
	{
		const FSOLItemInstance& Inst = Container->Contents[Idx];
		const FString Row = FString::Printf(TEXT("%d. %s  ×%d  (%.1fkg)"),
			Idx + 1, *Inst.Def.DisplayName, Inst.Count, Inst.TotalWeightKg());
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), PanelX, CursorY, PanelWidth, PanelRowHeight);
		DrawText(Row, FLinearColor::White, PanelX + PanelTextPaddingX, CursorY + 12.f, Font, RowTextScale);

		FSOLHotRect Hot;
		Hot.Box = FBox2D(FVector2D(PanelX, CursorY), FVector2D(PanelX + PanelWidth, CursorY + PanelRowHeight));
		Hot.ItemIndex = Idx;
		ContainerHotRects.Add(Hot);

		CursorY += PanelRowStep;
	}
}

void ASOLHUD::DrawContainerNameplates()
{
	UFont* Font = GEngine->GetMediumFont();
	APawn* Pawn = GetOwningPawn();
	if (!Font || !Pawn)
	{
		return;
	}

	for (TActorIterator<ASOLContainer> It(GetWorld()); It; ++It)
	{
		ASOLContainer* Container = *It;
		if (!Container)
		{
			continue;
		}

		const float Dist = FVector::Dist(Pawn->GetActorLocation(), Container->GetActorLocation());
		if (Dist > 4500.f)
		{
			continue; // beyond 45m the nameplate would be noise
		}

		// Project the point above the container into screen space; the
		// legacy Canvas->Project zeroes Z for points behind the camera.
		const FVector LabelPos = Container->GetActorLocation() + FVector(0.f, 0.f, 170.f);
		const FVector Projected = Canvas->Project(LabelPos);
		if (Projected.Z <= 0.f)
		{
			continue;
		}

		// Fade out with distance: solid up close, faint at the 45m cutoff.
		const float Alpha = FMath::GetMappedRangeValueClamped(
			FVector2D(4500.f, 1500.f), FVector2D(0.15f, 0.95f), Dist);

		const bool bInRange = Dist <= 250.f;
		const FString Text = bInRange
			? FString::Printf(TEXT("%s  [F 开启]"), *Container->GetDisplayNameZh())
			: FString::Printf(TEXT("%s  %.0fm"), *Container->GetDisplayNameZh(), Dist / 100.f);

		float TW = 0.f, TH = 0.f;
		Canvas->TextSize(Font, Text, TW, TH, 1.1f, 1.1f);
		const float CX = Projected.X - TW * 0.5f;
		const float CY = Projected.Y - TH * 1.1f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f * Alpha), CX - 10.f, CY - 6.f, TW + 20.f, TH + 12.f);
		const FLinearColor TextColor = bInRange
			? FLinearColor(1.f, 0.95f, 0.6f, Alpha)
			: FLinearColor(1.f, 0.85f, 0.55f, Alpha);
		DrawText(Text, TextColor, CX, CY, Font, 1.1f);
	}
}

void ASOLHUD::DrawExtractionNameplates()
{
	UFont* Font = GEngine->GetMediumFont();
	APawn* Pawn = GetOwningPawn();
	if (!Font || !Pawn)
	{
		return;
	}

	for (TActorIterator<ASOLExtractionZone> It(GetWorld()); It; ++It)
	{
		ASOLExtractionZone* Zone = *It;
		if (!Zone)
		{
			continue;
		}

		const float Dist = FVector::Dist2D(Pawn->GetActorLocation(), Zone->GetActorLocation());
		const bool bInside = Zone->IsPawnInside(Pawn);

		// Exits stay legible from much further out than containers: the whole
		// point is to navigate towards one from across the map.
		const FVector LabelPos = Zone->GetActorLocation() + FVector(0.f, 0.f, 900.f);
		const FVector Projected = Canvas->Project(LabelPos);
		if (Projected.Z <= 0.f)
		{
			// Behind the camera: fall back to a screen-edge marker so an
			// off-screen exit is still findable. Canvas->Project is useless
			// here, so derive the side from the pawn's own right vector.
			const FVector ToZone = (Zone->GetActorLocation() - Pawn->GetActorLocation()).GetSafeNormal2D();
			const bool bToTheRight = FVector::DotProduct(ToZone, Pawn->GetActorRightVector().GetSafeNormal2D()) >= 0.f;
			const FString Text = bToTheRight
				? FString::Printf(TEXT("%s %.0fm  右后方"), *Zone->GetDisplayName(), Dist / 100.f)
				: FString::Printf(TEXT("左后方  %s %.0fm"), *Zone->GetDisplayName(), Dist / 100.f);

			float TW = 0.f, TH = 0.f;
			Canvas->TextSize(Font, Text, TW, TH, 1.05f, 1.05f);
			const float EX = bToTheRight ? (Canvas->SizeX - TW - 30.f) : 30.f;
			const float EY = Canvas->SizeY * 0.46f;
			DrawRect(FLinearColor(0.f, 0.1f, 0.04f, 0.35f), EX - 10.f, EY - 6.f, TW + 20.f, TH + 12.f);
			DrawText(Text, FLinearColor(0.5f, 0.95f, 0.55f, 0.55f), EX, EY, Font, 1.05f);
			continue;
		}

		const float Alpha = FMath::GetMappedRangeValueClamped(
			FVector2D(12000.f, 2000.f), FVector2D(0.35f, 1.f), Dist);

		FString Text;
		if (bInside)
		{
			Text = FString::Printf(TEXT("%s · 撤离中"), *Zone->GetDisplayName());
		}
		else if (Zone->MinValueRequired > 0)
		{
			Text = FString::Printf(TEXT("%s  %.0fm  (需 %d 价值)"),
				*Zone->GetDisplayName(), Dist / 100.f, Zone->MinValueRequired);
		}
		else
		{
			Text = FString::Printf(TEXT("%s  %.0fm"), *Zone->GetDisplayName(), Dist / 100.f);
		}

		float TW = 0.f, TH = 0.f;
		Canvas->TextSize(Font, Text, TW, TH, 1.15f, 1.15f);
		const float CX = Projected.X - TW * 0.5f;
		const float CY = Projected.Y - TH * 1.15f;
		DrawRect(FLinearColor(0.f, 0.12f, 0.05f, 0.5f * Alpha), CX - 12.f, CY - 7.f, TW + 24.f, TH + 14.f);
		// Green reads as "exit / safe" and keeps zones visually distinct from
		// the amber container nameplates.
		DrawText(Text, FLinearColor(0.55f, 1.f, 0.6f, Alpha), CX, CY, Font, 1.15f);
	}
}

void ASOLHUD::DrawExtractionProgress()
{
	UFont* Font = GEngine->GetMediumFont();
	APawn* Pawn = GetOwningPawn();
	APlayerController* PC = GetOwningPlayerController();
	APlayerState* MyState = PC ? PC->PlayerState : nullptr;
	if (!Font || !Pawn || !MyState)
	{
		return;
	}

	// Find the zone this player is standing in, and read the *replicated*
	// progress the server computed. The client never runs the timer itself:
	// if the two disagreed, the bar would lie about a life-or-death moment.
	ASOLExtractionZone* ActiveZone = nullptr;
	float Ratio = 0.f;
	for (TActorIterator<ASOLExtractionZone> It(GetWorld()); It; ++It)
	{
		ASOLExtractionZone* Zone = *It;
		if (Zone && Zone->IsPawnInside(Pawn))
		{
			ActiveZone = Zone;
			Ratio = Zone->GetProgressFor(MyState);
			break;
		}
	}
	if (!ActiveZone)
	{
		return;
	}

	const USOLBackpackComponent* Backpack = Pawn->FindComponentByClass<USOLBackpackComponent>();
	const int32 Carried = Backpack ? Backpack->CurrentValue() : 0;
	const bool bBlocked = ActiveZone->MinValueRequired > 0 && Carried < ActiveZone->MinValueRequired;

	// Layout: a wide bar just above the bottom hint line, centred.
	const float BarW = FMath::Min(760.f, Canvas->SizeX * 0.5f);
	const float BarH = 30.f;
	const float BarX = Canvas->SizeX * 0.5f - BarW * 0.5f;
	const float BarY = Canvas->SizeY * 0.74f;

	FString Title;
	if (bBlocked)
	{
		Title = FString::Printf(TEXT("%s ｜ 需要携带价值 %d 才能撤离（当前 %d）"),
			*ActiveZone->GetDisplayName(), ActiveZone->MinValueRequired, Carried);
	}
	else
	{
		const float Remain = FMath::Max(0.f, ActiveZone->HoldSeconds * (1.f - Ratio));
		Title = FString::Printf(TEXT("%s ｜ 撤离中 %.0f%%  剩余 %.1fs  ｜ 离开区域将重置"),
			*ActiveZone->GetDisplayName(), Ratio * 100.f, Remain);
	}

	float TW = 0.f, TH = 0.f;
	Canvas->TextSize(Font, Title, TW, TH, 1.25f, 1.25f);
	DrawText(Title, bBlocked ? FLinearColor(1.f, 0.55f, 0.4f) : FLinearColor(0.7f, 1.f, 0.75f),
		Canvas->SizeX * 0.5f - TW * 0.5f, BarY - TH - 10.f, Font, 1.25f);

	// Track + fill. Two DrawRect calls beat any texture asset here.
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f), BarX - 3.f, BarY - 3.f, BarW + 6.f, BarH + 6.f);
	DrawRect(FLinearColor(0.06f, 0.08f, 0.07f, 0.9f), BarX, BarY, BarW, BarH);
	if (!bBlocked && Ratio > 0.f)
	{
		// Colour ramps from amber to green as the hold completes, so the
		// state is readable at a glance without reading the number.
		const FLinearColor Fill = FMath::Lerp(
			FLinearColor(1.f, 0.75f, 0.25f), FLinearColor(0.35f, 1.f, 0.45f), Ratio);
		DrawRect(Fill, BarX, BarY, BarW * Ratio, BarH);
	}
}

void ASOLHUD::DrawCarriedValue(USOLBackpackComponent* Backpack)
{
	UFont* Font = GEngine->GetMediumFont();
	if (!Font || !Backpack)
	{
		return;
	}
	const int32 Value = Backpack->CurrentValue();

	// Round progress is worth showing even with an empty bag (knowing a rival
	// already left changes how greedy you should be), so build the strip in
	// two parts and skip only the value half when carrying nothing.
	const ASOLGameState* GS = GetWorld() ? GetWorld()->GetGameState<ASOLGameState>() : nullptr;
	const bool bShowRound = GS && GS->TotalPlayers > 1;
	if (Value <= 0 && !bShowRound)
	{
		return; // nothing to report; keep the screen clean
	}

	FString Text;
	if (Value > 0)
	{
		// Encumbrance is shown next to the weight it comes from: seeing
		// "满载 -45% 速度" right beside "29.4 / 30.0 kg" is what makes the
		// slowdown legible instead of feeling like a bug.
		const float SpeedPct = Backpack->GetSpeedMultiplier() * 100.f;
		Text = FString::Printf(TEXT("携带价值 %d  ｜  %.1f / %.1f kg  ｜  %s"),
			Value, Backpack->CurrentWeightKg(), Backpack->MaxWeightKg,
			*Backpack->GetEncumbranceLabel());
		if (SpeedPct < 99.5f)
		{
			Text += FString::Printf(TEXT("  速度 %.0f%%"), SpeedPct);
		}
	}
	else
	{
		Text = TEXT("背包空");
	}
	if (bShowRound)
	{
		Text += FString::Printf(TEXT("　　已撤离 %d / %d"), GS->ExtractedCount, GS->TotalPlayers);
	}

	float TW = 0.f, TH = 0.f;
	Canvas->TextSize(Font, Text, TW, TH, 1.25f, 1.25f);
	const float X = PanelSideMargin;
	const float Y = 28.f;
	DrawRect(FLinearColor(0.02f, 0.03f, 0.05f, 0.75f), X - 12.f, Y - 8.f, TW + 24.f, TH + 16.f);
	DrawText(Text, Value > 0 ? FLinearColor(1.f, 0.92f, 0.55f) : FLinearColor(1.f, 1.f, 1.f, 0.6f),
		X, Y, Font, 1.25f);
}

void ASOLHUD::DrawSettlementBoard()
{
	UFont* Font = GEngine->GetMediumFont();
	APlayerController* PC = GetOwningPlayerController();
	const ASOLPlayerState* MyState = PC ? PC->GetPlayerState<ASOLPlayerState>() : nullptr;
	if (!Font || !MyState)
	{
		return;
	}

	// Full-screen dim so the board reads as an end state, not an overlay.
	// A failed extraction gets a red wash instead of a neutral one — losing
	// the run's loot should not look like a successful one.
	DrawRect(MyState->bFailedExtraction
		? FLinearColor(0.14f, 0.f, 0.f, 0.78f)
		: FLinearColor(0.f, 0.f, 0.f, 0.72f),
		0.f, 0.f, Canvas->SizeX, Canvas->SizeY);

	const float CenterX = Canvas->SizeX * 0.5f;
	float CursorY = Canvas->SizeY * 0.2f;

	auto DrawCentered = [&](const FString& Text, const FLinearColor& Color, float Scale)
	{
		float W = 0.f, H = 0.f;
		Canvas->TextSize(Font, Text, W, H, Scale, Scale);
		DrawText(Text, Color, CenterX - W * 0.5f, CursorY, Font, Scale);
		CursorY += H * Scale + 14.f;
	};

	// Three outcomes, not two: extracted, ran out of time, or the whole round
	// is finished. The GameState knows which; the pawn cannot.
	const ASOLGameState* GS = GetWorld() ? GetWorld()->GetGameState<ASOLGameState>() : nullptr;
	const bool bRoundOver = GS && GS->bRoundOver;

	if (MyState->bFailedExtraction)
	{
		DrawCentered(TEXT("撤离失败"), FLinearColor(1.f, 0.35f, 0.3f), 2.6f);
		CursorY += 10.f;
		DrawCentered(TEXT("时间耗尽，未能带出任何物资"), FLinearColor(1.f, 1.f, 1.f, 0.85f), 1.5f);
		DrawCentered(FString::Printf(TEXT("损失物资价值：%d"), MyState->LostValue),
			FLinearColor(1.f, 0.55f, 0.45f), 2.f);
	}
	else
	{
		DrawCentered(bRoundOver ? TEXT("本局结束") : TEXT("撤离成功"),
			FLinearColor(0.45f, 1.f, 0.55f), 2.6f);
		CursorY += 10.f;
		DrawCentered(FString::Printf(TEXT("撤离点：%s"), *MyState->ExtractedZoneName),
			FLinearColor(1.f, 1.f, 1.f, 0.85f), 1.5f);
		DrawCentered(FString::Printf(TEXT("带出物资：%d 件"), MyState->ExtractedStacks),
			FLinearColor(1.f, 1.f, 1.f, 0.85f), 1.5f);
		DrawCentered(FString::Printf(TEXT("本局收益：%d"), MyState->ExtractedValue),
			FLinearColor(1.f, 0.92f, 0.5f), 2.f);
	}

	// Combat tally: kills are worth what the victim was carrying, so the two
	// numbers together tell the story of how the run was played.
	DrawCentered(FString::Printf(TEXT("击杀 %d ｜ 死亡 %d ｜ 击杀掉落价值 %d"),
		MyState->Kills, MyState->Deaths, MyState->KillBountyTotal),
		FLinearColor(0.85f, 0.9f, 1.f, 0.9f), 1.4f);

	if (GS && GS->TotalPlayers > 0)
	{
		DrawCentered(FString::Printf(TEXT("撤离进度：%d / %d  ｜  全队收益 %d"),
			GS->ExtractedCount, GS->TotalPlayers, GS->TeamValue),
			FLinearColor(0.75f, 0.9f, 1.f, 0.9f), 1.4f);
	}

	CursorY += 20.f;

	// Everyone else's result: PlayerState is replicated to all clients, so a
	// scoreboard needs no extra networking — read GameState's player array.
	if (const AGameStateBase* BaseGS = GetWorld() ? GetWorld()->GetGameState() : nullptr)
	{
		DrawCentered(TEXT("——  本局战况  ——"), FLinearColor(1.f, 1.f, 1.f, 0.55f), 1.3f);
		for (APlayerState* Other : BaseGS->PlayerArray)
		{
			const ASOLPlayerState* OtherSOL = Cast<ASOLPlayerState>(Other);
			if (!OtherSOL)
			{
				continue;
			}
			const bool bMe = (OtherSOL == MyState);
			FString Line;
			if (OtherSOL->bExtracted)
			{
				Line = FString::Printf(TEXT("%s%s：已撤离，收益 %d（%d 杀）"),
					bMe ? TEXT("▶ ") : TEXT("　"), *OtherSOL->GetPlayerName(),
					OtherSOL->ExtractedValue, OtherSOL->Kills);
			}
			else if (OtherSOL->bFailedExtraction)
			{
				Line = FString::Printf(TEXT("%s%s：撤离失败，损失 %d（%d 杀）"),
					bMe ? TEXT("▶ ") : TEXT("　"), *OtherSOL->GetPlayerName(),
					OtherSOL->LostValue, OtherSOL->Kills);
			}
			else
			{
				Line = FString::Printf(TEXT("　%s：仍在区域内（%d 杀 / %d 死）"),
					*OtherSOL->GetPlayerName(), OtherSOL->Kills, OtherSOL->Deaths);
			}
			DrawCentered(Line, bMe ? FLinearColor(0.6f, 1.f, 0.7f) : FLinearColor(1.f, 1.f, 1.f, 0.7f), 1.3f);
		}
	}

	// The round can be replayed in place; without this line nobody would know.
	CursorY += 26.f;
	DrawCentered(TEXT("按 R 重新开始本局"), FLinearColor(0.8f, 0.95f, 1.f, 0.85f), 1.5f);
}

void ASOLHUD::DrawCrosshair()
{
	// Four ticks around a gap, drawn with DrawRect — no texture, and the gap
	// keeps the exact aim point unobscured. Blooms and turns red on a kill,
	// which is the entire hit-feedback budget of this project.
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;

	const bool bHit = HitMarkerTime > 0.f;
	const float Gap = bHit ? 9.f : 6.f;
	const float Len = bHit ? 13.f : 10.f;
	const float Thick = 2.f;
	FLinearColor Color = FLinearColor(1.f, 1.f, 1.f, 0.75f);
	if (bHit)
	{
		Color = bHitMarkerWasKill
			? FLinearColor(1.f, 0.25f, 0.2f, 1.f)
			: FLinearColor(1.f, 0.95f, 0.55f, 1.f);
	}

	DrawRect(Color, CX - Gap - Len, CY - Thick * 0.5f, Len, Thick);          // left
	DrawRect(Color, CX + Gap, CY - Thick * 0.5f, Len, Thick);               // right
	DrawRect(Color, CX - Thick * 0.5f, CY - Gap - Len, Thick, Len);         // up
	DrawRect(Color, CX - Thick * 0.5f, CY + Gap, Thick, Len);               // down

	if (HitMarkerTime > 0.f)
	{
		HitMarkerTime -= GetWorld()->GetDeltaSeconds();
	}
}

void ASOLHUD::DrawVitals(ASOLCharacter* MyChar)
{
	UFont* Font = GEngine->GetMediumFont();
	if (!Font || !MyChar)
	{
		return;
	}

	const float Ratio = MyChar->GetHealthRatio();
	const float BarW = 320.f;
	const float BarH = 26.f;
	const float X = PanelSideMargin;
	const float Y = Canvas->SizeY - 110.f;

	// Health. Colour ramps green -> amber -> red so the state reads without
	// parsing the number.
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X - 3.f, Y - 3.f, BarW + 6.f, BarH + 6.f);
	DrawRect(FLinearColor(0.08f, 0.03f, 0.03f, 0.9f), X, Y, BarW, BarH);
	const FLinearColor Fill = Ratio > 0.5f
		? FMath::Lerp(FLinearColor(0.95f, 0.8f, 0.2f), FLinearColor(0.35f, 0.9f, 0.4f), (Ratio - 0.5f) * 2.f)
		: FMath::Lerp(FLinearColor(0.95f, 0.2f, 0.15f), FLinearColor(0.95f, 0.8f, 0.2f), Ratio * 2.f);
	DrawRect(Fill, X, Y, BarW * Ratio, BarH);

	const FString HpText = FString::Printf(TEXT("%.0f"), Ratio * 100.f);
	DrawText(HpText, FLinearColor::White, X + 10.f, Y + 3.f, Font, 1.15f);

	// Spawn protection: without a readout, "my shots land but I take no
	// damage" looks like a bug on both sides of the fight.
	if (MyChar->HealthComp && MyChar->HealthComp->IsProtected())
	{
		const FString Prot = FString::Printf(TEXT("重生保护 %.1fs"), MyChar->HealthComp->ProtectionLeft);
		float PW = 0.f, PH = 0.f;
		Canvas->TextSize(Font, Prot, PW, PH, 1.15f, 1.15f);
		DrawRect(FLinearColor(0.f, 0.1f, 0.2f, 0.6f), X - 3.f, Y - PH - 14.f, PW + 16.f, PH + 10.f);
		DrawText(Prot, FLinearColor(0.55f, 0.85f, 1.f), X + 5.f, Y - PH - 12.f, Font, 1.15f);
	}

	// Ammo, bottom-right. Reload state matters more than the count, so it
	// replaces it while reloading.
	const FString AmmoText = MyChar->IsReloading()
		? TEXT("装填中…")
		: FString::Printf(TEXT("%d / %d"), MyChar->GetCurrentAmmo(), MyChar->MagazineSize);
	float AW = 0.f, AH = 0.f;
	Canvas->TextSize(Font, AmmoText, AW, AH, 1.6f, 1.6f);
	const float AX = Canvas->SizeX - AW - PanelSideMargin;
	const float AY = Canvas->SizeY - 110.f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), AX - 14.f, AY - 8.f, AW + 28.f, AH + 16.f);
	const FLinearColor AmmoColor = MyChar->IsReloading()
		? FLinearColor(1.f, 0.75f, 0.3f)
		: (MyChar->GetCurrentAmmo() <= 5 ? FLinearColor(1.f, 0.45f, 0.35f) : FLinearColor::White);
	DrawText(AmmoText, AmmoColor, AX, AY, Font, 1.6f);
}

void ASOLHUD::DrawMatchClock()
{
	UFont* Font = GEngine->GetMediumFont();
	const ASOLGameState* GS = GetWorld() ? GetWorld()->GetGameState<ASOLGameState>() : nullptr;
	if (!Font || !GS || GS->MatchDurationSeconds <= 0)
	{
		return;
	}

	const int32 Left = GS->RemainingSeconds;
	const FString Text = FString::Printf(TEXT("%02d:%02d"), Left / 60, Left % 60);

	// Under a minute the clock turns red and grows — the one moment in the
	// round where the timer should dominate the screen.
	const bool bUrgent = Left <= 60;
	const float Scale = bUrgent ? 2.2f : 1.8f;
	float TW = 0.f, TH = 0.f;
	Canvas->TextSize(Font, Text, TW, TH, Scale, Scale);
	const float X = Canvas->SizeX * 0.5f - TW * 0.5f;
	const float Y = 22.f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X - 18.f, Y - 8.f, TW + 36.f, TH + 16.f);
	DrawText(Text, bUrgent ? FLinearColor(1.f, 0.35f, 0.3f) : FLinearColor(1.f, 1.f, 1.f, 0.9f),
		X, Y, Font, Scale);

	if (bUrgent)
	{
		const FString Warn = TEXT("时间即将耗尽 —— 尽快撤离");
		float WW = 0.f, WH = 0.f;
		Canvas->TextSize(Font, Warn, WW, WH, 1.2f, 1.2f);
		DrawText(Warn, FLinearColor(1.f, 0.55f, 0.45f, 0.9f),
			Canvas->SizeX * 0.5f - WW * 0.5f, Y + TH * Scale + 6.f, Font, 1.2f);
	}
}

void ASOLHUD::DrawEnemyNameplates()
{
	UFont* Font = GEngine->GetMediumFont();
	APawn* MyPawn = GetOwningPawn();
	if (!Font || !MyPawn)
	{
		return;
	}

	// Both players and scavengers wear SK_Mannequin, so the nameplate is the
	// only way to tell them apart. It also carries the health bar, which is
	// what makes a firefight readable ("two more hits").
	for (TActorIterator<ASOLScavenger> It(GetWorld()); It; ++It)
	{
		ASOLScavenger* Scav = *It;
		if (!Scav || Scav->IsDead())
		{
			continue;
		}

		const float Dist = FVector::Dist(MyPawn->GetActorLocation(), Scav->GetActorLocation());
		if (Dist > 6000.f)
		{
			continue;
		}

		const FVector LabelPos = Scav->GetActorLocation() + FVector(0.f, 0.f, 120.f);
		const FVector Projected = Canvas->Project(LabelPos);
		if (Projected.Z <= 0.f)
		{
			continue;
		}

		const float Alpha = FMath::GetMappedRangeValueClamped(
			FVector2D(6000.f, 1500.f), FVector2D(0.3f, 1.f), Dist);

		const FString Text = FString::Printf(TEXT("%s  %.0fm"), *Scav->DisplayNameZh, Dist / 100.f);
		float TW = 0.f, TH = 0.f;
		Canvas->TextSize(Font, Text, TW, TH, 1.05f, 1.05f);
		const float CX = Projected.X - TW * 0.5f;
		const float CY = Projected.Y - TH * 1.05f;

		DrawRect(FLinearColor(0.12f, 0.f, 0.f, 0.45f * Alpha), CX - 8.f, CY - 5.f, TW + 16.f, TH + 10.f);
		DrawText(Text, FLinearColor(1.f, 0.5f, 0.45f, Alpha), CX, CY, Font, 1.05f);

		// Health bar under the label, reading the replicated health.
		const float HB_W = FMath::Max(TW, 70.f);
		const float HB_H = 5.f;
		const float HB_X = Projected.X - HB_W * 0.5f;
		const float HB_Y = CY + TH * 1.05f + 3.f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f * Alpha), HB_X - 1.f, HB_Y - 1.f, HB_W + 2.f, HB_H + 2.f);
		DrawRect(FLinearColor(0.25f, 0.05f, 0.05f, 0.8f * Alpha), HB_X, HB_Y, HB_W, HB_H);
		DrawRect(FLinearColor(0.95f, 0.3f, 0.25f, Alpha), HB_X, HB_Y, HB_W * Scav->GetHealthRatio(), HB_H);
	}
}

void ASOLHUD::DrawKillFeed()
{
	UFont* Font = GEngine->GetMediumFont();
	if (!Font || KillFeed.Num() == 0)
	{
		return;
	}

	const float Delta = GetWorld()->GetDeltaSeconds();
	float Y = 84.f;
	for (int32 Idx = KillFeed.Num() - 1; Idx >= 0; --Idx)
	{
		FSOLFeedLine& Line = KillFeed[Idx];
		Line.TimeLeft -= Delta;
		if (Line.TimeLeft <= 0.f)
		{
			KillFeed.RemoveAt(Idx);
			continue;
		}

		// Fade over the last second so lines leave instead of blinking out.
		const float Alpha = FMath::Clamp(Line.TimeLeft, 0.f, 1.f);
		float TW = 0.f, TH = 0.f;
		Canvas->TextSize(Font, Line.Text, TW, TH, 1.2f, 1.2f);
		const float X = Canvas->SizeX - TW - PanelSideMargin;
		DrawRect(FLinearColor(0.05f, 0.f, 0.f, 0.5f * Alpha), X - 12.f, Y - 6.f, TW + 24.f, TH + 12.f);
		DrawText(Line.Text, FLinearColor(1.f, 0.85f, 0.5f, Alpha), X, Y, Font, 1.2f);
		Y += TH + 18.f;
	}
}

void ASOLHUD::DrawDamageVignette(ASOLCharacter* MyChar)
{
	if (!MyChar)
	{
		return;
	}

	// Taking damage is the one thing a first-person player cannot see (no
	// flinch animation on your own camera), so the screen edges flash red.
	// Derived from the replicated health dropping — zero extra networking.
	const float Ratio = MyChar->GetHealthRatio();
	if (Ratio < LastSeenHealthRatio - KINDA_SMALL_NUMBER)
	{
		DamageFlashTime = 0.45f;
	}
	LastSeenHealthRatio = Ratio;

	if (DamageFlashTime <= 0.f)
	{
		// Low health keeps a permanent faint tint, so "nearly dead" is a
		// state you can feel rather than a number you have to check.
		if (Ratio < 0.35f)
		{
			const float Band = 26.f;
			const float A = (0.35f - Ratio) * 0.6f;
			DrawRect(FLinearColor(0.6f, 0.f, 0.f, A), 0.f, 0.f, Canvas->SizeX, Band);
			DrawRect(FLinearColor(0.6f, 0.f, 0.f, A), 0.f, Canvas->SizeY - Band, Canvas->SizeX, Band);
		}
		return;
	}

	DamageFlashTime -= GetWorld()->GetDeltaSeconds();
	const float Alpha = FMath::Clamp(DamageFlashTime / 0.45f, 0.f, 1.f) * 0.55f;
	const float Band = 60.f;
	DrawRect(FLinearColor(0.8f, 0.05f, 0.05f, Alpha), 0.f, 0.f, Canvas->SizeX, Band);
	DrawRect(FLinearColor(0.8f, 0.05f, 0.05f, Alpha), 0.f, Canvas->SizeY - Band, Canvas->SizeX, Band);
	DrawRect(FLinearColor(0.8f, 0.05f, 0.05f, Alpha), 0.f, 0.f, Band, Canvas->SizeY);
	DrawRect(FLinearColor(0.8f, 0.05f, 0.05f, Alpha), Canvas->SizeX - Band, 0.f, Band, Canvas->SizeY);
}

void ASOLHUD::DrawDeathScreen()
{
	UFont* Font = GEngine->GetMediumFont();
	if (!Font)
	{
		return;
	}

	DrawRect(FLinearColor(0.15f, 0.f, 0.f, 0.75f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);

	const float CenterX = Canvas->SizeX * 0.5f;
	float CursorY = Canvas->SizeY * 0.34f;
	auto Centered = [&](const FString& Text, const FLinearColor& Color, float Scale)
	{
		float W = 0.f, H = 0.f;
		Canvas->TextSize(Font, Text, W, H, Scale, Scale);
		DrawText(Text, Color, CenterX - W * 0.5f, CursorY, Font, Scale);
		CursorY += H * Scale + 16.f;
	};

	Centered(TEXT("你已被击杀"), FLinearColor(1.f, 0.35f, 0.3f), 2.8f);
	if (!DeathKillerName.IsEmpty())
	{
		Centered(FString::Printf(TEXT("击杀者：%s"), *DeathKillerName), FLinearColor(1.f, 1.f, 1.f, 0.85f), 1.5f);
	}
	// The important half: dying costs the run's loot, not just time.
	Centered(TEXT("携带的物资已散落在原地"), FLinearColor(1.f, 0.8f, 0.5f, 0.9f), 1.4f);

	if (DeathRespawnLeft > 0.f)
	{
		DeathRespawnLeft -= GetWorld()->GetDeltaSeconds();
		Centered(FString::Printf(TEXT("%.0f 秒后重生"), FMath::Max(0.f, DeathRespawnLeft)),
			FLinearColor(0.8f, 0.9f, 1.f, 0.9f), 1.6f);
	}
	else
	{
		// The respawn itself is server-driven; once the new pawn arrives the
		// live HUD comes back on its own, so clearing the flag here just stops
		// the overlay from lingering if the pawn is slow to replicate.
		bShowingDeath = false;
	}
}

bool ASOLHUD::HitTestContainer(const FVector2D& Pos, int32& OutIndex) const{
	for (const FSOLHotRect& Hot : ContainerHotRects)
	{
		if (Hot.Box.IsInside(Pos))
		{
			OutIndex = Hot.ItemIndex;
			return true;
		}
	}
	return false;
}

bool ASOLHUD::HitTestBackpack(const FVector2D& Pos, int32& OutIndex) const
{
	for (const FSOLHotRect& Hot : BackpackHotRects)
	{
		if (Hot.Box.IsInside(Pos))
		{
			OutIndex = Hot.ItemIndex;
			return true;
		}
	}
	return false;
}

void ASOLHUD::ShowMessage(const FString& Message)
{
	FlashMessage = Message;
	FlashMessageTime = 1.8f;
}

void ASOLHUD::FlashHitMarker(bool bKill)
{
	// A kill marker stays up longer than a plain hit: it is the one piece of
	// feedback worth interrupting the player's attention for.
	HitMarkerTime = bKill ? 0.45f : 0.18f;
	bHitMarkerWasKill = bKill;
}

void ASOLHUD::PushKillFeed(const FString& Line)
{
	FSOLFeedLine Entry;
	Entry.Text = Line;
	Entry.TimeLeft = 5.f;
	KillFeed.Add(Entry);

	// Cap the log: four lines is all the corner has room for, and older
	// entries are the ones the player has already read.
	while (KillFeed.Num() > 4)
	{
		KillFeed.RemoveAt(0);
	}
}

void ASOLHUD::ShowDeathScreen(const FString& KillerName, float RespawnSeconds)
{
	bShowingDeath = true;
	DeathKillerName = KillerName;
	DeathRespawnLeft = RespawnSeconds;

	// Dying is also a feed entry, so the log reads as a history of the run.
	PushKillFeed(FString::Printf(TEXT("被 %s 击杀"), *KillerName));
}
