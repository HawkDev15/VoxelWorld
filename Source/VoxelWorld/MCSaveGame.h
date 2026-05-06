#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "BlockType.h"
#include "InventoryTypes.h"
#include "MCSaveGame.generated.h"

USTRUCT()
struct FBlockModification
{
	GENERATED_BODY()

	UPROPERTY()
	FIntVector LocalBlock = FIntVector::ZeroValue;

	UPROPERTY()
	EBlockType Type = EBlockType::Air;
};

USTRUCT()
struct FChunkModifications
{
	GENERATED_BODY()

	UPROPERTY()
	FIntPoint ChunkIndex = FIntPoint::ZeroValue;

	UPROPERTY()
	TArray<FBlockModification> Blocks;
};

UCLASS()
class VOXELWORLD_API UMCSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// World
	UPROPERTY()
	int32 Seed = 0;

	UPROPERTY()
	TArray<FChunkModifications> ModifiedChunks;

	// Player
	UPROPERTY()
	FVector PlayerPosition = FVector::ZeroVector;

	UPROPERTY()
	FRotator PlayerRotation = FRotator::ZeroRotator;

	UPROPERTY()
	TArray<FInventorySlot> Inventory;

	UPROPERTY()
	int32 SelectedSlot = 0;
};
