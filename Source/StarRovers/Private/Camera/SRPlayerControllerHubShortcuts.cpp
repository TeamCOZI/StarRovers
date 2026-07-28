#include "Camera/SRPlayerController.h"

#include "Assembly/SRAssemblySurfaceFocusInfoBuilder.h"
#include "Automation/SRFacilityNetworkComponent.h"
#include "Camera/SRCameraPawn.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "UI/SRFocusedHubShortcutWidget.h"

namespace
{
	FText ResolveHubDisplayName(const FSRFacilityInstance& FacilityInstance, FName OccupantId)
	{
		if (IsValid(FacilityInstance.StructureDataAsset.Get()))
		{
			const FSRStructureData StructureData = FacilityInstance.StructureDataAsset->BuildData();
			if (!StructureData.DisplayName.IsEmpty())
			{
				return StructureData.DisplayName;
			}
			if (!StructureData.StructureId.IsNone())
			{
				return FText::FromName(StructureData.StructureId);
			}
		}

		return FText::FromName(OccupantId);
	}

	bool CompareHubShortcutInfos(const FSRFocusedHubShortcutInfo& Left, const FSRFocusedHubShortcutInfo& Right)
	{
		const int32 LeftFace = static_cast<int32>(Left.OriginCellId.Face);
		const int32 RightFace = static_cast<int32>(Right.OriginCellId.Face);
		if (LeftFace != RightFace)
		{
			return LeftFace < RightFace;
		}
		if (Left.OriginCellId.CellY != Right.OriginCellId.CellY)
		{
			return Left.OriginCellId.CellY < Right.OriginCellId.CellY;
		}
		if (Left.OriginCellId.CellX != Right.OriginCellId.CellX)
		{
			return Left.OriginCellId.CellX < Right.OriginCellId.CellX;
		}

		return Left.OccupantId.LexicalLess(Right.OccupantId);
	}

	void ApplySelectedStructureSurfacePreview(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRFocusedSurfaceStructureInfo& StructureInfo)
	{
		if (!IsValid(SurfaceGrid) || !StructureInfo.bIsValid)
		{
			return;
		}

		TArray<FSRPlanetSurfaceGridCellId> InputConnectionCellIds;
		TArray<FSRPlanetSurfaceGridCellId> OutputConnectionCellIds;
		StarRovers::Assembly::FSRAssemblySurfaceFocusInfoBuilder::GatherFacilityPortPreviewCells(
			StructureInfo.FacilityPorts,
			InputConnectionCellIds,
			OutputConnectionCellIds);

		SurfaceGrid->SetSelectedFootprintCells(StructureInfo.FootprintCellIds);
		SurfaceGrid->SetOccupiedPreviewCells(StructureInfo.FootprintCellIds);
		SurfaceGrid->SetFacilityPortPreviewCells(InputConnectionCellIds, OutputConnectionCellIds);
	}
}

void ASRPlayerController::RefreshFocusedHubShortcutWidget(bool bForceRefresh)
{
	if (!FocusedHubShortcutWidget)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const double CurrentTime = World ? World->GetTimeSeconds() : 0.0;
	const float SafeRefreshInterval = FMath::Max(0.0f, FocusedHubShortcutRefreshInterval);
	if (!bForceRefresh && SafeRefreshInterval > 0.0f && CurrentTime < NextFocusedHubShortcutRefreshTime)
	{
		return;
	}

	NextFocusedHubShortcutRefreshTime = CurrentTime + SafeRefreshInterval;

	TArray<FSRFocusedHubShortcutInfo> HubInfos;
	BuildFocusedHubShortcutInfos(HubInfos);
	if (HubInfos.IsEmpty())
	{
		FocusedHubShortcutWidget->ClearHubShortcuts();
		return;
	}

	FocusedHubShortcutWidget->SetHubShortcuts(HubInfos);
}

void ASRPlayerController::BuildFocusedHubShortcutInfos(TArray<FSRFocusedHubShortcutInfo>& OutHubInfos) const
{
	OutHubInfos.Reset();

	const ASRCameraPawn* CameraPawn = Cast<ASRCameraPawn>(GetPawn());
	AActor* FocusedActor = IsValid(CameraPawn) ? CameraPawn->GetFocusedActor() : SelectedActor.Get();
	if (!IsValid(FocusedActor) || !USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(FocusedActor))
	{
		return;
	}

	USRFacilityNetworkComponent* FacilityNetwork = FocusedActor->FindComponentByClass<USRFacilityNetworkComponent>();
	USRPlanetSurfaceGrid* SurfaceGrid = FocusedActor->FindComponentByClass<USRPlanetSurfaceGrid>();
	if (!IsValid(FacilityNetwork) || !IsValid(SurfaceGrid))
	{
		return;
	}

	TArray<FName> OccupantIds;
	FacilityNetwork->GetRegisteredFacilityOccupantIds(OccupantIds);
	OutHubInfos.Reserve(OccupantIds.Num());
	for (const FName OccupantId : OccupantIds)
	{
		if (OccupantId.IsNone() || !FacilityNetwork->IsHubFacility(OccupantId))
		{
			continue;
		}

		FSRFacilityInstance FacilityInstance;
		if (!FacilityNetwork->GetFacilityInstance(OccupantId, FacilityInstance))
		{
			continue;
		}

		FSRFocusedHubShortcutInfo HubInfo;
		HubInfo.BodyActor = FocusedActor;
		HubInfo.OccupantId = OccupantId;
		HubInfo.DisplayName = ResolveHubDisplayName(FacilityInstance, OccupantId);
		HubInfo.OriginCellId = FacilityInstance.OriginCellId;
		SurfaceGrid->GetCellInfoById(HubInfo.OriginCellId, HubInfo.OriginCellInfo);
		OutHubInfos.Add(HubInfo);
	}

	OutHubInfos.Sort(CompareHubShortcutInfos);
}

void ASRPlayerController::HandleFocusedHubShortcutRequested(const FSRFocusedHubShortcutInfo& HubInfo)
{
	AActor* BodyActor = HubInfo.BodyActor.Get();
	if (!IsValid(BodyActor) || HubInfo.OccupantId.IsNone())
	{
		return;
	}
	RequestFacilityFocus(BodyActor, HubInfo.OccupantId, true);
}

bool ASRPlayerController::RequestFacilityFocus(
	AActor* BodyActor,
	FName OccupantId,
	bool bCenterSurface)
{
	if (!IsValid(BodyActor) || OccupantId.IsNone())
	{
		return false;
	}

	USRFacilityNetworkComponent* FacilityNetwork =
		BodyActor->FindComponentByClass<USRFacilityNetworkComponent>();
	FSRFacilityInstance FacilityInstance;
	if (!IsValid(FacilityNetwork)
		|| !FacilityNetwork->GetFacilityInstance(OccupantId, FacilityInstance))
	{
		return false;
	}
	return RequestSurfaceStructureFocus(BodyActor, OccupantId, bCenterSurface);
}

bool ASRPlayerController::RequestSurfaceStructureFocus(
	AActor* BodyActor,
	FName OccupantId,
	bool bCenterSurface)
{
	if (!IsValid(BodyActor) || OccupantId.IsNone())
	{
		return false;
	}

	USRPlanetSurfaceGrid* SurfaceGrid =
		BodyActor->FindComponentByClass<USRPlanetSurfaceGrid>();
	USRStructureInstanceManagerComponent* StructureManager =
		BodyActor->FindComponentByClass<USRStructureInstanceManagerComponent>();
	FSRPlacedStructureInstance PlacedStructure;
	if (!IsValid(SurfaceGrid)
		|| !IsValid(StructureManager)
		|| !StructureManager->GetPlacedStructure(OccupantId, PlacedStructure))
	{
		return false;
	}

	RuntimeState.bPendingInitialPrimaryStarFocus = false;
	// Reapplying a surface target after entering Assembly mode must not rebuild
	// the selected actor's focus info. That rebuild intentionally clears the
	// selected surface structure and would make a direct command lose its
	// deposit marker as soon as the Build Dock selects an option.
	if (SelectedActor != BodyActor)
	{
		RequestFocusActor(BodyActor, false);
	}

	FSRPlanetSurfaceGridCell OriginCell;
	if (!SurfaceGrid->GetCellById(PlacedStructure.OriginCellId, OriginCell))
	{
		return false;
	}

	FSRFocusedSurfaceStructureInfo StructureInfo;
	if (!StarRovers::Assembly::FSRAssemblySurfaceFocusInfoBuilder::TryBuildSelectedStructureInfo(
		BodyActor,
		SurfaceGrid,
		OriginCell,
		StructureInfo))
	{
		return false;
	}

	ApplySelectedStructureSurfacePreview(SurfaceGrid, StructureInfo);
	SetSelectedActorSurfaceStructureInfo(BodyActor, StructureInfo);

	FSRPlanetSurfaceGridCellInfo OriginCellInfo;
	if (!SurfaceGrid->GetCellInfoById(PlacedStructure.OriginCellId, OriginCellInfo))
	{
		return false;
	}

	if (bCenterSurface)
	{
		if (ASRCameraPawn* CameraPawn = Cast<ASRCameraPawn>(GetPawn()))
		{
			const FVector BodyCenter = BodyActor->GetActorLocation();
			const FVector SurfaceDirection = OriginCellInfo.WorldCenter - BodyCenter;
			const float SurfaceRadius = SurfaceDirection.Size();
			const FVector ActorLocalSurfaceDirection = BodyActor->GetActorTransform()
				.InverseTransformVectorNoScale(SurfaceDirection)
				.GetSafeNormal();
			if (!ActorLocalSurfaceDirection.IsNearlyZero() && SurfaceRadius > KINDA_SMALL_NUMBER)
			{
				CameraPawn->CenterFocusedSurfaceActorLocalDirection(
					ActorLocalSurfaceDirection,
					SurfaceRadius,
					false);
			}
		}
	}
	return true;
}
