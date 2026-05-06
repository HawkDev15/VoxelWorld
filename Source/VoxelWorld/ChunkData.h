#pragma once

#include "CoreMinimal.h"
#include "BlockType.h"
#include "ChunkData.generated.h"

USTRUCT(BlueprintType)
struct FChunkData
{
	GENERATED_BODY()

	static constexpr int32 DefaultSizeX = 16;
	static constexpr int32 DefaultSizeY = 16;
	static constexpr int32 DefaultSizeZ = 64;

	UPROPERTY()
	int32 SizeX = DefaultSizeX;

	UPROPERTY()
	int32 SizeY = DefaultSizeY;

	UPROPERTY()
	int32 SizeZ = DefaultSizeZ;

	UPROPERTY()
	FIntPoint ChunkIndex = FIntPoint::ZeroValue;

	UPROPERTY()
	TArray<EBlockType> Blocks;

	void Init(const FIntPoint& InChunkIndex, int32 InSizeX, int32 InSizeY, int32 InSizeZ)
	{
		ChunkIndex = InChunkIndex;
		SizeX = InSizeX;
		SizeY = InSizeY;
		SizeZ = InSizeZ;
		Blocks.SetNumZeroed(SizeX * SizeY * SizeZ);
	}

	int32 BlockIndex(int32 X, int32 Y, int32 Z) const
	{
		return X + Y * SizeX + Z * SizeX * SizeY;
	}

	bool InBounds(int32 X, int32 Y, int32 Z) const
	{
		return X >= 0 && X < SizeX && Y >= 0 && Y < SizeY && Z >= 0 && Z < SizeZ;
	}

	EBlockType GetBlock(int32 X, int32 Y, int32 Z) const
	{
		if (!InBounds(X, Y, Z)) return EBlockType::Air;
		return Blocks[BlockIndex(X, Y, Z)];
	}

	void SetBlock(int32 X, int32 Y, int32 Z, EBlockType Type)
	{
		if (!InBounds(X, Y, Z)) return;
		Blocks[BlockIndex(X, Y, Z)] = Type;
	}

	bool IsOpaque(int32 X, int32 Y, int32 Z) const
	{
		if (!InBounds(X, Y, Z)) return false;
		return Blocks[BlockIndex(X, Y, Z)] != EBlockType::Air;
	}
};
