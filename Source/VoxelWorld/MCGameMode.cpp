#include "MCGameMode.h"
#include "MCSaveGame.h"
#include "ChunkWorldSubsystem.h"
#include "PlayerCharacter/MCPlayerCharacter.h"
#include "PlayerCharacter/MCPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

static const FString SavePrefix = TEXT("MC_");

AMCGameMode::AMCGameMode()
{
	DefaultPawnClass = AMCPlayerCharacter::StaticClass();
	PlayerControllerClass = AMCPlayerController::StaticClass();
}

void AMCGameMode::NewWorld()
{
	UChunkWorldSubsystem* Sub = GetWorld()->GetSubsystem<UChunkWorldSubsystem>();
	if (Sub)
	{
		Sub->ResetWorld();
		Sub->SetSeed(FMath::Rand());

		const FVector SpawnPos(0.f, 0.f, Sub->GetChunkSizeZ() * AWorldMeshActor::BlockSize);
		Sub->ForceLoadChunksAt(SpawnPos);

		AMCPlayerCharacter* Player = Cast<AMCPlayerCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
		if (Player)
		{
			Player->SetActorLocation(SpawnPos);

			// Reset inventory to defaults
			Player->Inventory.SetNum(4);
			Player->Inventory[0] = { EBlockType::Stone, 32 };
			Player->Inventory[1] = { EBlockType::Dirt,  32 };
			Player->Inventory[2] = { EBlockType::Grass, 32 };
			Player->Inventory[3] = { EBlockType::Snow,  32 };
			Player->SelectedSlot = 0;
		}
	}
}

void AMCGameMode::SaveWorld(const FString& SlotName)
{
	UMCSaveGame* Save = NewObject<UMCSaveGame>();

	UChunkWorldSubsystem* Sub = GetWorld()->GetSubsystem<UChunkWorldSubsystem>();
	if (Sub)
	{
		Save->Seed = Sub->GetSeed();

		for (const auto& [ChunkIndex, BlockMap] : Sub->GetModifiedBlocks())
		{
			FChunkModifications ChunkMod;
			ChunkMod.ChunkIndex = ChunkIndex;

			for (const auto& [LocalPos, Type] : BlockMap)
			{
				FBlockModification BlockMod;
				BlockMod.LocalBlock = LocalPos;
				BlockMod.Type = Type;
				ChunkMod.Blocks.Add(BlockMod);
			}

			Save->ModifiedChunks.Add(ChunkMod);
		}
	}

	AMCPlayerCharacter* Player = Cast<AMCPlayerCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (Player)
	{
		Save->PlayerPosition = Player->GetActorLocation();
		Save->PlayerRotation = Player->GetActorRotation();
		Save->Inventory = Player->Inventory;
		Save->SelectedSlot = Player->SelectedSlot;
	}

	UGameplayStatics::SaveGameToSlot(Save, SavePrefix + SlotName, 0);
}

void AMCGameMode::LoadWorld(const FString& SlotName)
{
	const FString FullSlot = SavePrefix + SlotName;

	if (!UGameplayStatics::DoesSaveGameExist(FullSlot, 0))
	{
		return;
	}

	UMCSaveGame* Save = Cast<UMCSaveGame>(UGameplayStatics::LoadGameFromSlot(FullSlot, 0));
	if (!Save)
	{
		return;
	}

	UChunkWorldSubsystem* Sub = GetWorld()->GetSubsystem<UChunkWorldSubsystem>();
	if (Sub)
	{
		Sub->ResetWorld();
		Sub->SetSeed(Save->Seed);

		TMap<FIntPoint, TMap<FIntVector, EBlockType>> Mods;
		for (const FChunkModifications& ChunkMod : Save->ModifiedChunks)
		{
			TMap<FIntVector, EBlockType>& BlockMap = Mods.FindOrAdd(ChunkMod.ChunkIndex);
			for (const FBlockModification& BlockMod : ChunkMod.Blocks)
			{
				BlockMap.Add(BlockMod.LocalBlock, BlockMod.Type);
			}
		}

		Sub->SetModifiedBlocks(Mods);
	}

	// Force-generate chunks at saved position so player doesn't fall through
	if (Sub)
	{
		Sub->ForceLoadChunksAt(Save->PlayerPosition);
	}

	// Restore player state immediately — pawn exists by StartPlay
	AMCPlayerCharacter* Player = Cast<AMCPlayerCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (Player)
	{
		Player->SetActorLocation(Save->PlayerPosition);
		if (Player->GetController())
		{
			Player->GetController()->SetControlRotation(Save->PlayerRotation);
		}
		Player->Inventory = Save->Inventory;
		Player->SelectedSlot = Save->SelectedSlot;
	}
}

void AMCGameMode::DeleteSave(const FString& SlotName)
{
	UGameplayStatics::DeleteGameInSlot(SavePrefix + SlotName, 0);
}

TArray<FString> AMCGameMode::GetSaveSlots() const
{
	TArray<FString> Slots;

	const FString SaveDir = FPaths::ProjectSavedDir() / TEXT("SaveGames");
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *SaveDir, TEXT(".sav"));

	for (const FString& File : Files)
	{
		const FString Name = FPaths::GetBaseFilename(File);
		if (Name.StartsWith(SavePrefix))
		{
			Slots.Add(Name.RightChop(SavePrefix.Len()));
		}
	}

	return Slots;
}

bool AMCGameMode::DoesSaveExist(const FString& SlotName) const
{
	return UGameplayStatics::DoesSaveGameExist(SavePrefix + SlotName, 0);
}
