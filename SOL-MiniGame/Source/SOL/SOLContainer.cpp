// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOLContainer.h"
#include "SOL.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/TextRenderComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Math/UnrealMathUtility.h"
#include "Net/UnrealNetwork.h"

ASOLContainer::ASOLContainer()
{
	PrimaryActorTick.bCanEverTick = false;

	// Replication is opt-in per actor: bReplicates=true is the prerequisite
	// for any UPROPERTY(Replicated) to flow over the wire. Without this
	// flag the server's Contents/bOpened never reaches P2 (container panel
	// is permanently empty on remote clients). NetUpdateFrequency=30 bumps
	// the property sync to 30Hz so a freshly opened container's Contents
	// replicate promptly without waiting for the next periodic sync.
	bReplicates = true;
	NetUpdateFrequency = 30.f;

	InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
	InteractSphere->InitSphereRadius(250.f);
	InteractSphere->SetCollisionProfileName(TEXT("OverlapOnlyPawn"));
	RootComponent = InteractSphere;

	ContainerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ContainerMesh"));
	ContainerMesh->SetupAttachment(RootComponent);
	ContainerMesh->SetRelativeScale3D(FVector(1.2f, 0.8f, 0.8f));
	ContainerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'"));
	if (CubeMesh.Succeeded())
	{
		ContainerMesh->SetStaticMesh(CubeMesh.Object);
	}

	LabelComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	LabelComponent->SetupAttachment(RootComponent);
	LabelComponent->SetRelativeLocation(FVector(0.f, 0.f, 140.f));
	LabelComponent->SetHorizontalAlignment(EHTA_Center);
	LabelComponent->SetTextRenderColor(FColor::White);
	LabelComponent->SetWorldSize(36.f);
}

void ASOLContainer::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// World labels stay ASCII — TextRender has no CJK glyph fallback.
	LabelComponent->SetText(FText::FromString(ContainerName.ToString()));
}

void ASOLContainer::PostLoad()
{
	Super::PostLoad();

	// Containers already placed in SOLNewMap were saved before replication
	// was introduced, so the map serialised bReplicates=false onto them.
	// A constructor only runs when an object is *created*; for a loaded
	// instance the serialised archive is applied afterwards and wins, which
	// silently reverts the flag and leaves remote clients with empty panels.
	// Force the networked defaults here so placed, duplicated and future
	// instances all end up replicated without hand-editing every actor.
	bReplicates = true;
	NetDormancy = DORM_Never;
	NetUpdateFrequency = 30.f;
}

void ASOLContainer::BeginPlay()
{
	Super::BeginPlay();

	// World ASCII labels are replaced by the HUD's Chinese screen-space
	// nameplates (TextRender only supports offline fonts, which have no CJK
	// glyphs), so hide the legacy 3D label during play.
	LabelComponent->SetVisibility(false);

	if (ItemPool.Num() == 0)
	{
		if (!TryLoadConfigRow())
		{
			ApplyDefaultPool();
		}
	}
}

FString ASOLContainer::GetDisplayNameZh() const
{
	if (!DisplayNameZh.IsEmpty())
	{
		return DisplayNameZh;
	}
	// Legacy ASCII names that existed before the Chinese name field.
	// Kept as a fallback so a container still reads correctly when its
	// DataTable row is missing (the subclasses set DisplayNameZh directly).
	const FString Ascii = ContainerName.ToString();
	static const TMap<FString, FString> Fallback = {
		{TEXT("CAGE"),       TEXT("鸟笼")},
		{TEXT("BOX"),        TEXT("补给箱")},
		{TEXT("LUGGAGE"),    TEXT("行李箱")},
		{TEXT("SAFEBOX"),    TEXT("保险箱")},
		{TEXT("ARMORY"),     TEXT("军械箱")},
		{TEXT("SERVERRACK"), TEXT("服务器机柜")},
		{TEXT("CONTRABAND"), TEXT("走私箱")},
		{TEXT("MEDSTATION"), TEXT("医疗站")},
	};
	if (const FString* Found = Fallback.Find(Ascii))
	{
		return *Found;
	}
	return Ascii;
}

bool ASOLContainer::TryLoadConfigRow()
{
	// Design doc: 对局道具配置走 DataTable，改表不重编译。Row key follows the
	// naming convention: the actor name (e.g. SOL_Cage_1) unless ConfigRow is set.
	UDataTable* Table = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *ConfigTablePath));
	if (!Table || !Table->RowStruct)
	{
		UE_LOG(LogSOL, Verbose, TEXT("Container %s: config table not found at %s, using default pool"),
			*GetName(), *ConfigTablePath);
		return false;
	}

	// MCP-spawned actors get engine-assigned "_N" suffixes (SOL_Cage_1), so
	// after the exact-key miss we retry with the numeric suffix stripped —
	// any SOL_Cage_* then matches the row keyed "SOL_Cage".
	FName RowKey = ConfigRow != NAME_None ? ConfigRow : GetFName();
	const FSOLContainerRow* Row = Table->FindRow<FSOLContainerRow>(RowKey, TEXT("SOLContainerConfig"));
	if (!Row && ConfigRow == NAME_None)
	{
		FString Stem = GetFName().ToString();
		int32 UnderscoreIdx = INDEX_NONE;
		if (Stem.FindLastChar(TEXT('_'), UnderscoreIdx) && Stem.Mid(UnderscoreIdx + 1).IsNumeric())
		{
			Stem = Stem.Left(UnderscoreIdx);
			Row = Table->FindRow<FSOLContainerRow>(FName(*Stem), TEXT("SOLContainerConfig"));
			if (Row)
			{
				RowKey = FName(*Stem);
			}
		}
	}
	if (!Row)
	{
		UE_LOG(LogSOL, Verbose, TEXT("Container %s: no row '%s' in %s, using default pool"),
			*GetName(), *RowKey.ToString(), *ConfigTablePath);
		return false;
	}

	if (Row->ContainerName != NAME_None)
	{
		ContainerName = Row->ContainerName;
	}
	if (!Row->DisplayNameZh.IsEmpty())
	{
		DisplayNameZh = Row->DisplayNameZh;
	}
	NumRolls = Row->NumRolls > 0 ? Row->NumRolls : NumRolls;
	ItemPool = Row->Items;

	UE_LOG(LogSOL, Log, TEXT("Container %s: loaded %d pool item(s) from row '%s' (zh=%s)"),
		*GetName(), ItemPool.Num(), *RowKey.ToString(), *DisplayNameZh);
	return ItemPool.Num() > 0;
}

void ASOLContainer::ApplyDefaultPool()
{
	// Last-resort pool used only when the DataTable row is missing. Two rules:
	//  1) never touch ContainerName / DisplayNameZh — the subclasses (armory
	//     crate, server rack, ...) set those in their constructors and an
	//     overwrite here used to rename a 军械箱 into a 鸟笼;
	//  2) always fill Value explicitly. FSOLItemDef defaults to 100, so an
	//     omitted value silently made every fallback item worth the same and
	//     flattened the whole extraction economy.
	// Pool is picked deterministically from the actor name hash — the same
	// actor name always gets the same pool, which keeps demos reproducible.
	const uint32 Hash = GetTypeHash(GetFName().ToString());
	const int32 PoolIndex = Hash % 3;

	if (PoolIndex == 0)
	{
		// Treasure-leaning: light, valuable, rewards a careful search.
		ItemPool = {
			{ TEXT("GOLD_SKULL"), TEXT("黄金骷髅"), 1.5f, 5.f, 800 },
			{ TEXT("HEART_OF_AFRICA"), TEXT("非洲之心"), 0.5f, 1.f, 1200 },
			{ TEXT("COINS"), TEXT("金币"), 2.0f, 8.f, 120 },
		};
	}
	else if (PoolIndex == 1)
	{
		// Utility-leaning: heavy, cheap — the pool that punishes hoarding.
		ItemPool = {
			{ TEXT("MEDKIT"), TEXT("医疗包"), 1.0f, 6.f, 80 },
			{ TEXT("AMMO"), TEXT("弹药"), 3.0f, 7.f, 60 },
			{ TEXT("COINS"), TEXT("金币"), 2.0f, 4.f, 120 },
		};
	}
	else
	{
		// Electronics-leaning: mid weight, mid value.
		ItemPool = {
			{ TEXT("LAPTOP"), TEXT("笔记本电脑"), 2.5f, 5.f, 250 },
			{ TEXT("HEART_OF_AFRICA"), TEXT("非洲之心"), 0.5f, 2.f, 1200 },
			{ TEXT("WATCH"), TEXT("名表"), 0.3f, 9.f, 300 },
		};
	}
	LabelComponent->SetText(FText::FromString(ContainerName.ToString()));
}

void ASOLContainer::ResetForNewRound()
{
	if (!HasAuthority())
	{
		return;
	}
	Contents.Reset();
	bOpened = false;

	// Both fields are replicated, so every client's panel empties and the
	// container goes back to looking unopened without any extra message.
	UE_LOG(LogSOL, Verbose, TEXT("Container %s re-locked for a new round"), *GetName());
}

void ASOLContainer::Open()
{
	if (bOpened)
	{
		return;
	}
	// Server authoritative: loot is rolled exactly once, on the server.
	// The rolled Contents then replicate to every client; clients calling
	// Open() (should not happen — the player controller sends an RPC
	// instead) are a harmless no-op.
	if (!HasAuthority())
	{
		return;
	}
	bOpened = true;

	// Weighted draw, NumRolls times; identical items stack into one entry.
	const float TotalWeight = [](const TArray<FSOLItemDef>& Pool)
	{
		float Sum = 0.f;
		for (const FSOLItemDef& Def : Pool)
		{
			Sum += FMath::Max(0.f, Def.SpawnWeight);
		}
		return Sum;
	}(ItemPool);

	if (TotalWeight <= 0.f || ItemPool.Num() == 0)
	{
		return;
	}

	for (int32 Roll = 0; Roll < NumRolls; ++Roll)
	{
		float Pick = FMath::FRandRange(0.f, TotalWeight);
		for (const FSOLItemDef& Def : ItemPool)
		{
			Pick -= FMath::Max(0.f, Def.SpawnWeight);
			if (Pick <= 0.f)
			{
				bool bMerged = false;
				for (FSOLItemInstance& Inst : Contents)
				{
					if (Inst.Def.ItemID == Def.ItemID)
					{
						++Inst.Count;
						bMerged = true;
						break;
					}
				}
				if (!bMerged)
				{
					Contents.Add({ Def, 1 });
				}
				break;
			}
		}
	}

	UE_LOG(LogSOL, Log, TEXT("Container %s opened, rolled %d stack(s)"), *ContainerName.ToString(), Contents.Num());
}

FSOLItemInstance ASOLContainer::TakeItemAt(int32 Index)
{
	// Server authoritative: only the authority mutates Contents; a client
	// replica would only modify its local copy which the next replication
	// update would bounce back anyway.
	if (!HasAuthority() || !Contents.IsValidIndex(Index))
	{
		return FSOLItemInstance();
	}
	FSOLItemInstance Out = Contents[Index];
	Contents.RemoveAt(Index);
	return Out;
}

void ASOLContainer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Minimal replication set: the rolled loot (and its opened flag) is the
	// only container state other players need to see. ItemPool / config
	// fields are static per-match and are loaded from the same DataTable on
	// both ends — no need to ship them over the wire.
	DOREPLIFETIME(ASOLContainer, Contents);
	DOREPLIFETIME(ASOLContainer, bOpened);
}

void ASOLContainer::OnRep_Contents()
{
	// Replication downlink landed on this client. Logged so we can confirm
	// from the log that the server's rolled loot is arriving at every
	// client's container instance (P1 main window + P2 small window).
	UE_LOG(LogSOL, Log, TEXT("Container %s: OnRep_Contents, %d stack(s) (bOpened=%s)"),
		*ContainerName.ToString(), Contents.Num(), bOpened ? TEXT("true") : TEXT("false"));
}

void ASOLContainer::OnRep_bOpened()
{
	UE_LOG(LogSOL, Verbose, TEXT("Container %s: OnRep_bOpened=%s"),
		*ContainerName.ToString(), bOpened ? TEXT("true") : TEXT("false"));
}
