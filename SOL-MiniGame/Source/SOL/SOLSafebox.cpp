// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOLSafebox.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"

ASOLSafebox::ASOLSafebox()
{
	// 贵重物品藏深处：交互半径比普通容器小（250 -> 180）。
	InteractSphere->InitSphereRadius(180.f);

	// 厚实的小箱子比例。
	ContainerMesh->SetRelativeScale3D(FVector(0.9f, 0.6f, 0.8f));

	ContainerName = TEXT("SAFEBOX");
	DisplayNameZh = TEXT("保险箱");
}
