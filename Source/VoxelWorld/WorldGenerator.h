#pragma once

#include "CoreMinimal.h"
#include "ChunkData.h"
#include "WorldGenerator.generated.h"

// Thread-safe generation parameters
USTRUCT(BlueprintType)
struct FWorldGenParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ChunkSizeX = FChunkData::DefaultSizeX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ChunkSizeY = FChunkData::DefaultSizeY;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ChunkSizeZ = FChunkData::DefaultSizeZ;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SeaLevel = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SnowAltitude = 45;

	// Large-scale terrain shape (mountains and valleys)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Continental")
	int32 ContinentalAmplitude = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Continental")
	float ContinentalFrequency = 0.002f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Continental")
	int32 ContinentalOctaves = 3;

	// Small-scale surface detail
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detail")
	int32 DetailAmplitude = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detail")
	float DetailFrequency = 0.015f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detail")
	int32 DetailOctaves = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detail")
	float Persistence = 0.5f;
};

// Static functions, safe to call from any thread
struct VOXELWORLD_API FWorldGenerator
{
	static FChunkData GenerateChunk(const FWorldGenParams& Params, const FIntPoint& ChunkIndex);

private:
	static float SampleNoise(float WorldX, float WorldY, float Frequency,
		int32 Octaves, float Persistence, float SeedOffset);
};
