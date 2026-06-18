#include "Surface/SRPlanetSurfaceGrid.h"

#include "Surface/SRPlanetSurfaceGridLibrary.h"
#include "Surface/SRPlanetTerrainGenerator.h"

float USRPlanetSurfaceGrid::GetSurfaceHeightOffsetAtDirection_Implementation(FVector LocalUnitDirection) const
{
	return ComputeProceduralDynamicMeshHeight(LocalUnitDirection);
}

void USRPlanetSurfaceGrid::ConfigureProceduralTerrain(
	bool bNewDynamicMeshGeneration,
	int32 NewGenerationSeed,
	float NewDynamicMeshHeight,
	float NewDetailFrequency,
	int32 NewNoiseOctaves,
	float NewNoisePersistence)
{
	FSRDynamicMeshGeneration NewDynamicMeshGeneration = DynamicMeshGeneration;
	NewDynamicMeshGeneration.bDynamicMeshGeneration = bNewDynamicMeshGeneration;
	NewDynamicMeshGeneration.GenerationSeed = NewGenerationSeed;
	NewDynamicMeshGeneration.DynamicMeshHeight = FMath::Max(0.0f, NewDynamicMeshHeight);
	NewDynamicMeshGeneration.DetailFrequency = FMath::Max(0.01f, NewDetailFrequency);
	NewDynamicMeshGeneration.NoiseOctaves = FMath::Max(1, NewNoiseOctaves);
	NewDynamicMeshGeneration.NoisePersistence = FMath::Clamp(NewNoisePersistence, 0.0f, 1.0f);
	ConfigureTerrain(NewDynamicMeshGeneration);
}

void USRPlanetSurfaceGrid::ConfigureTerrain(const FSRDynamicMeshGeneration& NewDynamicMeshGeneration)
{
	DynamicMeshGeneration = NewDynamicMeshGeneration;
	DynamicMeshGeneration.DynamicMeshHeight = FMath::Max(0.0f, DynamicMeshGeneration.DynamicMeshHeight);
	DynamicMeshGeneration.ContinentFrequency = FMath::Max(0.01f, DynamicMeshGeneration.ContinentFrequency);
	DynamicMeshGeneration.MountainFrequency = FMath::Max(0.01f, DynamicMeshGeneration.MountainFrequency);
	DynamicMeshGeneration.DetailFrequency = FMath::Max(0.01f, DynamicMeshGeneration.DetailFrequency);
	DynamicMeshGeneration.ValleyStrength = FMath::Clamp(DynamicMeshGeneration.ValleyStrength, 0.0f, 1.0f);
	DynamicMeshGeneration.MountainStrength = FMath::Clamp(DynamicMeshGeneration.MountainStrength, 0.5f, 4.0f);
	DynamicMeshGeneration.NoiseStrength = FMath::Clamp(DynamicMeshGeneration.NoiseStrength, 0.0f, 1.0f);
	DynamicMeshGeneration.RiverStrength = FMath::Clamp(DynamicMeshGeneration.RiverStrength, 0.0f, 1.0f);
	DynamicMeshGeneration.LakeStrength = FMath::Clamp(DynamicMeshGeneration.LakeStrength, 0.0f, 1.0f);
	DynamicMeshGeneration.DetailStrength = FMath::Clamp(DynamicMeshGeneration.DetailStrength, 0.0f, 1.0f);
	DynamicMeshGeneration.MoistureFrequency = FMath::Max(0.01f, DynamicMeshGeneration.MoistureFrequency);
	DynamicMeshGeneration.TemperatureFrequency = FMath::Max(0.01f, DynamicMeshGeneration.TemperatureFrequency);
	DynamicMeshGeneration.NoiseOctaves = FMath::Max(1, DynamicMeshGeneration.NoiseOctaves);
	DynamicMeshGeneration.NoisePersistence = FMath::Clamp(DynamicMeshGeneration.NoisePersistence, 0.0f, 1.0f);
	DynamicMeshGeneration.OceanThreshold = FMath::Clamp(DynamicMeshGeneration.OceanThreshold, -1.0f, 1.0f);
	DynamicMeshGeneration.AtmosphereThreshold = FMath::Max(0.01f, DynamicMeshGeneration.AtmosphereThreshold);
	if (DynamicMeshGeneration.BiomeDataAssets.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Planet surface grid terrain requires Profile BiomeDataAssets."));
	}
	else
	{
		DynamicMeshGeneration.NormalizeBiomeMaterials(DynamicMeshGeneration.BiomeDataAssets);
	}
	bCellsDirty = true;
	bGridMeshDirty = true;
	if (bGridVisible)
	{
		RebuildGrid();
	}
}

FSRPlanetTerrainSample USRPlanetSurfaceGrid::GetTerrainSampleAtDirection(FVector LocalUnitDirection) const
{
	FSRBiomeSampleContext SampleContext;
	SampleContext.LocalUnitDirection = LocalUnitDirection.GetSafeNormal();
	if (SampleContext.LocalUnitDirection.IsNearlyZero())
	{
		SampleContext.LocalUnitDirection = FVector::UpVector;
	}

	FSRPlanetSurfaceGridCellId CellId;
	FVector2D FaceCoordinates = FVector2D::ZeroVector;
	if (USRPlanetSurfaceGridLibrary::ProjectDirectionToCubeSphereCellId(
		SampleContext.LocalUnitDirection,
		FMath::Max(1, FaceResolution),
		CellId,
		FaceCoordinates))
	{
		SampleContext.Face = CellId.Face;
		SampleContext.CellX = CellId.CellX;
		SampleContext.CellY = CellId.CellY;
		SampleContext.FaceResolution = FMath::Max(1, FaceResolution);
		SampleContext.FaceUV = FaceCoordinates;
	}

	FSRPlanetTerrainSample Sample = FSRPlanetTerrainGenerator::SampleTerrain(SampleContext, DynamicMeshGeneration);
	const float HeightStep = GetTerrainHeightStep();
	if (HeightStep > KINDA_SMALL_NUMBER)
	{
		Sample.HeightOffset = FMath::RoundToFloat(Sample.HeightOffset / HeightStep) * HeightStep;
	}
	return Sample;
}

float USRPlanetSurfaceGrid::GetTerrainHeightStep() const
{
	const float SafeDynamicMeshHeight = FMath::Max(0.0f, DynamicMeshGeneration.DynamicMeshHeight);
	return DynamicMeshGeneration.bMinecraft && DynamicMeshGeneration.bDynamicMeshGeneration && SafeDynamicMeshHeight > KINDA_SMALL_NUMBER
		? (2.0f * FMath::Max(1.0f, PlanetRadius)) / static_cast<float>(FMath::Max(1, FaceResolution))
		: 0.0f;
}

FVector USRPlanetSurfaceGrid::ResolveLocalSurfacePoint(const FVector& LocalUnitDirection, float HeightOffset) const
{
	const FVector LocalDirection = LocalUnitDirection.GetSafeNormal();
	if (LocalDirection.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	const float SurfaceHeightOffset = GetSurfaceHeightOffsetAtDirection(LocalDirection);
	const FVector LocalBasePoint = LocalDirection * FMath::Max(1.0f, PlanetRadius + SurfaceHeightOffset);
	const FVector LocalSurfaceNormal = DynamicMeshGeneration.bDynamicMeshGeneration
		? ComputeProceduralSurfaceNormal(LocalDirection)
		: LocalDirection;
	return LocalBasePoint + (LocalSurfaceNormal.GetSafeNormal() * HeightOffset);
}

FVector USRPlanetSurfaceGrid::ResolveWorldSurfacePoint(const FVector& LocalUnitDirection, float HeightOffset) const
{
	return GetComponentTransform().TransformPosition(ResolveLocalSurfacePoint(LocalUnitDirection, HeightOffset));
}

float USRPlanetSurfaceGrid::ComputeProceduralDynamicMeshHeight(FVector LocalUnitDirection) const
{
	return GetTerrainSampleAtDirection(LocalUnitDirection).HeightOffset;
}

FVector USRPlanetSurfaceGrid::ComputeProceduralSurfaceNormal(FVector LocalUnitDirection) const
{
	const FVector Direction = LocalUnitDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return FVector::UpVector;
	}

	FVector TangentA = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
	if (TangentA.IsNearlyZero())
	{
		TangentA = FVector::CrossProduct(Direction, FVector::RightVector).GetSafeNormal();
	}
	const FVector TangentB = FVector::CrossProduct(Direction, TangentA).GetSafeNormal();
	if (TangentA.IsNearlyZero() || TangentB.IsNearlyZero())
	{
		return Direction;
	}

	auto ResolveBasePoint = [this](const FVector& SampleDirection)
	{
		const FVector SafeDirection = SampleDirection.GetSafeNormal();
		const float SurfaceHeightOffset = GetSurfaceHeightOffsetAtDirection(SafeDirection);
		return SafeDirection * FMath::Max(1.0f, PlanetRadius + SurfaceHeightOffset);
	};

	constexpr float NormalSampleStep = 0.003f;
	const FVector PointA0 = ResolveBasePoint(Direction - TangentA * NormalSampleStep);
	const FVector PointA1 = ResolveBasePoint(Direction + TangentA * NormalSampleStep);
	const FVector PointB0 = ResolveBasePoint(Direction - TangentB * NormalSampleStep);
	const FVector PointB1 = ResolveBasePoint(Direction + TangentB * NormalSampleStep);

	FVector Normal = FVector::CrossProduct(PointA1 - PointA0, PointB1 - PointB0).GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		return Direction;
	}

	if (FVector::DotProduct(Normal, Direction) < 0.0f)
	{
		Normal *= -1.0f;
	}
	return Normal;
}