#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MCGameMode.generated.h"

class UMCSaveGame;

UCLASS()
class VOXELWORLD_API AMCGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMCGameMode();

	// Start a new world with a fresh random seed, reset player to default state
	UFUNCTION(BlueprintCallable, Category = "Save")
	void NewWorld();

	// Save current world and player state to a named slot
	UFUNCTION(BlueprintCallable, Category = "Save")
	void SaveWorld(const FString& SlotName);

	// Load world and player state from a named slot
	UFUNCTION(BlueprintCallable, Category = "Save")
	void LoadWorld(const FString& SlotName);

	// Delete a save slot
	UFUNCTION(BlueprintCallable, Category = "Save")
	void DeleteSave(const FString& SlotName);

	// Returns list of all existing save slot names
	UFUNCTION(BlueprintCallable, Category = "Save")
	TArray<FString> GetSaveSlots() const;

	// Check if a save slot exists
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Save")
	bool DoesSaveExist(const FString& SlotName) const;
};
