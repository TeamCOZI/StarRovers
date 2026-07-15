#include "Celestial/SRCelestialBody.h"

#include "Utility/SRLog.h"
#include "SRCelestialBodyLog.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const FName PlanetCenterMaterialParameterName(TEXT("PlanetCenterWS"));
	const FName ToonLineEnabledMaterialParameterName(TEXT("ToonLineEnabled"));
	const FName ToonCellGridLineEnabledMaterialParameterName(TEXT("ToonCellGridLineEnabled"));
	const FName ToonLineColorMaterialParameterName(TEXT("ToonLineColor"));
	const FName ToonLineThicknessMaterialParameterName(TEXT("ToonLineThickness"));

	void ApplyCelestialBodyMaterialParameters(
		UMaterialInstanceDynamic* MaterialInstance,
		const FSRToonOutlineSettings& ToonOutlineSettings,
		const FVector& PlanetCenterWS)
	{
		if (!IsValid(MaterialInstance))
		{
			return;
		}

		MaterialInstance->SetVectorParameterValue(PlanetCenterMaterialParameterName, FLinearColor(PlanetCenterWS));
		const bool bEnableMaterialCellGridLine =
			ToonOutlineSettings.bEnableToonOutline
			&& (!ToonOutlineSettings.bUseFeatureEdgeToonOutline || ToonOutlineSettings.bDrawMaterialCellGridToonOutline);
		MaterialInstance->SetScalarParameterValue(ToonLineEnabledMaterialParameterName, bEnableMaterialCellGridLine ? 1.0f : 0.0f);
		MaterialInstance->SetScalarParameterValue(ToonCellGridLineEnabledMaterialParameterName, bEnableMaterialCellGridLine ? 1.0f : 0.0f);
		MaterialInstance->SetVectorParameterValue(ToonLineColorMaterialParameterName, ToonOutlineSettings.ToonLineColor);
		MaterialInstance->SetScalarParameterValue(ToonLineThicknessMaterialParameterName, FMath::Max(0.0f, ToonOutlineSettings.ToonLineThickness));
	}

	void RefreshComponentMaterialParameters(
		UMeshComponent* MeshComponent,
		const FSRToonOutlineSettings& ToonOutlineSettings,
		const FVector& PlanetCenterWS)
	{
		if (!IsValid(MeshComponent))
		{
			return;
		}

		const int32 MaterialCount = MeshComponent->GetNumMaterials();
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			ApplyCelestialBodyMaterialParameters(
				Cast<UMaterialInstanceDynamic>(MeshComponent->GetMaterial(MaterialIndex)),
				ToonOutlineSettings,
				PlanetCenterWS);
		}
	}

	UMaterialInstanceDynamic* ResolveComponentMaterialInstance(
		UObject* Outer,
		UMeshComponent* MeshComponent,
		int32 MaterialSlotIndex,
		UMaterialInterface* DesiredBaseMaterial)
	{
		if (!IsValid(Outer) || !IsValid(MeshComponent) || !IsValid(DesiredBaseMaterial))
		{
			return nullptr;
		}

		UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(MeshComponent->GetMaterial(MaterialSlotIndex));
		const UMaterialInstance* DynamicMaterialInstance = DynamicMaterial;
		if (!IsValid(DynamicMaterial) || DynamicMaterialInstance->Parent != DesiredBaseMaterial)
		{
			DynamicMaterial = UMaterialInstanceDynamic::Create(DesiredBaseMaterial, Outer);
			MeshComponent->SetMaterial(MaterialSlotIndex, DynamicMaterial);
		}
		return DynamicMaterial;
	}

	UMaterialInterface* ResolveDesiredBaseBodyMaterial(
		UMaterialInterface* ConfiguredMaterial,
		bool bIsStellarBody,
		UMaterialInterface* CurrentAssignedMaterial)
	{
		if (IsValid(ConfiguredMaterial))
		{
			return ConfiguredMaterial;
		}

		return bIsStellarBody && IsValid(CurrentAssignedMaterial)
			? CurrentAssignedMaterial
			: nullptr;
	}

	void ApplyBaseMaterialToBodyMeshes(
		UObject* Outer,
		UStaticMeshComponent* StaticMeshComponent,
		const TArray<TObjectPtr<UDynamicMeshComponent>>& DynamicMeshFaces,
		UMaterialInterface* DesiredBaseMaterial)
	{
		if (IsValid(StaticMeshComponent))
		{
			ResolveComponentMaterialInstance(Outer, StaticMeshComponent, 0, DesiredBaseMaterial);
		}

		for (UDynamicMeshComponent* DynamicMeshComponent : DynamicMeshFaces)
		{
			if (IsValid(DynamicMeshComponent))
			{
				ResolveComponentMaterialInstance(Outer, DynamicMeshComponent, 0, DesiredBaseMaterial);
			}
		}
	}

	void ApplyBiomeMaterialsToDynamicMeshFaces(
		UObject* Outer,
		const FSRDynamicMeshGeneration& DynamicMeshGeneration,
		const TArray<TObjectPtr<UDynamicMeshComponent>>& DynamicMeshFaces)
	{
		for (const FSRBiomeMaterialEntry& BiomeMaterialEntry : DynamicMeshGeneration.BiomeMaterials)
		{
			UMaterialInterface* BiomeMaterial = BiomeMaterialEntry.Material.Get();
			const int32 MaterialSlotIndex = DynamicMeshGeneration.GetBiomeMaterialSlotIndex(BiomeMaterialEntry.BiomeId);
			if (!IsValid(BiomeMaterial) || MaterialSlotIndex <= 0)
			{
				continue;
			}

			for (UDynamicMeshComponent* DynamicMeshComponent : DynamicMeshFaces)
			{
				if (IsValid(DynamicMeshComponent))
				{
					ResolveComponentMaterialInstance(Outer, DynamicMeshComponent, MaterialSlotIndex, BiomeMaterial);
				}
			}
		}
	}
}

bool ASRCelestialBody::ApplyCelestialBodyMeshMaterials()
{
	if (!IsValid(CelestialBodyDynamicMesh.Get()))
	{
		return false;
	}

	UMaterialInterface* DesiredBaseMaterial = ResolveDesiredBaseBodyMaterial(
		Material,
		IsStellarBody(),
		CelestialBodyDynamicMesh->GetMaterial(0));
	if (!IsValid(DesiredBaseMaterial))
	{
		SR_LOG(Celestial, LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires Material."), *GetName());
		return false;
	}

	ApplyBaseMaterialToBodyMeshes(this, CelestialBodyStaticMesh.Get(), CelestialBodyDynamicMeshFaces, DesiredBaseMaterial);
	ApplyBiomeMaterialsToDynamicMeshFaces(this, DynamicMeshGeneration, CelestialBodyDynamicMeshFaces);
	RefreshMaterialParameters();
	return true;
}

void ASRCelestialBody::RefreshMaterialParameters()
{
	const FVector PlanetCenterWS = GetActorLocation();
	for (UDynamicMeshComponent* DynamicMeshComponent : CelestialBodyDynamicMeshFaces)
	{
		RefreshComponentMaterialParameters(DynamicMeshComponent, ToonOutlineSettings, PlanetCenterWS);
	}
	RefreshComponentMaterialParameters(CelestialBodyStaticMesh.Get(), ToonOutlineSettings, PlanetCenterWS);
}

UMaterialInstanceDynamic* ASRCelestialBody::GetActiveBodyDynamicMaterial() const
{
	return IsValid(CelestialBodyDynamicMesh.Get())
		? Cast<UMaterialInstanceDynamic>(CelestialBodyDynamicMesh->GetMaterial(0))
		: nullptr;
}
