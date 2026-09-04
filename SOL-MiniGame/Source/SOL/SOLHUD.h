// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SOLContainer.h"
#include "SOLItemTypes.h"
#include "SOLHUD.generated.h"

class USOLBackpackComponent;

// One clickable row on a HUD panel (screen-space rect -> item index).
struct FSOLHotRect
{
	FBox2D Box;
	int32 ItemIndex = -1;
};

// Canvas-drawn container/backpack panels. Same zero-asset approach as the
// battlefield HUD: no UMG, no art dependencies, Chinese text works through
// the engine's CJK font fallback. Mouse double-click is detected by the
// player controller and resolved against the hot rects recorded here each
// frame (double-click container item -> transfer to backpack; double-click
// backpack item -> drop a world pickup).
UCLASS()
class SOL_API ASOLHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	bool IsOpen() const { return OpenedContainer.IsValid(); }

	void ShowMessage(const FString& Message);

	// ---------------------------------------------------------------- combat
	// Shooter-side feedback, driven by the server's hit confirmation (see
	// ASOLCharacter::ClientHitFeedback): the crosshair blooms white on a hit
	// and red on a kill. Feedback lives on the client because it is pure
	// presentation — the hit itself was already decided on the server.
	void FlashHitMarker(bool bKill);

	// Rolling kill/death log in the top-right (own kills and own deaths only;
	// a full server-wide feed would need its own replicated event channel).
	void PushKillFeed(const FString& Line);

	// Death screen with the killer's name and a respawn countdown.
	void ShowDeathScreen(const FString& KillerName, float RespawnSeconds);

	// Hit tests in viewport coordinates (same space as GetMousePosition).
	bool HitTestContainer(const FVector2D& Pos, int32& OutIndex) const;
	bool HitTestBackpack(const FVector2D& Pos, int32& OutIndex) const;

	TWeakObjectPtr<ASOLContainer> OpenedContainer;

private:
	void DrawBackpackPanel(USOLBackpackComponent* Backpack);
	void DrawContainerPanel(ASOLContainer* Container);

	// Screen-space Chinese nameplates floating above every nearby container
	// (replaces the ASCII world TextRender labels, which cannot render CJK
	// because TextRender only accepts offline fonts).
	void DrawContainerNameplates();

	// Extraction layer: a distance/direction readout for every zone, the
	// hold progress bar while standing inside one, and the end-of-round
	// settlement board once this player has extracted. All of it reads
	// replicated state — the HUD computes nothing authoritative.
	void DrawExtractionNameplates();
	void DrawExtractionProgress();
	void DrawSettlementBoard();

	// Carried loot value strip (what the run is currently worth), shown
	// whenever the player carries something and has not extracted yet.
	void DrawCarriedValue(USOLBackpackComponent* Backpack);

	// Combat layer. All of it reads replicated state (health on the health
	// component, ammo on the pawn, the clock on the GameState) — the HUD
	// computes nothing authoritative.
	void DrawCrosshair();
	void DrawVitals(class ASOLCharacter* MyChar);
	void DrawMatchClock();
	void DrawEnemyNameplates();
	void DrawKillFeed();
	void DrawDamageVignette(class ASOLCharacter* MyChar);
	void DrawDeathScreen();

	TArray<FSOLHotRect> ContainerHotRects;
	TArray<FSOLHotRect> BackpackHotRects;

	FString FlashMessage;
	float FlashMessageTime = 0.f;

	// Hit marker: how long it stays up, and whether it was a kill.
	float HitMarkerTime = 0.f;
	bool bHitMarkerWasKill = false;

	// Kill feed entries with their own fade timers.
	struct FSOLFeedLine
	{
		FString Text;
		float TimeLeft = 0.f;
	};
	TArray<FSOLFeedLine> KillFeed;

	// Death screen state (client-side only).
	bool bShowingDeath = false;
	FString DeathKillerName;
	float DeathRespawnLeft = 0.f;

	// Remembers the health seen last frame so a drop can be turned into a
	// screen-edge damage flash without any extra networking.
	float LastSeenHealthRatio = 1.f;
	float DamageFlashTime = 0.f;
};
