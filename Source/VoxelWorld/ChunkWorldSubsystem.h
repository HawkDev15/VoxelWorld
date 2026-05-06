#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ChunkData.h"
#include "WorldMeshActor.h"
#include "WorldGenerator.h"
#include "ChunkWorldSubsystem.generated.h"

class UVoxelWorldSettings;

UCLASS()
class VOXELWORLD_API UChunkWorldSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	TObjectPtr<UVoxelWorldSettings> Settings;

	// Returns the block type at the given world position. Air if out of bounds or not loaded.
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	EBlockType GetBlock(const FVector& HitLocation) const;

	// Direct block lookup without FindSolidBlock. For use when coords are already known.
	EBlockType GetBlockAt(const FIntPoint& ChunkIndex, const FIntVector& LocalBlock) const;

	// Returns mining time for the given block type. Defaults to 1.0 if not configured.
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	float GetBlockMiningTime(EBlockType Type) const;

	// Removes the solid block at the hit surface. Returns true if a block was removed.
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	bool RemoveBlock(const FVector& HitLocation);

	// Places a block on the face of the solid block that was hit. Returns true if placed.
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	bool PlaceBlock(const FVector& HitLocation, EBlockType Type);

	// Returns chunk index and local block coordinates of the solid block at the hit surface.
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	bool GetBlockCoords(const FVector& HitLocation,
		FIntPoint& OutChunkIndex, FIntVector& OutLocalBlock) const;

	// Computes world-space center from chunk index and local block coordinates.
	FVector BlockWorldCenter(const FIntPoint& ChunkIndex, const FIntVector& LocalBlock) const;

	// Returns the world-space center of the empty cell where a block would be placed.
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	bool GetPlaceBlockWorldCenter(const FVector& HitLocation, FVector& OutCenter) const;

	// Sets the mining target block and progress (0.0 to 1.0) for visual feedback via MPC.
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	void SetMiningProgress(const FVector& BlockCenter, float Progress);

	// Clears the mining visual (sets progress to 0).
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	void ClearMiningProgress();

	int32 GetSeed() const { return GenParams.Seed; }
	int32 GetChunkSizeZ() const { return ChunkSizeZ; }

	// Sets seed before first chunk generation (used by load)
	void SetSeed(int32 InSeed);

	// Returns all block modifications made by the player
	const TMap<FIntPoint, TMap<FIntVector, EBlockType>>& GetModifiedBlocks() const { return ModifiedBlocks; }

	// Applies loaded modifications (call before chunks generate)
	void SetModifiedBlocks(const TMap<FIntPoint, TMap<FIntVector, EBlockType>>& InModified);

	// Clears all chunks, cache, and modifications. Call before loading a different world.
	void ResetWorld();

	// Synchronously generates and applies chunks around a world position (for spawn/load)
	void ForceLoadChunksAt(const FVector& WorldPos);

private:
	bool FindSolidBlock(const FVector& HitLocation,
		FIntPoint& OutChunkIndex, FIntVector& OutLocalBlock) const;

	bool FindAdjacentEmpty(const FVector& HitLocation,
		FIntPoint& OutChunkIndex, FIntVector& OutLocalBlock) const;

	void GlobalToLocal(int32 GlobalX, int32 GlobalY, int32 GlobalZ,
		FIntPoint& OutChunkIndex, FIntVector& OutLocalBlock) const;

	void QueueRebuild(const FIntPoint& ChunkIndex);
	void ProcessRebuildQueue();

	struct FAsyncChunkResult
	{
		FIntPoint ChunkIndex;
		FChunkData Data;
		FPreparedChunkMesh Mesh;
	};

	FWorldGenParams GenParams;

	// Chunk dimensions, set once in Initialize from DataAsset
	int32 ChunkSizeX = FChunkData::DefaultSizeX;
	int32 ChunkSizeY = FChunkData::DefaultSizeY;
	int32 ChunkSizeZ = FChunkData::DefaultSizeZ;

	TMap<FIntPoint, FChunkData> ChunkCache;

	// Tracks all player modifications (delta from generated terrain)
	TMap<FIntPoint, TMap<FIntVector, EBlockType>> ModifiedBlocks;
	TSet<FIntPoint> ActiveChunks;
	TSet<FIntPoint> InFlightChunks;
	TArray<FIntPoint> PendingLoadQueue;
	TArray<FAsyncChunkResult> PendingApply;
	TArray<FIntPoint> RebuildQueue;
	TSet<FIntPoint> RebuildSet;

	// MPSC queue: TaskGraph threads enqueue, game thread dequeues in Tick.
	// Shared pointer ensures the queue outlives the subsystem if async tasks are still in flight.
	TSharedPtr<TQueue<FAsyncChunkResult>> ReadyQueue = MakeShared<TQueue<FAsyncChunkResult>>();

	UPROPERTY()
	AWorldMeshActor* WorldMesh = nullptr;

	int32 RenderDistance = 4;
	int32 ChunksPerFrame = 2;
	int32 MaxInFlight = 4;

	void EnsureWorldMesh();
	FIntPoint WorldToChunkIndex(const FVector& WorldPos) const;
	void RequestChunkAsync(const FIntPoint& Index);
	void ApplyReadyChunks(const TSet<FIntPoint>& DesiredChunks);
	void UnloadChunk(const FIntPoint& Index);
};
