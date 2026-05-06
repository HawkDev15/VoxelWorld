#include "MCPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "VoxelWorld/ChunkWorldSubsystem.h"
#include "VoxelWorld/WorldMeshActor.h"
#include "DrawDebugHelpers.h"

AMCPlayerCharacter::AMCPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->SetRelativeLocation(FVector(0.0, 0.0, 70.0));
	SpringArm->TargetArmLength = 0.0f;
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bDoCollisionTest = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;

	// Default inventory: 32 of each block type
	Inventory.SetNum(4);
	Inventory[0] = { EBlockType::Stone, 32 };
	Inventory[1] = { EBlockType::Dirt,  32 };
	Inventory[2] = { EBlockType::Grass, 32 };
	Inventory[3] = { EBlockType::Snow,  32 };
}

void AMCPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateTrace();

	const float Extent = AWorldMeshActor::BlockSize * 0.51f;
	if (bHasAimedBlock)
	{
		DrawDebugBox(GetWorld(), AimedBlockCenter, FVector(Extent), FColor::Red, false, 0.f, 0, 1.f);
	}
	if (bHasPlaceBlock)
	{
		DrawDebugBox(GetWorld(), PlaceBlockCenter, FVector(Extent), FColor::Green, false, 0.f, 0, 1.f);
	}
}

void AMCPlayerCharacter::UpdateTrace()
{
	bHasAimedBlock = false;
	bHasPlaceBlock = false;

	const FVector TraceStart = Camera->GetComponentLocation();
	const FVector TraceEnd = TraceStart + Camera->GetForwardVector() * 500.0f;

	FHitResult HitResult;
	bTraceHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility);

	if (!bTraceHit)
	{
		return;
	}

	TraceHitLocation = HitResult.Location;

	UChunkWorldSubsystem* Sub = GetWorld()->GetSubsystem<UChunkWorldSubsystem>();
	if (!Sub)
	{
		return;
	}

	if (Sub->GetBlockCoords(TraceHitLocation, AimedChunkIndex, AimedBlockCoords))
	{
		bHasAimedBlock = true;
		AimedBlockCenter = Sub->BlockWorldCenter(AimedChunkIndex, AimedBlockCoords);
		AimedBlockType = Sub->GetBlockAt(AimedChunkIndex, AimedBlockCoords);
	}

	bHasPlaceBlock = Sub->GetPlaceBlockWorldCenter(TraceHitLocation, PlaceBlockCenter);
}

void AMCPlayerCharacter::OnUseTriggered()
{
	if (!bHasAimedBlock)
	{
		ClearMine();
		return;
	}

	const bool bSameBlock = bHasTarget && (AimedBlockCoords == TargetBlockCoords);

	if (bSameBlock && bIsMining)
	{
		// Still mining the same block — update progress visual
		UChunkWorldSubsystem* Sub = GetWorld()->GetSubsystem<UChunkWorldSubsystem>();
		if (!Sub)
		{
			return;
		}

		const float Remaining = GetWorldTimerManager().GetTimerRemaining(MiningTimerHandle);
		const float Total = Sub->GetBlockMiningTime(TargetBlockType);
		const float Progress = (Total > 0.f) ? FMath::Clamp(1.f - (Remaining / Total), 0.f, 1.f) : 1.f;
		Sub->SetMiningProgress(AimedBlockCenter, Progress);
		return;
	}

	// New block or not mining yet — start fresh
	ClearMine();
	bHasTarget = true;
	TargetBlockCoords = AimedBlockCoords;
	TargetBlockType = AimedBlockType;
	MiningHitLocation = TraceHitLocation;
	StartMining();
}

void AMCPlayerCharacter::OnUseCompleted()
{
	ClearMine();
}

void AMCPlayerCharacter::StartMining()
{
	UChunkWorldSubsystem* Sub = GetWorld()->GetSubsystem<UChunkWorldSubsystem>();
	if (!Sub)
	{
		return;
	}

	const float Duration = Sub->GetBlockMiningTime(TargetBlockType);
	bIsMining = true;
	GetWorldTimerManager().SetTimer(MiningTimerHandle, this, &AMCPlayerCharacter::CompleteMining, Duration, false);
}

void AMCPlayerCharacter::ClearMine()
{
	bIsMining = false;
	bHasTarget = false;
	TargetBlockCoords = FIntVector::ZeroValue;
	TargetBlockType = EBlockType::Air;
	GetWorldTimerManager().ClearTimer(MiningTimerHandle);

	if (UChunkWorldSubsystem* Sub = GetWorld()->GetSubsystem<UChunkWorldSubsystem>())
	{
		Sub->ClearMiningProgress();
	}
}

void AMCPlayerCharacter::CompleteMining()
{
	if (UChunkWorldSubsystem* Sub = GetWorld()->GetSubsystem<UChunkWorldSubsystem>())
	{
		if (Sub->RemoveBlock(MiningHitLocation))
		{
			// Add the destroyed block to matching inventory slot
			for (FInventorySlot& Slot : Inventory)
			{
				if (Slot.Type == TargetBlockType)
				{
					++Slot.Count;
					break;
				}
			}
		}
	}

	ClearMine();
}

void AMCPlayerCharacter::SelectSlot(int32 SlotIndex)
{
	if (SlotIndex >= 0 && SlotIndex < Inventory.Num())
	{
		SelectedSlot = SlotIndex;
	}
}

void AMCPlayerCharacter::PlaceSelectedBlock()
{
	if (!bTraceHit || !Inventory.IsValidIndex(SelectedSlot))
	{
		return;
	}

	FInventorySlot& Slot = Inventory[SelectedSlot];
	if (Slot.Type == EBlockType::Air || Slot.Count <= 0)
	{
		return;
	}

	if (UChunkWorldSubsystem* Sub = GetWorld()->GetSubsystem<UChunkWorldSubsystem>())
	{
		if (Sub->PlaceBlock(TraceHitLocation, Slot.Type))
		{
			--Slot.Count;
		}
	}
}
