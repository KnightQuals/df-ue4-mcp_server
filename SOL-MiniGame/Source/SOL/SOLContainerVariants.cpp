// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOLContainerVariants.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"

ASOLArmoryCrate::ASOLArmoryCrate()
{
	// 军械箱：宽扁的大箱子，站在旁边就能开。
	InteractSphere->InitSphereRadius(280.f);
	ContainerMesh->SetRelativeScale3D(FVector(1.8f, 1.0f, 0.7f));
	ContainerName = TEXT("ARMORY");
	DisplayNameZh = TEXT("军械箱");
}

ASOLServerRack::ASOLServerRack()
{
	// 服务器机柜：高瘦，要走到跟前才能操作。
	InteractSphere->InitSphereRadius(200.f);
	ContainerMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 2.2f));
	ContainerName = TEXT("SERVERRACK");
	DisplayNameZh = TEXT("服务器机柜");
}

ASOLContrabandCase::ASOLContrabandCase()
{
	// 走私箱：最小的目标、最高的价值密度，容易走过头。
	InteractSphere->InitSphereRadius(160.f);
	ContainerMesh->SetRelativeScale3D(FVector(0.8f, 0.5f, 0.45f));
	ContainerName = TEXT("CONTRABAND");
	DisplayNameZh = TEXT("走私箱");
}

ASOLMedStation::ASOLMedStation()
{
	// 医疗站：低矮宽大，本来就是给人急着找到的。
	InteractSphere->InitSphereRadius(300.f);
	ContainerMesh->SetRelativeScale3D(FVector(1.5f, 1.2f, 0.5f));
	ContainerName = TEXT("MEDSTATION");
	DisplayNameZh = TEXT("医疗站");
}
