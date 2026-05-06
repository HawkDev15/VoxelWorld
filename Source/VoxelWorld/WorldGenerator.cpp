#include "WorldGenerator.h"

FChunkData FWorldGenerator::GenerateChunk(const FWorldGenParams& Params, const FIntPoint& ChunkIndex)
{
	FChunkData Data;
	Data.Init(ChunkIndex, Params.ChunkSizeX, Params.ChunkSizeY, Params.ChunkSizeZ);

	const int32 SX = Params.ChunkSizeX;
	const int32 SY = Params.ChunkSizeY;
	const int32 SZ = Params.ChunkSizeZ;

	const int32 BaseX = ChunkIndex.X * SX;
	const int32 BaseY = ChunkIndex.Y * SY;

	// Different seed offsets for each noise layer so they don't correlate
	const float SeedOffset1 = static_cast<float>(Params.Seed) * 1000.f;
	const float SeedOffset2 = static_cast<float>(Params.Seed) * 3571.f;

	for (int32 X = 0; X < SX; ++X)
	{
		for (int32 Y = 0; Y < SY; ++Y)
		{
			const float WorldX = static_cast<float>(BaseX + X);
			const float WorldY = static_cast<float>(BaseY + Y);

			const float Continental = SampleNoise(WorldX, WorldY,
				Params.ContinentalFrequency, Params.ContinentalOctaves, Params.Persistence, SeedOffset1);

			const float Detail = SampleNoise(WorldX, WorldY,
				Params.DetailFrequency, Params.DetailOctaves, Params.Persistence, SeedOffset2);

			const int32 SurfaceZ = FMath::Clamp(
				Params.SeaLevel
				+ FMath::RoundToInt(Continental * Params.ContinentalAmplitude)
				+ FMath::RoundToInt(Detail * Params.DetailAmplitude),
				1, SZ - 1);

			for (int32 Z = 0; Z <= SurfaceZ; ++Z)
			{
				EBlockType Type;

				if (Z == SurfaceZ)
				{
					Type = SurfaceZ >= Params.SnowAltitude ? EBlockType::Snow : EBlockType::Grass;
				}
				else if (Z >= SurfaceZ - 4)
				{
					Type = EBlockType::Dirt;
				}
				else
				{
					Type = EBlockType::Stone;
				}

				Data.SetBlock(X, Y, Z, Type);
			}
		}
	}

	return Data;
}

float FWorldGenerator::SampleNoise(float WorldX, float WorldY, float Frequency,
	int32 Octaves, float Persistence, float SeedOffset)
{
	float Total = 0.f;
	float Amp = 1.f;
	float Freq = Frequency;
	float MaxAmp = 0.f;

	for (int32 i = 0; i < Octaves; ++i)
	{
		const float Nx = (WorldX + SeedOffset) * Freq;
		const float Ny = (WorldY + SeedOffset) * Freq;

		Total += FMath::PerlinNoise2D(FVector2D(Nx, Ny)) * Amp;

		MaxAmp += Amp;
		Amp *= Persistence;
		Freq *= 2.f;
	}

	return Total / MaxAmp;
}
