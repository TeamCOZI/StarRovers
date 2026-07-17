#include "Camera/SRPlayerController.h"

#include "Assembly/SRAssemblySurfaceFocusInfoBuilder.h"
#include "Automation/SRFacilityNetworkComponent.h"
#include "Camera/SRCameraPawn.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Structure/SRStructureDataAsset.h"
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

	void ApplySelectedHubSurfacePreview(
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

	RuntimeState.bPendingInitialPrimaryStarFocus = false;
	RequestFocusActor(BodyActor, false);

	USRPlanetSurfaceGrid* SurfaceGrid = BodyActor->FindComponentByClass<USRPlanetSurfaceGrid>();
	if (!IsValid(SurfaceGrid))
	{
		return;
	}

	FSRPlanetSurfaceGridCell OriginCell;
	if (!SurfaceGrid->GetCellById(HubInfo.OriginCellId, OriginCell))
	{
		return;
	}

	FSRFocusedSurfaceStructureInfo StructureInfo;
	if (!StarRovers::Assembly::FSRAssemblySurfaceFocusInfoBuilder::TryBuildSelectedStructureInfo(
		BodyActor,
		SurfaceGrid,
		OriginCell,
		StructureInfo))
	{
		return;
	}

	ApplySelectedHubSurfacePreview(SurfaceGrid, StructureInfo);
	SetSelectedActorSurfaceStructureInfo(BodyActor, StructureInfo);

	FSRPlanetSurfaceGridCellInfo OriginCellInfo;
	if (!SurfaceGrid->GetCellInfoById(HubInfo.OriginCellId, OriginCellInfo))
	{
		return;
	}

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
			CameraPawn->CenterFocusedSurfaceActorLocalDirection(ActorLocalSurfaceDirection, SurfaceRadius, false);
		}
	}
}
