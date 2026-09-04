// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOLPlayerController.h"
#include "SOL.h"
#include "SOLHUD.h"
#include "SOLCharacter.h"
#include "SOLContainer.h"
#include "SOLGameMode.h"
#include "SOLGameState.h"
#include "SOLItemPickup.h"
#include "SOLBackpackComponent.h"
#include "SOLItemTypes.h"
#include "SOLPlayerState.h"
#include "Components/InputComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

void ASOLPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindAction(TEXT("Interact"), IE_Pressed, this, &ASOLPlayerController::OnInteract);

	// Bound as raw keys rather than through DefaultInput.ini action names:
	// the file already ships with the project and adding entries there would
	// mean an extra config edit for every build, while BindKey keeps the whole
	// mapping visible in one place in code.
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ASOLPlayerController::OnFirePressed);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ASOLPlayerController::OnReloadPressed);
}

void ASOLPlayerController::OnFirePressed()
{
	// Panel open -> the click belongs to the UI (item transfer). Panel closed
	// -> it is a trigger pull. Checking the panel rather than the input mode
	// keeps the two behaviours impossible to trigger at once.
	ASOLHUD* HUD = GetSOLHUD();
	if (HUD && HUD->IsOpen())
	{
		OnMouseClick();
		return;
	}

	if (ASOLCharacter* MyChar = Cast<ASOLCharacter>(GetPawn()))
	{
		MyChar->TryFire();
	}
}

void ASOLPlayerController::OnReloadPressed()
{
	// R has two jobs, split by whether the round is still live. On the
	// settlement screen there is no weapon to reload, so the same key restarts
	// the round — one less thing to explain during a demo.
	const ASOLPlayerState* PS = GetPlayerState<ASOLPlayerState>();
	const ASOLGameState* GS = GetWorld() ? GetWorld()->GetGameState<ASOLGameState>() : nullptr;
	const bool bMyRoundOver = PS && (PS->bExtracted || PS->bFailedExtraction);
	const bool bRoundOver = GS && GS->bRoundOver;
	if (bMyRoundOver || bRoundOver)
	{
		ServerRequestRoundReset();
		return;
	}

	if (ASOLCharacter* MyChar = Cast<ASOLCharacter>(GetPawn()))
	{
		MyChar->TryReload();
	}
}

void ASOLPlayerController::ServerRequestRoundReset_Implementation()
{
	// Any player may restart, but only once the round is actually finished —
	// otherwise a stray R press mid-run would wipe everyone's progress.
	UWorld* World = GetWorld();
	const ASOLPlayerState* PS = GetPlayerState<ASOLPlayerState>();
	const ASOLGameState* GS = World ? World->GetGameState<ASOLGameState>() : nullptr;
	const bool bAllowed = (PS && (PS->bExtracted || PS->bFailedExtraction)) || (GS && GS->bRoundOver);
	if (!bAllowed)
	{
		UE_LOG(LogSOL, Warning, TEXT("Round reset denied for %s: round still running"), *GetName());
		return;
	}

	if (ASOLGameMode* GM = World ? World->GetAuthGameMode<ASOLGameMode>() : nullptr)
	{
		UE_LOG(LogSOL, Log, TEXT("Round reset requested by %s"), *GetName());
		GM->ResetRound();
	}
}

bool ASOLPlayerController::ServerRequestRoundReset_Validate()
{
	return true;
}

void ASOLPlayerController::OnLocalHitConfirmed(bool bKilled, const FString& VictimName, int32 Bounty)
{
	ASOLHUD* HUD = GetSOLHUD();
	if (!HUD)
	{
		return;
	}

	// Hit marker always; a kill line only when something actually died.
	HUD->FlashHitMarker(bKilled);
	if (bKilled)
	{
		HUD->PushKillFeed(Bounty > 0
			? FString::Printf(TEXT("击杀 %s　掉落物资价值 %d"), *VictimName, Bounty)
			: FString::Printf(TEXT("击杀 %s"), *VictimName));
	}
}

void ASOLPlayerController::NotifyDeath(AController* Killer)
{
	// Server-side entry point (the pawn's death handler calls it).
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FString KillerName = TEXT("未知");
	if (Killer)
	{
		// A scavenger's AIController has no PlayerState, so fall back to the
		// pawn's readable name — that is where the 拾荒者 label lives.
		if (const APlayerState* KillerPS = Killer->PlayerState)
		{
			KillerName = KillerPS->GetPlayerName();
		}
		else if (const APawn* KillerPawn = Killer->GetPawn())
		{
			KillerName = KillerPawn->GetHumanReadableName();
		}
	}

	float Delay = 5.f;
	if (ASOLGameMode* GM = World->GetAuthGameMode<ASOLGameMode>())
	{
		Delay = GM->RespawnDelay;
		GM->ScheduleRespawn(this);
	}

	ClientNotifyDeath(KillerName, Delay);
}

void ASOLPlayerController::ClientNotifyDeath_Implementation(const FString& KillerName, float RespawnSeconds)
{
	// The loot panel must go: its pawn is a corpse and its cursor would sit on
	// top of the death screen.
	CloseContainer();

	if (ASOLHUD* HUD = GetSOLHUD())
	{
		HUD->ShowDeathScreen(KillerName, RespawnSeconds);
	}
}

ASOLPlayerController::ASOLPlayerController()
{
	// Tick drives the "walked away -> auto close" check while a panel is open.
	PrimaryActorTick.bCanEverTick = true;
}

void ASOLPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Auto-close: once the pawn leaves the container's interaction sphere
	// (plus a small hysteresis margin so edge-walking does not flap the panel),
	// the loot panel closes itself — operating a container from far away is
	// both a gameplay bug and feels wrong. No toast: the panel visibly
	// closing is feedback enough (user feedback 2026-08-25).
	ASOLHUD* HUD = GetSOLHUD();
	ASOLContainer* Container = HUD ? HUD->OpenedContainer.Get() : nullptr;
	const APawn* MyPawn = GetPawn();
	if (!HUD || !HUD->IsOpen() || !Container || !MyPawn)
	{
		return;
	}
	const float Radius = Container->InteractSphere
		? Container->InteractSphere->GetUnscaledSphereRadius() + 50.f
		: 300.f;
	const float DistSq = FVector::DistSquared2D(MyPawn->GetActorLocation(), Container->GetActorLocation());
	if (DistSq > Radius * Radius)
	{
		CloseContainer();
	}
}

ASOLHUD* ASOLPlayerController::GetSOLHUD() const
{
	return Cast<ASOLHUD>(GetHUD());
}

USOLBackpackComponent* ASOLPlayerController::GetBackpack() const
{
	return GetPawn() ? GetPawn()->FindComponentByClass<USOLBackpackComponent>() : nullptr;
}

void ASOLPlayerController::OnInteract()
{
	ASOLHUD* HUD = GetSOLHUD();
	if (!HUD)
	{
		return;
	}

	// F while a panel is open always closes it first.
	if (HUD->IsOpen())
	{
		CloseContainer();
		return;
	}

	const APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return;
	}

	// Nearest container whose interaction sphere contains the pawn.
	ASOLContainer* BestContainer = nullptr;
	float BestContainerDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<ASOLContainer> It(GetWorld()); It; ++It)
	{
		ASOLContainer* Container = *It;
		if (!Container)
		{
			continue;
		}
		const float Radius = Container->InteractSphere ? Container->InteractSphere->GetUnscaledSphereRadius() : 250.f;
		const float DistSq = FVector::DistSquared2D(MyPawn->GetActorLocation(), Container->GetActorLocation());
		if (DistSq <= Radius * Radius && DistSq < BestContainerDistSq)
		{
			BestContainer = Container;
			BestContainerDistSq = DistSq;
		}
	}
	if (BestContainer)
	{
		OpenContainer(BestContainer);
		return;
	}

	// Otherwise: nearest dropped item pickup.
	ASOLItemPickup* BestPickup = nullptr;
	float BestPickupDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<ASOLItemPickup> It(GetWorld()); It; ++It)
	{
		ASOLItemPickup* Pickup = *It;
		if (!Pickup)
		{
			continue;
		}
		const float Radius = Pickup->PickupSphere ? Pickup->PickupSphere->GetUnscaledSphereRadius() : 150.f;
		const float DistSq = FVector::DistSquared2D(MyPawn->GetActorLocation(), Pickup->GetActorLocation());
		if (DistSq <= Radius * Radius && DistSq < BestPickupDistSq)
		{
			BestPickup = Pickup;
			BestPickupDistSq = DistSq;
		}
	}
	if (BestPickup)
	{
		TryPickup(BestPickup);
	}
}

void ASOLPlayerController::OpenContainer(ASOLContainer* Container)
{
	ASOLHUD* HUD = GetSOLHUD();
	if (!HUD || !Container)
	{
		return;
	}

	// Multiplayer: the panel opens locally right away (presentation), while
	// the loot roll happens on the server — ServerOpenContainer rolls once
	// and the resulting Contents replicate back. The HUD redraws every frame
	// from the replicated array, so the rows appear as soon as the data
	// lands (sub-frame on a 127.0.0.1 server).
	ServerOpenContainer(Container);
	HUD->OpenedContainer = Container;
	PendingContainerIndex = -1;
	PendingBackpackIndex = -1;

	// Show the cursor but keep game input alive so F still closes the panel.
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void ASOLPlayerController::CloseContainer()
{
	ASOLHUD* HUD = GetSOLHUD();
	if (HUD)
	{
		HUD->OpenedContainer = nullptr;
	}
	PendingContainerIndex = -1;
	PendingBackpackIndex = -1;
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
}

void ASOLPlayerController::TryPickup(ASOLItemPickup* Pickup)
{
	ASOLHUD* HUD = GetSOLHUD();
	USOLBackpackComponent* Backpack = GetBackpack();
	if (!Pickup || !Backpack || !HUD)
	{
		return;
	}

	// Multiplayer: weight fitting is adjudicated on the server; the result
	// message comes back through the Client RPC.
	ServerTryPickup(Pickup);
}

void ASOLPlayerController::OnMouseClick()
{
	ASOLHUD* HUD = GetSOLHUD();
	if (!HUD || !HUD->IsOpen())
	{
		return;
	}

	float MouseX = 0.f, MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return;
	}
	const FVector2D MousePos(MouseX, MouseY);
	const float Now = GetWorld()->GetTimeSeconds();

	int32 ContainerIndex = -1;
	if (HUD->HitTestContainer(MousePos, ContainerIndex))
	{
		// Double-click on a container row: request the transfer. The server
		// takes the stack out of the container and adds it to this player's
		// backpack (range + weight validated there); both arrays replicate
		// back and the always-redrawing HUD reflects the new state.
		if (ContainerIndex == PendingContainerIndex && Now - LastClickTime <= DoubleClickTime)
		{
			ASOLContainer* Container = HUD->OpenedContainer.Get();
			if (Container && GetBackpack())
			{
				ServerTakeItem(Container, ContainerIndex);
			}
			PendingContainerIndex = -1;
		}
		else
		{
			PendingContainerIndex = ContainerIndex;
			PendingBackpackIndex = -1;
		}
		LastClickTime = Now;
		return;
	}

	int32 BackpackIndex = -1;
	if (HUD->HitTestBackpack(MousePos, BackpackIndex))
	{
		// Double-click on a backpack row: ask the server to drop a world
		// pickup at the feet. The server spawns it (replicated to all) and
		// removes the stack from this player's owner-only backpack replica.
		if (BackpackIndex == PendingBackpackIndex && Now - LastClickTime <= DoubleClickTime)
		{
			if (GetBackpack() && GetPawn())
			{
				ServerDropItem(BackpackIndex);
			}
			PendingBackpackIndex = -1;
		}
		else
		{
			PendingBackpackIndex = BackpackIndex;
			PendingContainerIndex = -1;
		}
		LastClickTime = Now;
		return;
	}

	// Click on empty space resets the pending double-click state.
	PendingContainerIndex = -1;
	PendingBackpackIndex = -1;
	LastClickTime = Now;
}

bool ASOLPlayerController::IsPawnInRange(const AActor* Target, float ExtraMargin) const
{
	const APawn* MyPawn = GetPawn();
	if (!Target || !MyPawn)
	{
		return false;
	}

	// Containers expose their interaction radius on the sphere component;
	// pickups use the same layout. Fall back to a sane default radius.
	float Radius = 250.f;
	if (const USceneComponent* Root = Target->GetRootComponent())
	{
		if (const USphereComponent* Sphere = Cast<USphereComponent>(Root))
		{
			Radius = Sphere->GetUnscaledSphereRadius();
		}
	}
	Radius += ExtraMargin;

	const float DistSq = FVector::DistSquared2D(MyPawn->GetActorLocation(), Target->GetActorLocation());
	return DistSq <= Radius * Radius;
}

void ASOLPlayerController::ServerOpenContainer_Implementation(ASOLContainer* Container)
{
	// Server authoritative: reject requests from pawns that are not
	// actually standing at the container (a tampered client cannot loot
	// from across the map). Open() rolls the loot exactly once; Contents
	// then replicate to every client.
	if (!Container)
	{
		UE_LOG(LogSOL, Warning, TEXT("ServerOpenContainer: null container from %s"), *GetName());
		return;
	}
	if (!IsPawnInRange(Container))
	{
		// A silent return here is indistinguishable from "the container is
		// empty" on the client, which is exactly how the P2 empty-panel bug
		// hid itself. Log the measured distance and tell the player.
		const APawn* MyPawn = GetPawn();
		const float Dist = MyPawn ? FVector::Dist2D(MyPawn->GetActorLocation(), Container->GetActorLocation()) : -1.f;
		UE_LOG(LogSOL, Warning, TEXT("ServerOpenContainer REJECTED: %s too far from %s (%.0fcm)"),
			*GetName(), *Container->ContainerName.ToString(), Dist);
		ClientShowMessage(TEXT("距离太远，靠近后再打开"));
		return;
	}
	Container->Open();
}

bool ASOLPlayerController::ServerOpenContainer_Validate(ASOLContainer* Container)
{
	// Pointer sanity only — range and state checks live in the
	// implementation so legit races (container just destroyed) degrade
	// gracefully instead of disconnecting the player.
	return true;
}

void ASOLPlayerController::ServerTakeItem_Implementation(ASOLContainer* Container, int32 ItemIndex)
{
	USOLBackpackComponent* Backpack = GetBackpack();
	if (!Container || !Backpack)
	{
		UE_LOG(LogSOL, Warning, TEXT("ServerTakeItem: missing container/backpack for %s"), *GetName());
		return;
	}
	if (!IsPawnInRange(Container))
	{
		UE_LOG(LogSOL, Warning, TEXT("ServerTakeItem REJECTED: %s out of range of %s"),
			*GetName(), *Container->ContainerName.ToString());
		ClientShowMessage(TEXT("距离太远，靠近后再拿取"));
		return;
	}

	FSOLItemInstance Stack = Container->TakeItemAt(ItemIndex);
	if (Stack.Count <= 0)
	{
		// Two players clicking the same stack: the loser gets nothing.
		// Report it so the race is visible instead of feeling like a dead click.
		UE_LOG(LogSOL, Log, TEXT("ServerTakeItem: %s lost the race for slot %d in %s"),
			*GetName(), ItemIndex, *Container->ContainerName.ToString());
		ClientShowMessage(TEXT("这件物品已被拿走"));
		return;
	}

	if (Backpack->AddItem(Stack))
	{
		ClientShowMessage(FString::Printf(TEXT("放入背包：%s ×%d"), *Stack.Def.DisplayName, Stack.Count));
	}
	else
	{
		// Over capacity: put it back into the container.
		Container->Contents.Add(Stack);
		ClientShowMessage(TEXT("背包超重，放不下"));
	}
}

bool ASOLPlayerController::ServerTakeItem_Validate(ASOLContainer* Container, int32 ItemIndex)
{
	return true;
}

void ASOLPlayerController::ServerDropItem_Implementation(int32 BackpackIndex)
{
	USOLBackpackComponent* Backpack = GetBackpack();
	APawn* MyPawn = GetPawn();
	if (!Backpack || !MyPawn)
	{
		return;
	}

	FSOLItemInstance Stack = Backpack->RemoveItemAt(BackpackIndex);
	if (Stack.Count <= 0)
	{
		return;
	}

	// Spawn the world pickup on the server (ground-snapped so it never
	// hovers); it replicates to every client and OnRep_Data rebuilds the
	// label on each of them.
	const FVector DropLocation = MyPawn->GetActorLocation()
		+ MyPawn->GetActorForwardVector() * 120.f;
	ASOLItemPickup::SpawnGrounded(GetWorld(), DropLocation, Stack);
	ClientShowMessage(FString::Printf(TEXT("丢弃：%s ×%d"), *Stack.Def.DisplayName, Stack.Count));
}

bool ASOLPlayerController::ServerDropItem_Validate(int32 BackpackIndex)
{
	return true;
}

void ASOLPlayerController::ServerTryPickup_Implementation(ASOLItemPickup* Pickup)
{
	USOLBackpackComponent* Backpack = GetBackpack();
	if (!Pickup || !Backpack)
	{
		return;
	}
	if (!IsPawnInRange(Pickup))
	{
		UE_LOG(LogSOL, Warning, TEXT("ServerTryPickup REJECTED: %s out of range"), *GetName());
		ClientShowMessage(TEXT("距离太远，靠近后再拾取"));
		return;
	}

	if (Backpack->AddItem(Pickup->Data))
	{
		ClientShowMessage(FString::Printf(TEXT("拾取：%s ×%d"), *Pickup->Data.Def.DisplayName, Pickup->Data.Count));
		Pickup->Destroy();
	}
	else
	{
		ClientShowMessage(TEXT("背包超重，放不下"));
	}
}

bool ASOLPlayerController::ServerTryPickup_Validate(ASOLItemPickup* Pickup)
{
	return true;
}

void ASOLPlayerController::ClientShowMessage_Implementation(const FString& Message)
{
	ASOLHUD* HUD = GetSOLHUD();
	if (HUD)
	{
		HUD->ShowMessage(Message);
	}
}

void ASOLPlayerController::ClientClearContainerPanel_Implementation()
{
	// CloseContainer already does the full local teardown: drop the panel
	// reference, reset the pending double-click indices, hide the cursor and
	// restore game-only input. Reusing it keeps one definition of "closed".
	CloseContainer();
}
