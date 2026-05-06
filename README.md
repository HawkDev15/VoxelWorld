# VoxelWorld

A Minecraft-inspired voxel sandbox built on Unreal Engine 5.7. Infinite procedural world driven by multi-octave Perlin noise, block breaking and placing, world save/load, and a hotbar inventory.

## Demo

[![VoxelWorld demo](https://img.youtube.com/vi/J5yOp9EUoHg/maxresdefault.jpg)](https://www.youtube.com/watch?v=J5yOp9EUoHg)

[Watch on YouTube](https://www.youtube.com/watch?v=J5yOp9EUoHg)

## Stack

- **Unreal Engine** 5.7
- **Language**: C++ (Runtime module `VoxelWorld`) + Blueprints for UI/configuration
- **Plugins**: `ProceduralMeshComponent`, `ModelingToolsEditorMode` (editor only), `EnhancedInput`, `UMG`

## Features

- Infinite procedural world streamed in chunks (16×16×64 by default)
- Two-layer Perlin noise: continental (mountains/valleys) + detail (surface roughness)
- Biome layering: stone → dirt (4 blocks) → grass; snow above `SnowAltitude`
- Asynchronous chunk generation and mesh build on TaskGraph (MPSC queue, no hitches)
- Pooled `UProceduralMeshComponent`s with chunk streaming based on `RenderDistance`
- Block mining with per-type mining time and a shader-driven progress overlay via `MaterialParameterCollection`
- Block placement from the selected hotbar slot (4 slots)
- Save/load to named slots — only the modification delta is stored, plus seed and player state
- Main menu, HUD, and save-slot widgets (UMG)

## Source layout

```
Source/VoxelWorld/
├── BlockType.h              EBlockType: Air, Stone, Dirt, Grass, Snow
├── ChunkData.h              FChunkData — single-chunk block storage and helpers
├── ChunkWorldSubsystem.*    UWorldSubsystem: streaming, cache, queues, block API
├── WorldGenerator.*         FWorldGenParams + thread-safe static chunk generation
├── WorldMeshActor.*         AWorldMeshActor: procedural mesh, sections per block type, collision
├── VoxelWorldSettings.h     UDataAsset with world, material and MPC settings
├── InventoryTypes.h         FInventorySlot
├── MCGameMode.*             AMCGameMode: NewWorld / SaveWorld / LoadWorld / GetSaveSlots
├── MCSaveGame.h             USaveGame: seed, block delta, player position/inventory
└── PlayerCharacter/
    ├── MCPlayerCharacter.*  ACharacter: crosshair trace, mining, placement, inventory
    └── MCPlayerController.* APlayerController: EnhancedInput bindings, HUD, menu
```

## Content

```
Content/
├── LVL_World.umap                  Gameplay map
├── Chunks/                         M_BaseMaterial + MI_Stone/Dirt/Grass/Snow + textures
├── Data/
│   ├── DA_WorldSettings.uasset     UVoxelWorldSettings (seed, biomes, materials)
│   └── MPC_Mining.uasset           Mining-progress MPC consumed by the block shader
├── GameSources/
│   ├── BP_GameMode / BP_PlayerController / BP_PlayerPawn
│   └── Inputs/                     IMC + IA_Move/Look/Jump/Use/SecondUse/Item_01..04/MainMenu
└── UI/                             WG_MainMenu, WG_PlayerDefault, WG_SaveSlot, WG_InventoryItem
```

## Controls

| Action                | Input Action       |
|-----------------------|--------------------|
| Movement              | `IA_Move`          |
| Camera                | `IA_Look`          |
| Jump                  | `IA_Jump`          |
| Mine block (hold)     | `IA_Use`           |
| Place block           | `IA_SecondUse`     |
| Hotbar slots 1–4      | `IA_Item_01..04`   |
| Menu                  | `IA_MainMenu`      |

Key bindings live in `Content/GameSources/Inputs/MC_Game` (Input Mapping Context).

## How the world works

1. Each tick, `UChunkWorldSubsystem` computes the desired chunks around the player (`RenderDistance`) and requests missing ones via `RequestChunkAsync`.
2. Background tasks (`FWorldGenerator::GenerateChunk` + `AWorldMeshActor::PrepareChunkMesh`) build chunk data and prepare mesh sections, then push the result into a thread-safe `TQueue`.
3. On the game thread, ready chunks are applied in batches (`ChunksPerFrame`); `UProceduralMeshComponent`s are pulled from a reusable pool.
4. Player edits (mining/placing) live in `ModifiedBlocks` as a delta against the generated terrain — saves memory and serialization size.
5. On save, only seed + delta + player state are written. On load, the world is re-derived from the seed and the delta is applied on top.

## Build and run

1. Right-click `VoxelWorld.uproject` → **Generate Visual Studio project files**.
2. Open `VoxelWorld.uproject` in Unreal Engine 5.7 (let it rebuild modules if prompted), or build from the IDE via `VoxelWorld.sln` (Development Editor, Win64).
3. Launch the editor and press **Play** on `LVL_World`.

> Visual Studio 2022 with the C++ workload and a Windows SDK is required to rebuild the C++ module.

## World configuration

All generation parameters live in `Content/Data/DA_WorldSettings` (`UVoxelWorldSettings`):

- `Seed`, `RenderDistance`, `ChunkSize`, `ChunkHeight`
- `SeaLevel`, `SnowAltitude`
- Continental noise: `ContinentalAmplitude/Frequency/Octaves`
- Detail noise: `DetailAmplitude/Frequency/Octaves`, `Persistence`
- `BlockMaterials` — material per `EBlockType`
- `BlockMiningTime` — mining time (seconds) per block type
- `MiningMPC` — `MaterialParameterCollection` the block shader reads progress from

## Save / Load (Blueprint API on `AMCGameMode`)

- `NewWorld()` — new world with a random seed, inventory reset to defaults
- `SaveWorld(SlotName)` / `LoadWorld(SlotName)` / `DeleteSave(SlotName)`
- `GetSaveSlots()` / `DoesSaveExist(SlotName)`

Save files are written to `Saved/SaveGames/MC_<SlotName>.sav`.
