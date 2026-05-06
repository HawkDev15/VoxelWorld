#include "WorldMeshActor.h"
#include "ProceduralMeshComponent.h"

AWorldMeshActor::AWorldMeshActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWorldMeshActor::AddFace(FChunkMeshSection& Section, const FVector& Origin,
	const FVector& Right, const FVector& Up, const FVector& Normal)
{
	const int32 BaseIdx = Section.Vertices.Num();

	Section.Vertices.Add(Origin);
	Section.Vertices.Add(Origin + Right);
	Section.Vertices.Add(Origin + Right + Up);
	Section.Vertices.Add(Origin + Up);

	Section.Triangles.Add(BaseIdx);
	Section.Triangles.Add(BaseIdx + 2);
	Section.Triangles.Add(BaseIdx + 1);
	Section.Triangles.Add(BaseIdx);
	Section.Triangles.Add(BaseIdx + 3);
	Section.Triangles.Add(BaseIdx + 2);

	for (int32 i = 0; i < 4; ++i)
	{
		Section.Normals.Add(Normal);
	}

	// UV scales with quad size so textures tile correctly
	const float USize = Right.Size() / BlockSize;
	const float VSize = Up.Size() / BlockSize;
	Section.UVs.Add(FVector2D(0.f, 0.f));
	Section.UVs.Add(FVector2D(USize, 0.f));
	Section.UVs.Add(FVector2D(USize, VSize));
	Section.UVs.Add(FVector2D(0.f, VSize));
}

// Face info for greedy meshing algorithm
struct FGreedyFace
{
	EBlockType Type = EBlockType::Air;
	bool bVisible = false;
};

// Merges adjacent same-type faces in a 2D mask into larger quads
static void GreedyMeshSlice(
	TArray<FGreedyFace>& Mask, int32 Width, int32 Height,
	TMap<EBlockType, FChunkMeshSection>& Sections,
	FChunkMeshSection& CollisionSection,
	bool bAddCollision,
	const FVector& SliceOrigin, const FVector& AxisU, const FVector& AxisV,
	const FVector& Normal, float BlockSize)
{
	for (int32 J = 0; J < Height; ++J)
	{
		for (int32 I = 0; I < Width; ++I)
		{
			const FGreedyFace& Face = Mask[I + J * Width];
			if (!Face.bVisible)
			{
				continue;
			}

			const EBlockType Type = Face.Type;

			// Expand along I
			int32 W = 1;
			while (I + W < Width)
			{
				const FGreedyFace& Next = Mask[(I + W) + J * Width];
				if (!Next.bVisible || Next.Type != Type)
				{
					break;
				}
				++W;
			}

			// Expand along J
			int32 H = 1;
			bool bDone = false;
			while (J + H < Height && !bDone)
			{
				for (int32 K = 0; K < W; ++K)
				{
					const FGreedyFace& Next = Mask[(I + K) + (J + H) * Width];
					if (!Next.bVisible || Next.Type != Type)
					{
						bDone = true;
						break;
					}
				}
				if (!bDone)
				{
					++H;
				}
			}

			const FVector Origin = SliceOrigin + AxisU * (I * BlockSize) + AxisV * (J * BlockSize);
			const FVector Right = AxisU * (W * BlockSize);
			const FVector Up = AxisV * (H * BlockSize);

			AWorldMeshActor::AddFace(Sections.FindOrAdd(Type), Origin, Right, Up, Normal);

			if (bAddCollision)
			{
				AWorldMeshActor::AddFace(CollisionSection, Origin, Right, Up, Normal);
			}

			// Mark merged faces as processed
			for (int32 DJ = 0; DJ < H; ++DJ)
			{
				for (int32 DI = 0; DI < W; ++DI)
				{
					Mask[(I + DI) + (J + DJ) * Width].bVisible = false;
				}
			}
		}
	}
}

// Greedy mesh: mask I index aligns with AxisU, J with AxisV
FPreparedChunkMesh AWorldMeshActor::PrepareChunkMesh(const FChunkData& Data)
{
	FPreparedChunkMesh Result;
	Result.ChunkIndex = Data.ChunkIndex;
	Result.SizeX = Data.SizeX;
	Result.SizeY = Data.SizeY;

	const float S = BlockSize;
	const int32 SX = Data.SizeX;
	const int32 SY = Data.SizeY;
	const int32 SZ = Data.SizeZ;

	// +Z: AxisU=+X, AxisV=+Y, mask I=X J=Y
	{
		TArray<FGreedyFace> Mask;
		Mask.SetNum(SX * SY);
		for (int32 Z = 0; Z < SZ; ++Z)
		{
			for (int32 Y = 0; Y < SY; ++Y)
				for (int32 X = 0; X < SX; ++X)
				{
					FGreedyFace& F = Mask[X + Y * SX];
					const EBlockType Type = Data.GetBlock(X, Y, Z);
					F.bVisible = (Type != EBlockType::Air && !Data.IsOpaque(X, Y, Z + 1));
					F.Type = Type;
				}
			GreedyMeshSlice(Mask, SX, SY, Result.Sections, Result.CollisionSection, true,
				FVector(0, 0, (Z + 1) * S), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), S);
		}
	}

	// -Z: AxisU=+Y, AxisV=+X, mask I=Y J=X
	{
		TArray<FGreedyFace> Mask;
		Mask.SetNum(SY * SX);
		for (int32 Z = 0; Z < SZ; ++Z)
		{
			for (int32 X = 0; X < SX; ++X)
				for (int32 Y = 0; Y < SY; ++Y)
				{
					FGreedyFace& F = Mask[Y + X * SY];
					const EBlockType Type = Data.GetBlock(X, Y, Z);
					F.bVisible = (Type != EBlockType::Air && !Data.IsOpaque(X, Y, Z - 1));
					F.Type = Type;
				}
			GreedyMeshSlice(Mask, SY, SX, Result.Sections, Result.CollisionSection, true,
				FVector(0, 0, Z * S), FVector(0, 1, 0), FVector(1, 0, 0), FVector(0, 0, -1), S);
		}
	}

	// +X: AxisU=+Y, AxisV=+Z, mask I=Y J=Z
	{
		TArray<FGreedyFace> Mask;
		Mask.SetNum(SY * SZ);
		for (int32 X = 0; X < SX; ++X)
		{
			for (int32 Z = 0; Z < SZ; ++Z)
				for (int32 Y = 0; Y < SY; ++Y)
				{
					FGreedyFace& F = Mask[Y + Z * SY];
					const EBlockType Type = Data.GetBlock(X, Y, Z);
					F.bVisible = (Type != EBlockType::Air && !Data.IsOpaque(X + 1, Y, Z));
					F.Type = Type;
				}
			GreedyMeshSlice(Mask, SY, SZ, Result.Sections, Result.CollisionSection, true,
				FVector((X + 1) * S, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FVector(1, 0, 0), S);
		}
	}

	// -X: AxisU=+Z, AxisV=+Y, mask I=Z J=Y
	{
		TArray<FGreedyFace> Mask;
		Mask.SetNum(SZ * SY);
		for (int32 X = 0; X < SX; ++X)
		{
			for (int32 Y = 0; Y < SY; ++Y)
				for (int32 Z = 0; Z < SZ; ++Z)
				{
					FGreedyFace& F = Mask[Z + Y * SZ];
					const EBlockType Type = Data.GetBlock(X, Y, Z);
					F.bVisible = (Type != EBlockType::Air && !Data.IsOpaque(X - 1, Y, Z));
					F.Type = Type;
				}
			GreedyMeshSlice(Mask, SZ, SY, Result.Sections, Result.CollisionSection, true,
				FVector(X * S, 0, 0), FVector(0, 0, 1), FVector(0, 1, 0), FVector(-1, 0, 0), S);
		}
	}

	// +Y: AxisU=+Z, AxisV=+X, mask I=Z J=X
	{
		TArray<FGreedyFace> Mask;
		Mask.SetNum(SZ * SX);
		for (int32 Y = 0; Y < SY; ++Y)
		{
			for (int32 X = 0; X < SX; ++X)
				for (int32 Z = 0; Z < SZ; ++Z)
				{
					FGreedyFace& F = Mask[Z + X * SZ];
					const EBlockType Type = Data.GetBlock(X, Y, Z);
					F.bVisible = (Type != EBlockType::Air && !Data.IsOpaque(X, Y + 1, Z));
					F.Type = Type;
				}
			GreedyMeshSlice(Mask, SZ, SX, Result.Sections, Result.CollisionSection, true,
				FVector(0, (Y + 1) * S, 0), FVector(0, 0, 1), FVector(1, 0, 0), FVector(0, 1, 0), S);
		}
	}

	// -Y: AxisU=+X, AxisV=+Z, mask I=X J=Z
	{
		TArray<FGreedyFace> Mask;
		Mask.SetNum(SX * SZ);
		for (int32 Y = 0; Y < SY; ++Y)
		{
			for (int32 Z = 0; Z < SZ; ++Z)
				for (int32 X = 0; X < SX; ++X)
				{
					FGreedyFace& F = Mask[X + Z * SX];
					const EBlockType Type = Data.GetBlock(X, Y, Z);
					F.bVisible = (Type != EBlockType::Air && !Data.IsOpaque(X, Y - 1, Z));
					F.Type = Type;
				}
			GreedyMeshSlice(Mask, SX, SZ, Result.Sections, Result.CollisionSection, true,
				FVector(0, Y * S, 0), FVector(1, 0, 0), FVector(0, 0, 1), FVector(0, -1, 0), S);
		}
	}

	return Result;
}

UProceduralMeshComponent* AWorldMeshActor::AcquireComponent()
{
	if (ComponentPool.Num() > 0)
	{
		UProceduralMeshComponent* Comp = ComponentPool.Pop();
		Comp->SetActive(true);
		Comp->SetVisibility(true);
		Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Comp->RegisterComponent();
		return Comp;
	}

	UProceduralMeshComponent* Comp = NewObject<UProceduralMeshComponent>(this);
	Comp->SetupAttachment(RootComponent);
	Comp->SetCastShadow(false);
	Comp->RegisterComponent();
	return Comp;
}

void AWorldMeshActor::ReleaseComponent(UProceduralMeshComponent* Comp)
{
	if (!Comp)
	{
		return;
	}

	Comp->ClearAllMeshSections();
	Comp->SetVisibility(false);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->UnregisterComponent();
	ComponentPool.Add(Comp);
}

void AWorldMeshActor::BuildMeshSections(UProceduralMeshComponent* Comp,
	const FPreparedChunkMesh& Prepared, FActiveChunk& OutChunk)
{
	Comp->ClearAllMeshSections();

	UMaterialInterface* FallbackMat = nullptr;
	for (const auto& [_, Mat] : BlockMaterials)
	{
		if (Mat) { FallbackMat = Mat; break; }
	}

	int32 SectionIndex = 0;
	for (const auto& [Type, Section] : Prepared.Sections)
	{
		if (Section.Vertices.Num() == 0)
		{
			continue;
		}

		Comp->CreateMeshSection(
			SectionIndex,
			Section.Vertices,
			Section.Triangles,
			Section.Normals,
			Section.UVs,
			TArray<FColor>(),
			TArray<FProcMeshTangent>(),
			false);

		UMaterialInterface** Mat = BlockMaterials.Find(Type);
		Comp->SetMaterial(SectionIndex, (Mat && *Mat) ? *Mat : FallbackMat);

		++SectionIndex;
	}

	OutChunk.MeshComp = Comp;
	OutChunk.CollisionSectionIndex = SectionIndex;
	OutChunk.PendingCollision = Prepared.CollisionSection;
	OutChunk.bHasCollision = false;
	OutChunk.bCastsShadow = false;
}

void AWorldMeshActor::ApplyChunk(const FIntPoint& Index, const FPreparedChunkMesh& Prepared)
{
	RemoveChunk(Index);

	UProceduralMeshComponent* Comp = AcquireComponent();
	if (!Comp)
	{
		return;
	}

	const FVector ChunkWorldOrigin(
		Index.X * Prepared.SizeX * BlockSize,
		Index.Y * Prepared.SizeY * BlockSize,
		0.f);
	Comp->SetRelativeLocation(ChunkWorldOrigin);

	FActiveChunk& Chunk = Chunks.Add(Index);
	BuildMeshSections(Comp, Prepared, Chunk);
}

void AWorldMeshActor::UpdateChunk(const FIntPoint& Index, const FPreparedChunkMesh& Prepared)
{
	FActiveChunk* Chunk = Chunks.Find(Index);
	if (!Chunk || !Chunk->MeshComp)
	{
		return;
	}

	BuildMeshSections(Chunk->MeshComp, Prepared, *Chunk);
}

void AWorldMeshActor::SetChunkShadows(const FIntPoint& Index, bool bCastShadow)
{
	FActiveChunk* Chunk = Chunks.Find(Index);
	if (Chunk && Chunk->MeshComp && Chunk->bCastsShadow != bCastShadow)
	{
		Chunk->bCastsShadow = bCastShadow;
		Chunk->MeshComp->SetCastShadow(bCastShadow);
	}
}

void AWorldMeshActor::RemoveChunk(const FIntPoint& Index)
{
	if (FActiveChunk* Found = Chunks.Find(Index))
	{
		ReleaseComponent(Found->MeshComp);
		Chunks.Remove(Index);
	}
}

void AWorldMeshActor::EnableCollision(const FIntPoint& Index)
{
	FActiveChunk* Chunk = Chunks.Find(Index);
	if (!Chunk || Chunk->bHasCollision || Chunk->PendingCollision.Vertices.Num() == 0)
	{
		return;
	}

	Chunk->MeshComp->CreateMeshSection(
		Chunk->CollisionSectionIndex,
		Chunk->PendingCollision.Vertices,
		Chunk->PendingCollision.Triangles,
		Chunk->PendingCollision.Normals,
		Chunk->PendingCollision.UVs,
		TArray<FColor>(),
		TArray<FProcMeshTangent>(),
		true);

	Chunk->MeshComp->SetMeshSectionVisible(Chunk->CollisionSectionIndex, false);
	Chunk->bHasCollision = true;
}

void AWorldMeshActor::DisableCollision(const FIntPoint& Index)
{
	FActiveChunk* Chunk = Chunks.Find(Index);
	if (!Chunk || !Chunk->bHasCollision)
	{
		return;
	}

	Chunk->MeshComp->ClearMeshSection(Chunk->CollisionSectionIndex);
	Chunk->bHasCollision = false;
}

bool AWorldMeshActor::HasCollision(const FIntPoint& Index) const
{
	const FActiveChunk* Chunk = Chunks.Find(Index);
	return Chunk && Chunk->bHasCollision;
}
