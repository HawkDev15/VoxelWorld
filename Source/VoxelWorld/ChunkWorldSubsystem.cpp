#include "ChunkWorldSubsystem.h"
#include "VoxelWorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
#include "Materials/MaterialParameterCollectionInstance.h"

void UChunkWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!Settings)
	{
		Settings = LoadObject<UVoxelWorldSettings>(nullptr,
			TEXT("/Game/Data/DA_WorldSettings.DA_WorldSettings"));
	}

	if (Settings)
	{
		ChunkSizeX = Settings->ChunkSize;
		ChunkSizeY = Settings->ChunkSize;
		ChunkSizeZ = Settings->ChunkHeight;

		RenderDistance = Settings->RenderDistance;
		GenParams.ChunkSizeX = ChunkSizeX;
		GenParams.ChunkSizeY = ChunkSizeY;
		GenParams.ChunkSizeZ = ChunkSizeZ;
		GenParams.Seed = (Settings->Seed == 0) ? FMath::Rand() : Settings->Seed;
		GenParams.SeaLevel = Settings->SeaLevel;
		GenParams.SnowAltitude = Settings->SnowAltitude;
		GenParams.ContinentalAmplitude = Settings->ContinentalAmplitude;
		GenParams.ContinentalFrequency = Settings->ContinentalFrequency;
		GenParams.ContinentalOctaves = Settings->ContinentalOctaves;
		GenParams.DetailAmplitude = Settings->DetailAmplitude;
		GenParams.DetailFrequency = Settings->DetailFrequency;
		GenParams.DetailOctaves = Settings->DetailOctaves;
		GenParams.Persistence = Settings->Persistence;
	}
}

void UChunkWorldSubsystem::EnsureWorldMesh()
{
	if (WorldMesh && !IsValid(WorldMesh))
	{
		WorldMesh = nullptr;
	}

	if (WorldMesh)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Clean up leftover actors from previous PIE sessions
	TArray<AActor*> StaleActors;
	UGameplayStatics::GetAllActorsOfClass(World, AWorldMeshActor::StaticClass(), StaleActors);
	for (AActor* Stale : StaleActors)
	{
		Stale->Destroy();
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	WorldMesh = World->SpawnActor<AWorldMeshActor>(
		AWorldMeshActor::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		Params);

	if (WorldMesh && Settings)
	{
		WorldMesh->BlockMaterials = Settings->BlockMaterials;
	}
}

void UChunkWorldSubsystem::Deinitialize()
{
	// Drain async queue (shared ptr keeps it alive if tasks are still in flight)
	FAsyncChunkResult Result;
	while (ReadyQueue->Dequeue(Result)) {}

	if (WorldMesh)
	{
		WorldMesh->Destroy();
		WorldMesh = nullptr;
	}

	ActiveChunks.Empty();
	ChunkCache.Empty();
	InFlightChunks.Empty();
	PendingApply.Empty();
	PendingLoadQueue.Empty();
	RebuildQueue.Empty();
	RebuildSet.Empty();

	Super::Deinitialize();
}

void UChunkWorldSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!WorldMesh)
	{
		EnsureWorldMesh();
		return;
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!PlayerPawn)
	{
		return;
	}

	const FIntPoint PlayerChunk = WorldToChunkIndex(PlayerPawn->GetActorLocation());

	TSet<FIntPoint> DesiredChunks;
	for (int32 DX = -RenderDistance; DX <= RenderDistance; ++DX)
	{
		for (int32 DY = -RenderDistance; DY <= RenderDistance; ++DY)
		{
			DesiredChunks.Add(FIntPoint(PlayerChunk.X + DX, PlayerChunk.Y + DY));
		}
	}

	// Unload chunks that left render distance
	TArray<FIntPoint> ToUnload;
	for (const FIntPoint& Index : ActiveChunks)
	{
		if (!DesiredChunks.Contains(Index))
		{
			ToUnload.Add(Index);
		}
	}
	for (const FIntPoint& Index : ToUnload)
	{
		UnloadChunk(Index);
	}

	// Cancel in-flight requests that are no longer needed
	TArray<FIntPoint> StaleInFlight;
	for (const FIntPoint& Index : InFlightChunks)
	{
		if (!DesiredChunks.Contains(Index))
		{
			StaleInFlight.Add(Index);
		}
	}
	for (const FIntPoint& Index : StaleInFlight)
	{
		InFlightChunks.Remove(Index);
	}

	ApplyReadyChunks(DesiredChunks);
	ProcessRebuildQueue();

	// Enable collision and shadows only for chunks near the player
	for (const FIntPoint& Index : ActiveChunks)
	{
		const bool bNearPlayer =
			FMath::Abs(Index.X - PlayerChunk.X) <= 1 &&
			FMath::Abs(Index.Y - PlayerChunk.Y) <= 1;

		if (bNearPlayer)
		{
			if (!WorldMesh->HasCollision(Index))
			{
				WorldMesh->EnableCollision(Index);
			}
			WorldMesh->SetChunkShadows(Index, true);
		}
		else
		{
			if (WorldMesh->HasCollision(Index))
			{
				WorldMesh->DisableCollision(Index);
			}
			WorldMesh->SetChunkShadows(Index, false);
		}
	}

	// Build pending set to avoid re-requesting chunks already waiting
	TSet<FIntPoint> PendingApplySet;
	for (const FAsyncChunkResult& R : PendingApply)
	{
		PendingApplySet.Add(R.ChunkIndex);
	}

	PendingLoadQueue.Reset();
	for (const FIntPoint& Index : DesiredChunks)
	{
		if (!ActiveChunks.Contains(Index) && !InFlightChunks.Contains(Index) && !PendingApplySet.Contains(Index))
		{
			PendingLoadQueue.Add(Index);
		}
	}

	PendingLoadQueue.Sort([&PlayerChunk](const FIntPoint& A, const FIntPoint& B)
	{
		const int32 DistA = FMath::Abs(A.X - PlayerChunk.X) + FMath::Abs(A.Y - PlayerChunk.Y);
		const int32 DistB = FMath::Abs(B.X - PlayerChunk.X) + FMath::Abs(B.Y - PlayerChunk.Y);
		return DistA < DistB;
	});

	while (InFlightChunks.Num() < MaxInFlight && PendingLoadQueue.Num() > 0)
	{
		const FIntPoint Index = PendingLoadQueue[0];
		PendingLoadQueue.RemoveAt(0);
		RequestChunkAsync(Index);
	}
}

TStatId UChunkWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UChunkWorldSubsystem, STATGROUP_Tickables);
}

FIntPoint UChunkWorldSubsystem::WorldToChunkIndex(const FVector& WorldPos) const
{
	return FIntPoint(
		FMath::FloorToInt(WorldPos.X / (ChunkSizeX * AWorldMeshActor::BlockSize)),
		FMath::FloorToInt(WorldPos.Y / (ChunkSizeY * AWorldMeshActor::BlockSize)));
}

void UChunkWorldSubsystem::RequestChunkAsync(const FIntPoint& Index)
{
	InFlightChunks.Add(Index);

	FWorldGenParams Params = GenParams;
	const bool bCached = ChunkCache.Contains(Index);

	FChunkData CachedCopy;
	if (bCached)
	{
		CachedCopy = ChunkCache[Index];
	}

	// Copy modifications for this chunk to apply after generation (thread-safe copy)
	TMap<FIntVector, EBlockType> ChunkMods;
	if (const auto* Mods = ModifiedBlocks.Find(Index))
	{
		ChunkMods = *Mods;
	}

	TSharedPtr<TQueue<FAsyncChunkResult>> Queue = ReadyQueue;

	Async(EAsyncExecution::TaskGraph,
		[Queue, Index, Params, bCached, CachedCopy = MoveTemp(CachedCopy),
		 ChunkMods = MoveTemp(ChunkMods)]() mutable
	{
		FAsyncChunkResult Result;
		Result.ChunkIndex = Index;
		Result.Data = bCached ? MoveTemp(CachedCopy) : FWorldGenerator::GenerateChunk(Params, Index);

		// Apply player modifications on top of generated terrain
		for (const auto& [Pos, Type] : ChunkMods)
		{
			Result.Data.SetBlock(Pos.X, Pos.Y, Pos.Z, Type);
		}

		Result.Mesh = AWorldMeshActor::PrepareChunkMesh(Result.Data);
		Queue->Enqueue(MoveTemp(Result));
	});
}

void UChunkWorldSubsystem::ApplyReadyChunks(const TSet<FIntPoint>& DesiredChunks)
{
	FAsyncChunkResult Result;
	while (ReadyQueue->Dequeue(Result))
	{
		InFlightChunks.Remove(Result.ChunkIndex);

		if (!ChunkCache.Contains(Result.ChunkIndex))
		{
			ChunkCache.Add(Result.ChunkIndex, MoveTemp(Result.Data));
		}

		if (!DesiredChunks.Contains(Result.ChunkIndex) || ActiveChunks.Contains(Result.ChunkIndex))
		{
			continue;
		}

		PendingApply.Add(MoveTemp(Result));
	}

	PendingApply.RemoveAll([&](const FAsyncChunkResult& R)
	{
		return !DesiredChunks.Contains(R.ChunkIndex) || ActiveChunks.Contains(R.ChunkIndex);
	});

	int32 Applied = 0;
	while (Applied < ChunksPerFrame && PendingApply.Num() > 0)
	{
		WorldMesh->ApplyChunk(PendingApply[0].ChunkIndex, PendingApply[0].Mesh);
		ActiveChunks.Add(PendingApply[0].ChunkIndex);
		PendingApply.RemoveAt(0);
		++Applied;
	}
}

void UChunkWorldSubsystem::UnloadChunk(const FIntPoint& Index)
{
	WorldMesh->RemoveChunk(Index);
	ActiveChunks.Remove(Index);
}

void UChunkWorldSubsystem::GlobalToLocal(int32 GlobalX, int32 GlobalY, int32 GlobalZ,
	FIntPoint& OutChunkIndex, FIntVector& OutLocalBlock) const
{
	OutChunkIndex.X = FMath::FloorToInt(static_cast<float>(GlobalX) / ChunkSizeX);
	OutChunkIndex.Y = FMath::FloorToInt(static_cast<float>(GlobalY) / ChunkSizeY);
	OutLocalBlock.X = GlobalX - OutChunkIndex.X * ChunkSizeX;
	OutLocalBlock.Y = GlobalY - OutChunkIndex.Y * ChunkSizeY;
	OutLocalBlock.Z = GlobalZ;
}

bool UChunkWorldSubsystem::FindSolidBlock(const FVector& HitLocation,
	FIntPoint& OutChunkIndex, FIntVector& OutLocalBlock) const
{
	const float S = AWorldMeshActor::BlockSize;
	const int32 CenterX = FMath::FloorToInt(HitLocation.X / S);
	const int32 CenterY = FMath::FloorToInt(HitLocation.Y / S);
	const int32 CenterZ = FMath::FloorToInt(HitLocation.Z / S);

	static const FIntVector Offsets[] = {
		{0, 0, 0}, {0, 0, -1}, {0, 0, 1},
		{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}
	};

	float BestDist = TNumericLimits<float>::Max();
	bool bFound = false;

	for (const FIntVector& Off : Offsets)
	{
		const int32 GX = CenterX + Off.X;
		const int32 GY = CenterY + Off.Y;
		const int32 GZ = CenterZ + Off.Z;

		if (GZ < 0 || GZ >= ChunkSizeZ)
		{
			continue;
		}

		FIntPoint CI;
		FIntVector LB;
		GlobalToLocal(GX, GY, GZ, CI, LB);

		const FChunkData* Data = ChunkCache.Find(CI);
		if (!Data || Data->GetBlock(LB.X, LB.Y, LB.Z) == EBlockType::Air)
		{
			continue;
		}

		const FVector BlockCenter((GX + 0.5f) * S, (GY + 0.5f) * S, (GZ + 0.5f) * S);
		const float Dist = FVector::DistSquared(HitLocation, BlockCenter);

		if (Dist < BestDist)
		{
			BestDist = Dist;
			OutChunkIndex = CI;
			OutLocalBlock = LB;
			bFound = true;
		}
	}

	return bFound;
}

bool UChunkWorldSubsystem::FindAdjacentEmpty(const FVector& HitLocation,
	FIntPoint& OutChunkIndex, FIntVector& OutLocalBlock) const
{
	FIntPoint SolidChunk;
	FIntVector SolidBlock;
	if (!FindSolidBlock(HitLocation, SolidChunk, SolidBlock))
	{
		return false;
	}

	const float S = AWorldMeshActor::BlockSize;
	const int32 SolidGX = SolidChunk.X * ChunkSizeX + SolidBlock.X;
	const int32 SolidGY = SolidChunk.Y * ChunkSizeY + SolidBlock.Y;
	const int32 SolidGZ = SolidBlock.Z;

	const FVector SolidCenter(
		(SolidGX + 0.5f) * S,
		(SolidGY + 0.5f) * S,
		(SolidGZ + 0.5f) * S);

	// Hit face is the axis with the largest offset from block center
	const FVector Offset = HitLocation - SolidCenter;
	const float AbsX = FMath::Abs(Offset.X);
	const float AbsY = FMath::Abs(Offset.Y);
	const float AbsZ = FMath::Abs(Offset.Z);

	int32 PlaceGX = SolidGX;
	int32 PlaceGY = SolidGY;
	int32 PlaceGZ = SolidGZ;

	if (AbsX >= AbsY && AbsX >= AbsZ)
	{
		PlaceGX += (Offset.X > 0) ? 1 : -1;
	}
	else if (AbsY >= AbsX && AbsY >= AbsZ)
	{
		PlaceGY += (Offset.Y > 0) ? 1 : -1;
	}
	else
	{
		PlaceGZ += (Offset.Z > 0) ? 1 : -1;
	}

	if (PlaceGZ < 0 || PlaceGZ >= ChunkSizeZ)
	{
		return false;
	}

	GlobalToLocal(PlaceGX, PlaceGY, PlaceGZ, OutChunkIndex, OutLocalBlock);

	const FChunkData* Data = ChunkCache.Find(OutChunkIndex);
	if (!Data || Data->GetBlock(OutLocalBlock.X, OutLocalBlock.Y, OutLocalBlock.Z) != EBlockType::Air)
	{
		return false;
	}

	return true;
}

EBlockType UChunkWorldSubsystem::GetBlock(const FVector& HitLocation) const
{
	FIntPoint ChunkIndex;
	FIntVector LocalBlock;

	if (!FindSolidBlock(HitLocation, ChunkIndex, LocalBlock))
	{
		return EBlockType::Air;
	}

	return GetBlockAt(ChunkIndex, LocalBlock);
}

EBlockType UChunkWorldSubsystem::GetBlockAt(const FIntPoint& ChunkIndex, const FIntVector& LocalBlock) const
{
	const FChunkData* Data = ChunkCache.Find(ChunkIndex);
	if (!Data)
	{
		return EBlockType::Air;
	}

	return Data->GetBlock(LocalBlock.X, LocalBlock.Y, LocalBlock.Z);
}

float UChunkWorldSubsystem::GetBlockMiningTime(EBlockType Type) const
{
	if (Settings)
	{
		if (const float* Time = Settings->BlockMiningTime.Find(Type))
		{
			return *Time;
		}
	}
	return 1.0f;
}

bool UChunkWorldSubsystem::RemoveBlock(const FVector& HitLocation)
{
	FIntPoint ChunkIndex;
	FIntVector LocalBlock;

	if (!FindSolidBlock(HitLocation, ChunkIndex, LocalBlock))
	{
		return false;
	}

	// Bedrock: cannot destroy blocks at Z=0
	if (LocalBlock.Z == 0)
	{
		return false;
	}

	FChunkData* Data = ChunkCache.Find(ChunkIndex);
	if (!Data)
	{
		return false;
	}

	Data->SetBlock(LocalBlock.X, LocalBlock.Y, LocalBlock.Z, EBlockType::Air);
	ModifiedBlocks.FindOrAdd(ChunkIndex).Add(FIntVector(LocalBlock.X, LocalBlock.Y, LocalBlock.Z), EBlockType::Air);
	QueueRebuild(ChunkIndex);

	return true;
}

bool UChunkWorldSubsystem::PlaceBlock(const FVector& HitLocation, EBlockType Type)
{
	if (Type == EBlockType::Air)
	{
		return false;
	}

	FIntPoint ChunkIndex;
	FIntVector LocalBlock;

	if (!FindAdjacentEmpty(HitLocation, ChunkIndex, LocalBlock))
	{
		return false;
	}

	FChunkData* Data = ChunkCache.Find(ChunkIndex);
	if (!Data)
	{
		return false;
	}

	Data->SetBlock(LocalBlock.X, LocalBlock.Y, LocalBlock.Z, Type);
	ModifiedBlocks.FindOrAdd(ChunkIndex).Add(FIntVector(LocalBlock.X, LocalBlock.Y, LocalBlock.Z), Type);
	QueueRebuild(ChunkIndex);

	return true;
}

bool UChunkWorldSubsystem::GetBlockCoords(const FVector& HitLocation,
	FIntPoint& OutChunkIndex, FIntVector& OutLocalBlock) const
{
	return FindSolidBlock(HitLocation, OutChunkIndex, OutLocalBlock);
}

FVector UChunkWorldSubsystem::BlockWorldCenter(const FIntPoint& ChunkIndex, const FIntVector& LocalBlock) const
{
	const float S = AWorldMeshActor::BlockSize;
	return FVector(
		(ChunkIndex.X * ChunkSizeX + LocalBlock.X + 0.5f) * S,
		(ChunkIndex.Y * ChunkSizeY + LocalBlock.Y + 0.5f) * S,
		(LocalBlock.Z + 0.5f) * S);
}

bool UChunkWorldSubsystem::GetPlaceBlockWorldCenter(const FVector& HitLocation, FVector& OutCenter) const
{
	FIntPoint ChunkIndex;
	FIntVector LocalBlock;

	if (!FindAdjacentEmpty(HitLocation, ChunkIndex, LocalBlock))
	{
		return false;
	}

	OutCenter = BlockWorldCenter(ChunkIndex, LocalBlock);
	return true;
}

void UChunkWorldSubsystem::SetMiningProgress(const FVector& BlockCenter, float Progress)
{
	if (!Settings || !Settings->MiningMPC)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UMaterialParameterCollectionInstance* MPC = World->GetParameterCollectionInstance(Settings->MiningMPC);
	if (!MPC)
	{
		return;
	}

	const float HalfBlock = AWorldMeshActor::BlockSize * 0.5f;
	const float Expand = 0.5f;
	MPC->SetVectorParameterValue(FName("MiningMin"), FLinearColor(
		BlockCenter.X - HalfBlock - Expand, BlockCenter.Y - HalfBlock - Expand, BlockCenter.Z - HalfBlock - Expand));
	MPC->SetVectorParameterValue(FName("MiningMax"), FLinearColor(
		BlockCenter.X + HalfBlock + Expand, BlockCenter.Y + HalfBlock + Expand, BlockCenter.Z + HalfBlock + Expand));
	MPC->SetScalarParameterValue(FName("MiningProgress"), FMath::Clamp(Progress, 0.f, 1.f));
}

void UChunkWorldSubsystem::ClearMiningProgress()
{
	SetMiningProgress(FVector::ZeroVector, 0.f);
}

void UChunkWorldSubsystem::QueueRebuild(const FIntPoint& ChunkIndex)
{
	if (!RebuildSet.Contains(ChunkIndex))
	{
		RebuildSet.Add(ChunkIndex);
		RebuildQueue.Add(ChunkIndex);
	}
}

void UChunkWorldSubsystem::ProcessRebuildQueue()
{
	if (RebuildQueue.Num() == 0 || !WorldMesh)
	{
		return;
	}

	const FIntPoint ChunkIndex = RebuildQueue.Last();
	RebuildQueue.Pop();
	RebuildSet.Remove(ChunkIndex);

	if (!ActiveChunks.Contains(ChunkIndex))
	{
		return;
	}

	const FChunkData* Data = ChunkCache.Find(ChunkIndex);
	if (!Data)
	{
		return;
	}

	FPreparedChunkMesh Mesh = AWorldMeshActor::PrepareChunkMesh(*Data);
	WorldMesh->UpdateChunk(ChunkIndex, Mesh);

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (PlayerPawn)
	{
		const FIntPoint PlayerChunk = WorldToChunkIndex(PlayerPawn->GetActorLocation());
		const bool bNearPlayer =
			FMath::Abs(ChunkIndex.X - PlayerChunk.X) <= 1 &&
			FMath::Abs(ChunkIndex.Y - PlayerChunk.Y) <= 1;

		if (bNearPlayer)
		{
			WorldMesh->EnableCollision(ChunkIndex);
		}
	}
}

void UChunkWorldSubsystem::SetSeed(int32 InSeed)
{
	GenParams.Seed = InSeed;
}

void UChunkWorldSubsystem::ResetWorld()
{
	// Remove all active chunk meshes
	if (WorldMesh)
	{
		for (const FIntPoint& Index : ActiveChunks)
		{
			WorldMesh->RemoveChunk(Index);
		}
	}

	ActiveChunks.Empty();
	ChunkCache.Empty();
	InFlightChunks.Empty();
	PendingApply.Empty();
	PendingLoadQueue.Empty();
	RebuildQueue.Empty();
	RebuildSet.Empty();
	ModifiedBlocks.Empty();

	// Drain async queue
	FAsyncChunkResult Result;
	while (ReadyQueue->Dequeue(Result)) {}
}

void UChunkWorldSubsystem::SetModifiedBlocks(const TMap<FIntPoint, TMap<FIntVector, EBlockType>>& InModified)
{
	ModifiedBlocks = InModified;
}

void UChunkWorldSubsystem::ForceLoadChunksAt(const FVector& WorldPos)
{
	EnsureWorldMesh();
	if (!WorldMesh)
	{
		return;
	}

	const FIntPoint Center = WorldToChunkIndex(WorldPos);

	// Generate player chunk and immediate neighbors (3x3) synchronously
	for (int32 DX = -1; DX <= 1; ++DX)
	{
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			const FIntPoint Index(Center.X + DX, Center.Y + DY);

			if (ActiveChunks.Contains(Index))
			{
				continue;
			}

			// Generate chunk data
			if (!ChunkCache.Contains(Index))
			{
				FChunkData Data = FWorldGenerator::GenerateChunk(GenParams, Index);

				// Apply modifications
				if (const auto* Mods = ModifiedBlocks.Find(Index))
				{
					for (const auto& [Pos, Type] : *Mods)
					{
						Data.SetBlock(Pos.X, Pos.Y, Pos.Z, Type);
					}
				}

				ChunkCache.Add(Index, MoveTemp(Data));
			}

			// Build and apply mesh
			const FChunkData* Data = ChunkCache.Find(Index);
			if (Data)
			{
				FPreparedChunkMesh Mesh = AWorldMeshActor::PrepareChunkMesh(*Data);
				WorldMesh->ApplyChunk(Index, Mesh);
				WorldMesh->EnableCollision(Index);
				ActiveChunks.Add(Index);
			}
		}
	}
}
