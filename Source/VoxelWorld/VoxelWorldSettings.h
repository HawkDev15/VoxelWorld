#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BlockType.h"
#include "VoxelWorldSettings.generated.h"

UCLASS(BlueprintType)
class VOXELWORLD_API UVoxelWorldSettings : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blocks")
	TMap<EBlockType, UMaterialInterface*> BlockMaterials;

	// MPC used to pass mining target position and progress to block materials
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blocks")
	class UMaterialParameterCollection* MiningMPC = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blocks")
	TMap<EBlockType, float> BlockMiningTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
	int32 Seed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
	int32 RenderDistance = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
	int32 ChunkSize = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
	int32 ChunkHeight = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	int32 SeaLevel = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	int32 SnowAltitude = 45;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Continental")
	int32 ContinentalAmplitude = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Continental")
	float ContinentalFrequency = 0.002f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Continental")
	int32 ContinentalOctaves = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detail")
	int32 DetailAmplitude = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detail")
	float DetailFrequency = 0.015f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detail")
	int32 DetailOctaves = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detail")
	float Persistence = 0.5f;
};
