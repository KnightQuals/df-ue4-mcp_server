// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SOLPlayerController.generated.h"

class ASOLHUD;
class ASOLContainer;
class ASOLItemPickup;
class USOLBackpackComponent;

// Handles the two interactions of the v1 loot loop:
//  - F: open/close the nearest container, or pick up the nearest dropped item
//  - mouse double-click on a HUD panel row: container -> backpack transfer,
//    or backpack -> drop a world pickup at the player's feet
//
// Multiplayer (DS architecture): the client keeps only local presentation —
// panel open/close, input scanning, the walk-away auto-close. Every state
// mutation goes through a Server RPC (open/roll, take item, drop item, pick
// up); the server validates range and weight, applies the change and the
// minimal replication set (container Contents, owner-only backpack Items,
// pickup Data) carries the result back to the clients.
UCLASS()
class SOL_API ASOLPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASOLPlayerController();

	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaSeconds) override;

	// Client RPC: server-adjudicated result messages shown on the HUD
	// (e.g. pickup succeeded / backpack over capacity / extraction status).
	// Public because the extraction zone (a separate server-side actor) also
	// reports progress and settlement through it.
	UFUNCTION(Client, Reliable)
	void ClientShowMessage(const FString& Message);

	// Client RPC: force the loot panel shut. Called by the extraction zone at
	// settlement — the pawn is destroyed server-side, but the client's HUD
	// still holds an OpenedContainer reference and its input state.
	UFUNCTION(Client, Reliable)
	void ClientClearContainerPanel();

	// Called on the shooter's client when the server confirms a hit, so the
	// crosshair and the kill feed can react. Public because the pawn (a
	// separate actor) is what receives the server's confirmation.
	void OnLocalHitConfirmed(bool bKilled, const FString& VictimName, int32 Bounty);

	// Server-side: this controller's pawn just died. Queues the respawn with
	// the GameMode and tells the client to put up the death screen. Public
	// because the pawn's death handler is the caller.
	void NotifyDeath(class AController* Killer);

protected:
	void OnInteract();
	void OnMouseClick();

	// Left mouse has two jobs: while a loot panel is open it is a UI click
	// (double-click transfers an item), otherwise it fires the weapon. One
	// button, disambiguated by whether the panel owns the cursor — the same
	// convention every looter shooter uses.
	void OnFirePressed();
	void OnReloadPressed();

	// Client RPC: death screen (who killed me, how long until I am back).
	UFUNCTION(Client, Reliable)
	void ClientNotifyDeath(const FString& KillerName, float RespawnSeconds);

	// Server RPCs: the client's only upstream channel is "what I want to
	// do" — never derived state (loot tables, weights, scores are all
	// computed server-side from the server's own world simulation).
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerOpenContainer(ASOLContainer* Container);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerTakeItem(ASOLContainer* Container, int32 ItemIndex);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerDropItem(int32 BackpackIndex);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerTryPickup(ASOLItemPickup* Pickup);

	// Restart the round in place, requested from the settlement screen. Server
	// side re-checks that the round really is over, because "any client may ask"
	// must never mean "any client may wipe a live round".
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestRoundReset();

	// Server-side anti-cheat-lite range check: the requesting pawn must
	// actually stand inside the actor's interaction sphere (+ margin).
	bool IsPawnInRange(const AActor* Target, float ExtraMargin = 100.f) const;

	// Double-click bookkeeping (same row clicked twice inside DoubleClickTime).
	int32 PendingContainerIndex = -1;
	int32 PendingBackpackIndex = -1;
	float LastClickTime = -10.f;
	static constexpr float DoubleClickTime = 0.35f;

	ASOLHUD* GetSOLHUD() const;
	USOLBackpackComponent* GetBackpack() const;

	void OpenContainer(ASOLContainer* Container);
	void CloseContainer();
	void TryPickup(ASOLItemPickup* Pickup);
};
