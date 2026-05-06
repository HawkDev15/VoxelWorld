#pragma once

#include "CoreMinimal.h"
#include "BlockType.h"
#include "InventoryTypes.generated.h"

USTRUCT(BlueprintType)
struct FInventorySlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EBlockType Type = EBlockType::Air;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Count = 0;
};
