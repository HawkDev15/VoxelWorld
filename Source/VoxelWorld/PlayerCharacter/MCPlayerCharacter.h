#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VoxelWorld/BlockType.h"
#include "VoxelWorld/InventoryTypes.h"
#include "MCPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;

UCLASS()
class VOXELWORLD_API AMCPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMCPlayerCharacter();

	virtual void Tick(float DeltaTime) override;

	void OnUseTriggered();
	void OnUseCompleted();

	// Select hotbar slot (0-3)
	void SelectSlot(int32 SlotIndex);

	// Place block from currently selected slot
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void PlaceSelectedBlock();

	// Inventory (4 hotbar slots)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<FInventorySlot> Inventory;

	// Currently selected hotbar slot index
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 SelectedSlot = 0;

private:
	void UpdateTrace();
	void StartMining();
	void ClearMine();
	void CompleteMining();

	// Trace result, updated once per frame in Tick
	bool bTraceHit = false;
	FVector TraceHitLocation = FVector::ZeroVector;

	// Block under crosshair (from trace)
	bool bHasAimedBlock = false;
	FIntPoint AimedChunkIndex;
	FIntVector AimedBlockCoords = FIntVector::ZeroValue;
	FVector AimedBlockCenter = FVector::ZeroVector;
	EBlockType AimedBlockType = EBlockType::Air;

	// Place preview
	bool bHasPlaceBlock = false;
	FVector PlaceBlockCenter = FVector::ZeroVector;

	// Mining state
	FTimerHandle MiningTimerHandle;
	bool bIsMining = false;
	bool bHasTarget = false;
	FIntVector TargetBlockCoords = FIntVector::ZeroValue;
	EBlockType TargetBlockType = EBlockType::Air;
	FVector MiningHitLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> Camera;
};
