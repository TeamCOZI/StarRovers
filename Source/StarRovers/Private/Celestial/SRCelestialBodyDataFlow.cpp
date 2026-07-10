#include "Celestial/SRCelestialBody.h"

#include "Utility/SRLog.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Surface/SRPlanetTerrainProfileDataAsset.h"

namespace
{
	bool IsProceduralTerrainBody(ESRCelestialBodyCategory BodyCategory)
	{
		return BodyCategory == ESRCelestialBodyCategory::Planet
			|| BodyCategory == ESRCelestialBodyCategory::Moon;
	}

	bool HasCelestialBodyMeshSource(
		const UStaticMesh* StaticMesh,
		const USRDynamicMeshBaseDataAsset* DynamicMeshBaseDataAsset)
	{
		return IsValid(StaticMesh) || IsValid(DynamicMeshBaseDataAsset);
	}

	bool ShouldAutoApplyBodyData(
		bool bHasBegunPlay,
		const UWorld* World,
		const UStaticMesh* StaticMesh,
		const USRDynamicMeshBaseDataAsset* DynamicMeshBaseDataAsset,
		const UMaterialInterface* Material)
	{
		return bHasBegunPlay
			&& World
			&& World->IsGameWorld()
			&& HasCelestialBodyMeshSource(StaticMesh, DynamicMeshBaseDataAsset)
			&& IsValid(Material);
	}

	void ApplyBodyMeshTransform(USceneComponent* MeshComponent, float BodyScale)
	{
		if (!IsValid(MeshComponent))
		{
			return;
		}

		MeshComponent->SetRelativeLocation(FVector::ZeroVector);
		MeshComponent->SetRelativeRotation(FRotator::ZeroRotator);
		MeshComponent->SetRelativeScale3D(FVector(BodyScale));
	}

	float ResolveScaledBodyRadius(
		const UStaticMesh* StaticMesh,
		const USRDynamicMeshBaseDataAsset* DynamicMeshBaseDataAsset,
		float BodyScale)
	{
		if (IsValid(StaticMesh))
		{
			return StaticMesh->GetBounds().SphereRadius * BodyScale;
		}

		return IsValid(DynamicMeshBaseDataAsset)
			? DynamicMeshBaseDataAsset->GetSafeBaseRadius() * BodyScale
			: 0.0f;
	}

	void ApplyClickSphereCollisionRadius(USphereComponent* ClickSphereCollision, float BodyRadius)
	{
		if (!IsValid(ClickSphereCollision))
		{
			return;
		}

		ClickSphereCollision->SetRelativeLocation(FVector::ZeroVector);
		ClickSphereCollision->SetRelativeRotation(FRotator::ZeroRotator);
		ClickSphereCollision->SetRelativeScale3D(FVector::OneVector);
		ClickSphereCollision->SetSphereRadius(FMath::Max(BodyRadius, 1.0f));
	}
}

void ASRCelestialBody::CopyBodyDataFields(const FSRCelestialBodyData& NewData)
{
	VariableName = NewData.VariableName;
	BodyCategory = NewData.BodyCategory;
	FocusZoomMultiplier = NewData.FocusZoomMultiplier;
	GenerationSeed = NewData.GenerationSeed;
	bRandomizeGenerationSeedEachRun = NewData.bRandomizeGenerationSeedEachRun;
	TerrainProfileDataAsset = NewData.TerrainProfileDataAsset;
	ProfileNaturalStructureSpawnRuleOverrides = NewData.ProfileNaturalStructureSpawnRuleOverrides;
	DynamicMeshGeneration = NewData.DynamicMeshGeneration;
	ToonOutlineSettings = NewData.ToonOutlineSettings;
	Scale = NewData.Scale;
	StaticMesh = NewData.StaticMesh;
	DynamicMeshBaseDataAsset = NewData.DynamicMeshBaseDataAsset;
	Material = NewData.Material;
	Mass = NewData.Mass;
	GravityRatio = NewData.GravityRatio;
	GravityRadiusRatio = NewData.GravityRadiusRatio;
	ShowGravityLine = NewData.ShowGravityLine;
	GravityLineColor = NewData.GravityLineColor;
	GravityLineOpacity = NewData.GravityLineOpacity;
	GravityLineSegments = NewData.GravityLineSegments;
	GravityLineThickness = NewData.GravityLineThickness;
}

void ASRCelestialBody::ApplyTerrainProfileData()
{
	if (IsValid(TerrainProfileDataAsset.Get()))
	{
		TerrainProfileDataAsset->ApplyToDynamicMeshGeneration(DynamicMeshGeneration);
		return;
	}

	if (IsProceduralTerrainBody(BodyCategory))
	{
		SR_LOG(Celestial, LogTemp, Error, TEXT("Celestial body '%s' requires TerrainProfileDataAsset for procedural terrain."), *GetName());
	}
}

bool ASRCelestialBody::ShouldAutoApplyDataAfterSet() const
{
	return ShouldAutoApplyBodyData(
		HasActorBegunPlay(),
		GetWorld(),
		StaticMesh.Get(),
		DynamicMeshBaseDataAsset.Get(),
		Material.Get());
}

void ASRCelestialBody::SanitizeBodyRuntimeValues()
{
	Scale = FMath::Max(0.0f, Scale);
	Mass = FMath::Max(0.0f, Mass);
	GravityRatio = FMath::Max(0.0f, GravityRatio);
	GravityRadiusRatio = FMath::Max(0.0f, GravityRadiusRatio);
	GravityLineOpacity = FMath::Clamp(GravityLineOpacity, 0.0f, 1.0f);
	GravityLineSegments = FMath::Max(3, GravityLineSegments);
	GravityLineThickness = FMath::Max(0.0f, GravityLineThickness);
	FocusZoomMultiplier = FMath::Max(0.0f, FocusZoomMultiplier);
}

void ASRCelestialBody::ApplyBodyMeshTransforms()
{
	if (IsValid(CelestialBodyDynamicMesh.Get()))
	{
		for (UDynamicMeshComponent* DynamicMeshComponent : CelestialBodyDynamicMeshFaces)
		{
			ApplyBodyMeshTransform(DynamicMeshComponent, Scale);
		}
	}

	ApplyBodyMeshTransform(CelestialBodyStaticMesh.Get(), Scale);
}

void ASRCelestialBody::UpdateDynamicMeshBuildStateForCurrentData()
{
	if (DynamicMeshState.HasBuild() && !DynamicMeshState.HasBuildHash(ComputeDynamicMeshBuildHash()))
	{
		ResetDynamicMeshCellColorData();
	}
}

bool ASRCelestialBody::ShouldBuildDynamicMeshForCurrentWorld() const
{
	const bool bIsGameWorld = GetWorld() && GetWorld()->IsGameWorld();
	return !bIsGameWorld || (IsValid(CelestialBodyDynamicMesh.Get()) && CelestialBodyDynamicMesh->IsVisible());
}

void ASRCelestialBody::ApplyClickCollisionForCurrentBody()
{
	const float BodyRadius = ResolveScaledBodyRadius(StaticMesh.Get(), DynamicMeshBaseDataAsset.Get(), Scale);
	ApplyClickSphereCollisionRadius(ClickSphereCollision.Get(), BodyRadius);
}

FSRCelestialBodyData ASRCelestialBody::BuildBodyDataSnapshot() const
{
	FSRCelestialBodyData CurrentData;
	CurrentData.VariableName = VariableName;
	CurrentData.BodyCategory = BodyCategory;
	CurrentData.FocusZoomMultiplier = FocusZoomMultiplier;
	CurrentData.GenerationSeed = GenerationSeed;
	CurrentData.bRandomizeGenerationSeedEachRun = bRandomizeGenerationSeedEachRun;
	CurrentData.TerrainProfileDataAsset = TerrainProfileDataAsset;
	CurrentData.ProfileNaturalStructureSpawnRuleOverrides = ProfileNaturalStructureSpawnRuleOverrides;
	CurrentData.DynamicMeshGeneration = DynamicMeshGeneration;
	CurrentData.Scale = Scale;
	CurrentData.StaticMesh = StaticMesh;
	CurrentData.DynamicMeshBaseDataAsset = DynamicMeshBaseDataAsset;
	CurrentData.Material = Material;
	CurrentData.ToonOutlineSettings = ToonOutlineSettings;
	CurrentData.Mass = Mass;
	CurrentData.GravityRatio = GravityRatio;
	CurrentData.GravityRadiusRatio = GravityRadiusRatio;
	CurrentData.ShowGravityLine = ShowGravityLine;
	CurrentData.GravityLineColor = GravityLineColor;
	CurrentData.GravityLineOpacity = GravityLineOpacity;
	CurrentData.GravityLineSegments = GravityLineSegments;
	CurrentData.GravityLineThickness = GravityLineThickness;
	return CurrentData;
}
