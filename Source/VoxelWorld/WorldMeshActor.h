#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChunkData.h"
#include "WorldMeshActor.generated.h"

class UProceduralMeshComponent;

// Mesh data for a single section (one block type)
struct FChunkMeshSection
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
};

// Pre-built chunk mesh, safe to prepare on any thread
struct FPreparedChunkMesh
{
	FIntPoint ChunkIndex;
	int32 SizeX = FChunkData::DefaultSizeX;
	int32 SizeY = FChunkData::DefaultSizeY;
	TMap<EBlockType, FChunkMeshSection> Sections;
	FChunkMeshSection CollisionSection;
};

// Runtime state for a loaded chunk
struct FActiveChunk
{
	UProceduralMeshComponent* MeshComp = nullptr;
	FChunkMeshSection PendingCollision;
	int32 CollisionSectionIndex = -1;
	bool bHasCollision = false;
	bool bCastsShadow = false;
};

// Single actor that owns all chunk mesh components
UCLASS()
class VOXELWORLD_API AWorldMeshActor : public AActor
{
	GENERATED_BODY()

public:
	AWorldMeshActor();

	static constexpr float BlockSize = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blocks")
	TMap<EBlockType, UMaterialInterface*> BlockMaterials;

	static FPreparedChunkMesh PrepareChunkMesh(const FChunkData& Data);

	void ApplyChunk(const FIntPoint& Index, const FPreparedChunkMesh& Prepared);
	void UpdateChunk(const FIntPoint& Index, const FPreparedChunkMesh& Prepared);
	void RemoveChunk(const FIntPoint& Index);
	void EnableCollision(const FIntPoint& Index);
	void DisableCollision(const FIntPoint& Index);
	bool HasCollision(const FIntPoint& Index) const;

	void SetChunkShadows(const FIntPoint& Index, bool bCastShadow);

	static void AddFace(FChunkMeshSection& Section, const FVector& Origin,
		const FVector& Right, const FVector& Up, const FVector& Normal);

private:
	TMap<FIntPoint, FActiveChunk> Chunks;

	UPROPERTY()
	TArray<UProceduralMeshComponent*> ComponentPool;

	UProceduralMeshComponent* AcquireComponent();
	void ReleaseComponent(UProceduralMeshComponent* Comp);
	void BuildMeshSections(UProceduralMeshComponent* Comp, const FPreparedChunkMesh& Prepared, FActiveChunk& OutChunk);
};
