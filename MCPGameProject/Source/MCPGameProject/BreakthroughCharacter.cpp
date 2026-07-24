// Copyright Epic Games, Inc. All Rights Reserved.

#include "BreakthroughCharacter.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/InputComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"

// Sets default values
ABreakthroughCharacter::ABreakthroughCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Load the UE4 Mannequin (Animation Starter Pack) and assign it to the engine's
	// built-in SkeletalMeshComponent (GetMesh). This is the canonical character body
	// — no extra BodyMesh cylinder needed; the SkeletalMesh shows up at the right
	// height/rotation because Mannequin's reference pose is calibrated for ACharacter.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannequinMeshFinder(
		TEXT("/Game/AnimStarterPack/UE4_Mannequin/Mesh/SK_Mannequin"));
	if (MannequinMeshFinder.Succeeded())
	{
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			MeshComp->SetSkeletalMesh(MannequinMeshFinder.Object);
			// Drop the mesh a bit so its feet sit at the capsule's base (UE4 Mannequin
			// is authored with its origin at the root, which is roughly the pelvis).
			MeshComp->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
			MeshComp->SetVisibility(true);
		}
	}
}

// Called when the game starts or when spawned
void ABreakthroughCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Body is now the SkeletalMeshComponent (UE4 Mannequin) assigned in the constructor.
	// No fallback BodyMesh cylinder to color anymore.

	// Play an idle animation sequence (Animation Starter Pack has no AnimBlueprint,
	// so we use UAnimSequence directly via PlayAnimation — it's a simple loop).
	if (GetMesh())
	{
		UAnimSequence* IdleAnim = LoadObject<UAnimSequence>(
			nullptr, TEXT("/Game/AnimStarterPack/Idle_Rifle_Hip"));
		if (IdleAnim)
		{
			GetMesh()->PlayAnimation(IdleAnim, true /*bLooping*/);
		}
	}

	// Rotate the mesh (not the capsule) to face the level center so the mannequin
	// is visible face-on when the player approaches. UE4 Mannequin's reference pose
	// faces -X; we add 180° to flip it so the front of the body points TOWARD center.
	if (GetMesh())
	{
		FVector ToCenter = FVector(0.f, 0.f, 0.f) - GetActorLocation();
		ToCenter.Z = 0.f;
		if (!ToCenter.IsNearlyZero())
		{
			FRotator LookAtCenter = ToCenter.Rotation();
			GetMesh()->SetWorldRotation(FRotator(0.f, LookAtCenter.Yaw + 180.f, 0.f));
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Character spawned team=%d"), Team);
}

// Called to bind functionality to input（旧版 Input BindAxis，不依赖 Enhanced Input 资产）
void ABreakthroughCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (!PlayerInputComponent) return;

	// 旧版 Axis 绑定（按键映射在 Config/DefaultInput.ini）
	PlayerInputComponent->BindAxis("MoveForward", this, &ABreakthroughCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ABreakthroughCharacter::MoveRight);
	PlayerInputComponent->BindAxis("LookUp", this, &ABreakthroughCharacter::LookUp);
	PlayerInputComponent->BindAxis("Turn", this, &ABreakthroughCharacter::Turn);
}

void ABreakthroughCharacter::MoveForward(float Value)
{
	if (Controller && Value != 0.0f)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(ForwardDirection, Value);
	}
}

void ABreakthroughCharacter::MoveRight(float Value)
{
	if (Controller && Value != 0.0f)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(RightDirection, Value);
	}
}

void ABreakthroughCharacter::LookUp(float Value)
{
	if (Value != 0.0f)
	{
		AddControllerPitchInput(Value);
	}
}

void ABreakthroughCharacter::Turn(float Value)
{
	if (Value != 0.0f)
	{
		AddControllerYawInput(Value);
	}
}
