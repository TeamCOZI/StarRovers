#include "SRSpaceLogisticsHubEndpointResolver.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	FVector ResolveCellOutwardNormal(
		const FVector& SurfaceCenter,
		const FSRPlanetSurfaceGridCellInfo& CellInfo)
	{
		FVector OutwardNormal = CellInfo.WorldNormal.GetSafeNormal();
		if (OutwardNormal.IsNearlyZero())
		{
			OutwardNormal = (CellInfo.WorldCenter - SurfaceCenter).GetSafeNormal();
		}
		else if (FVector::DotProduct(OutwardNormal, CellInfo.WorldCenter - SurfaceCenter) < 0.0f)
		{
			OutwardNormal *= -1.0f;
		}
		if (OutwardNormal.IsNearlyZero())
		{
			OutwardNormal = FVector::UpVector;
		}
		return OutwardNormal;
	}

	bool BuildHubEndpointFromPlacedStructure(
		AActor* BodyActor,
		const FSRPlacedStructureInstance& PlacedStructure,
		FSRSpaceLogisticsHubEndpoint& OutHubEndpoint)
	{
		OutHubEndpoint = FSRSpaceLogisticsHubEndpoint();
		if (!IsValid(BodyActor)
			|| PlacedStructure.OccupantId.IsNone()
			|| !IsValid(PlacedStructure.StructureDataAsset.Get()))
		{
			return false;
		}

		const FSRStructureData StructureData = PlacedStructure.StructureDataAsset->BuildData();
		OutHubEndpoint.BodyActor = BodyActor;
		OutHubEndpoint.HubOccupantId = PlacedStructure.OccupantId;
		OutHubEndpoint.StructureId = PlacedStructure.StructureId.IsNone()
			? StructureData.StructureId
			: PlacedStructure.StructureId;
		OutHubEndpoint.DisplayName = StructureData.DisplayName.IsEmpty()
			? FText::FromName(OutHubEndpoint.StructureId)
			: StructureData.DisplayName;
		OutHubEndpoint.OriginCellId = PlacedStructure.OriginCellId;
		OutHubEndpoint.FootprintCellIds = PlacedStructure.FootprintCellIds;
		return OutHubEndpoint.IsValid();
	}
}

void FSRSpaceLogisticsHubEndpointResolver::Rebuild(UWorld* World, TArray<FSRSpaceLogisticsHubEndpoint>& OutHubEndpoints)
{
	OutHubEndpoints.Reset();

	if (!IsValid(World))
	{
		return;
	}

	USRCelestialBodyRegistrySubsystem* CelestialRegistry = World->GetSubsystem<USRCelestialBodyRegistrySubsystem>();
	if (!IsValid(CelestialRegistry))
	{
		return;
	}

	TArray<AActor*> BodyActors;
	CelestialRegistry->GetCelestialBodies(BodyActors);
	if (BodyActors.IsEmpty())
	{
		CelestialRegistry->RefreshCelestialBodies();
		CelestialRegistry->GetCelestialBodies(BodyActors);
	}

	OutHubEndpoints.Reserve(BodyActors.Num());
	for (AActor* BodyActor : BodyActors)
	{
		if (!IsValid(BodyActor))
		{
			continue;
		}

		const USRStructureInstanceManagerComponent* StructureInstanceManager = BodyActor->FindComponentByClass<USRStructureInstanceManagerComponent>();
		const USRFacilityNetworkComponent* FacilityNetwork = BodyActor->FindComponentByClass<USRFacilityNetworkComponent>();
		if (!IsValid(StructureInstanceManager) || !IsValid(FacilityNetwork))
		{
			continue;
		}

		TArray<FSRPlacedStructureInstance> PlacedStructures;
		StructureInstanceManager->GetPlacedStructures(PlacedStructures);
		for (const FSRPlacedStructureInstance& PlacedStructure : PlacedStructures)
		{
			if (PlacedStructure.OccupantId.IsNone()
				|| PlacedStructure.bNaturalStructure
				|| !FacilityNetwork->IsHubFacility(PlacedStructure.OccupantId))
			{
				continue;
			}

			FSRSpaceLogisticsHubEndpoint HubEndpoint;
			if (BuildHubEndpointFromPlacedStructure(BodyActor, PlacedStructure, HubEndpoint))
			{
				OutHubEndpoints.Add(HubEndpoint);
			}
		}
	}
}

bool FSRSpaceLogisticsHubEndpointResolver::Build(
	AActor* BodyActor,
	FName HubOccupantId,
	FSRSpaceLogisticsHubEndpoint& OutHubEndpoint)
{
	OutHubEndpoint = FSRSpaceLogisticsHubEndpoint();
	if (!IsValid(BodyActor) || HubOccupantId.IsNone())
	{
		return false;
	}

	const USRStructureInstanceManagerComponent* StructureInstanceManager = BodyActor->FindComponentByClass<USRStructureInstanceManagerComponent>();
	const USRFacilityNetworkComponent* FacilityNetwork = BodyActor->FindComponentByClass<USRFacilityNetworkComponent>();
	if (!IsValid(StructureInstanceManager) || !IsValid(FacilityNetwork) || !FacilityNetwork->IsHubFacility(HubOccupantId))
	{
		return false;
	}

	FSRPlacedStructureInstance PlacedStructure;
	if (!StructureInstanceManager->GetPlacedStructure(HubOccupantId, PlacedStructure)
		|| !IsValid(PlacedStructure.StructureDataAsset.Get()))
	{
		return false;
	}

	return BuildHubEndpointFromPlacedStructure(BodyActor, PlacedStructure, OutHubEndpoint);
}

bool FSRSpaceLogisticsHubEndpointResolver::BuildSaveData(
	const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
	FSRSpaceLogisticsHubEndpointSaveData& OutSaveData)
{
	OutSaveData = FSRSpaceLogisticsHubEndpointSaveData();
	AActor* BodyActor = HubEndpoint.BodyActor.Get();
	if (!HubEndpoint.IsValid() || !IsValid(BodyActor))
	{
		return false;
	}

	OutSaveData.BodyActorName = BodyActor->GetFName();
	OutSaveData.BodyVariableName = USRCelestialBodyRuntimeLibrary::GetCelestialVariableName(BodyActor).ToString();
	OutSaveData.HubOccupantId = HubEndpoint.HubOccupantId;
	OutSaveData.StructureId = HubEndpoint.StructureId;
	return OutSaveData.IsValid();
}

bool FSRSpaceLogisticsHubEndpointResolver::ResolveSaved(
	UWorld* World,
	TArray<FSRSpaceLogisticsHubEndpoint>& CachedHubEndpoints,
	const FSRSpaceLogisticsHubEndpointSaveData& SaveData,
	FSRSpaceLogisticsHubEndpoint& OutHubEndpoint)
{
	OutHubEndpoint = FSRSpaceLogisticsHubEndpoint();
	if (!SaveData.IsValid())
	{
		return false;
	}

	Rebuild(World, CachedHubEndpoints);
	for (const FSRSpaceLogisticsHubEndpoint& HubEndpoint : CachedHubEndpoints)
	{
		AActor* BodyActor = HubEndpoint.BodyActor.Get();
		if (!IsValid(BodyActor) || HubEndpoint.HubOccupantId != SaveData.HubOccupantId)
		{
			continue;
		}

		const bool bMatchesActorName = !SaveData.BodyActorName.IsNone() && BodyActor->GetFName() == SaveData.BodyActorName;
		const FString BodyVariableName = USRCelestialBodyRuntimeLibrary::GetCelestialVariableName(BodyActor).ToString();
		const bool bMatchesVariableName = !SaveData.BodyVariableName.IsEmpty() && BodyVariableName == SaveData.BodyVariableName;
		if (bMatchesActorName || bMatchesVariableName)
		{
			OutHubEndpoint = HubEndpoint;
			return true;
		}
	}

	return false;
}

bool FSRSpaceLogisticsHubEndpointResolver::ResolveCurrent(
	const FSRSpaceLogisticsHubEndpoint& CandidateHubEndpoint,
	FSRSpaceLogisticsHubEndpoint& OutHubEndpoint)
{
	OutHubEndpoint = FSRSpaceLogisticsHubEndpoint();
	if (!CandidateHubEndpoint.IsValid())
	{
		return false;
	}

	return Build(CandidateHubEndpoint.BodyActor.Get(), CandidateHubEndpoint.HubOccupantId, OutHubEndpoint);
}

bool FSRSpaceLogisticsHubEndpointResolver::ResolveWorldLocationWithHeightOffset(
	const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
	float HeightOffset,
	FVector& OutWorldLocation)
{
	OutWorldLocation = FVector::ZeroVector;
	AActor* BodyActor = HubEndpoint.BodyActor.Get();
	if (!HubEndpoint.IsValid() || !IsValid(BodyActor))
	{
		return false;
	}

	const USRPlanetSurfaceGrid* SurfaceGrid = BodyActor->FindComponentByClass<USRPlanetSurfaceGrid>();
	const USRStructureInstanceManagerComponent* StructureInstanceManager = BodyActor->FindComponentByClass<USRStructureInstanceManagerComponent>();
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	const FVector SurfaceCenter = SurfaceGrid->GetComponentTransform().GetLocation();
	FSRPlanetSurfaceGridCellId OriginCellId = HubEndpoint.OriginCellId;
	const TArray<FSRPlanetSurfaceGridCellId>* FootprintCellIds = &HubEndpoint.FootprintCellIds;
	FSRPlacedStructureInstance CurrentPlacedStructure;
	if (IsValid(StructureInstanceManager))
	{
		if (StructureInstanceManager->GetPlacedStructure(HubEndpoint.HubOccupantId, CurrentPlacedStructure))
		{
			OriginCellId = CurrentPlacedStructure.OriginCellId;
			if (!CurrentPlacedStructure.FootprintCellIds.IsEmpty())
			{
				FootprintCellIds = &CurrentPlacedStructure.FootprintCellIds;
			}
		}
	}

	FSRPlanetSurfaceGridCellInfo OriginCellInfo;
	if (SurfaceGrid->GetCellInfoById(OriginCellId, OriginCellInfo))
	{
		OutWorldLocation = OriginCellInfo.WorldCenter
			+ (ResolveCellOutwardNormal(SurfaceCenter, OriginCellInfo) * FMath::Max(0.0f, HeightOffset));
		return true;
	}

	if (!FootprintCellIds || FootprintCellIds->IsEmpty())
	{
		return false;
	}

	FVector CenterSum = FVector::ZeroVector;
	FVector NormalSum = FVector::ZeroVector;
	int32 ValidCellCount = 0;
	for (const FSRPlanetSurfaceGridCellId& CellId : *FootprintCellIds)
	{
		FSRPlanetSurfaceGridCellInfo CellInfo;
		if (!SurfaceGrid->GetCellInfoById(CellId, CellInfo))
		{
			continue;
		}

		CenterSum += CellInfo.WorldCenter;
		NormalSum += ResolveCellOutwardNormal(SurfaceCenter, CellInfo);
		++ValidCellCount;
	}

	if (ValidCellCount <= 0)
	{
		return false;
	}

	FVector DockingNormal = NormalSum.GetSafeNormal();
	if (DockingNormal.IsNearlyZero())
	{
		DockingNormal = FVector::UpVector;
	}

	OutWorldLocation = (CenterSum / static_cast<float>(ValidCellCount))
		+ (DockingNormal * FMath::Max(0.0f, HeightOffset));
	return true;
}

bool FSRSpaceLogisticsHubEndpointResolver::ResolveSurfaceWorldLocation(
	const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
	float FallbackHeightOffset,
	FVector& OutWorldLocation)
{
	if (ResolveWorldLocationWithHeightOffset(HubEndpoint, 0.0f, OutWorldLocation))
	{
		return true;
	}

	return ResolveWorldLocationWithHeightOffset(HubEndpoint, FallbackHeightOffset, OutWorldLocation);
}

FString FSRSpaceLogisticsHubEndpointResolver::BuildMotionKey(const FSRSpaceLogisticsHubEndpoint& HubEndpoint)
{
	return FString::Printf(
		TEXT("%s|%s"),
		*GetPathNameSafe(HubEndpoint.BodyActor.Get()),
		*HubEndpoint.HubOccupantId.ToString());
}
