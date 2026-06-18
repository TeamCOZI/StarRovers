#include "Surface/SRPlanetTerrainGenerator.h"

FLinearColor FSRPlanetTerrainGenerator::GetBiomeColor(ESRPlanetBiome Biome, float HeightAlpha, float Moisture, float Temperature)
{
	FLinearColor BaseColor;
	switch (Biome)
	{
	case ESRPlanetBiome::Ocean:
		BaseColor = FLinearColor(0.018f, 0.105f, 0.255f, 1.0f);
		break;
	case ESRPlanetBiome::Coast:
		BaseColor = FLinearColor(0.62f, 0.56f, 0.38f, 1.0f);
		break;
	case ESRPlanetBiome::Snow:
		BaseColor = FLinearColor(0.78f, 0.80f, 0.77f, 1.0f);
		break;
	default:
		BaseColor = FLinearColor(0.28f, 0.46f, 0.23f, 1.0f);
		break;
	}

	const float HeightShade = FMath::Lerp(0.92f, 1.08f, FMath::Clamp((HeightAlpha + 1.0f) * 0.5f, 0.0f, 1.0f));
	const float MoistureShade = FMath::Lerp(0.96f, 1.06f, FMath::Clamp(Moisture, 0.0f, 1.0f));
	const float TemperatureShade = FMath::Lerp(0.97f, 1.03f, FMath::Clamp(Temperature, 0.0f, 1.0f));
	BaseColor *= HeightShade * MoistureShade * TemperatureShade;
	if (Biome == ESRPlanetBiome::Ocean)
	{
		const float ShallowWater = FMath::Clamp(HeightAlpha + 0.42f, 0.0f, 1.0f);
		BaseColor = FLinearColor::LerpUsingHSV(BaseColor, FLinearColor(0.04f, 0.30f, 0.46f, 1.0f), ShallowWater * 0.45f);
	}
	else if (Biome == ESRPlanetBiome::Coast)
	{
		const float WetCoast = FMath::Clamp(Moisture, 0.0f, 1.0f);
		BaseColor = FLinearColor::LerpUsingHSV(BaseColor, FLinearColor(0.20f, 0.46f, 0.24f, 1.0f), WetCoast * 0.35f);
	}
	BaseColor.A = 1.0f;
	return BaseColor;
}
