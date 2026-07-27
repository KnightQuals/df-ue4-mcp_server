// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BreakthroughCharacter.generated.h"

class UStaticMeshComponent;
class UCameraComponent;

// Forward declarations for Enhanced Input assets.
class UInputMappingContext;
class UInputAction;

// Playable character for the Breakthrough game mode. Team assignment (0 = attacker,
// 1 = defender) drives which side of the capture logic this pawn counts toward.
// V1 simplification: no replication, default character movement component.
// 旧版 Input（BindAxis）生效，但保留 EnhancedInput UPROPERTY 兼容 BP_BreakthroughCharacter 蓝图变量引用。
UCLASS()
class MCPGAMEPROJECT_API ABreakthroughCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABreakthroughCharacter();

	// First-person camera attached to the capsule, positioned at eye height (~1.7m).
	// Player sees the world through this camera; the SkeletalMesh is hidden from the
	// owning player (OwnerNoSee) so it doesn't occlude the view, but other players
	// (and the defender AI pawn) still see the mannequin body.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* FirstPersonCamera;

	// Team this character belongs to: 0 = attackers, 1 = defenders.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	int32 Team = 0;

	// 保留 UPROPERTY 兼容 BP_BreakthroughCharacter 蓝图变量引用（不实际使用）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* DefaultIMC = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* MoveAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* LookAction = nullptr;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// 旧版 Input Axis 回调
	void MoveForward(float Value);
	void MoveRight(float Value);
	void LookUp(float Value);
	void Turn(float Value);
};
