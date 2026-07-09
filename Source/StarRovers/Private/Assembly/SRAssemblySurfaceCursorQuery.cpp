#include "Assembly/SRAssemblySurfaceCursorQuery.h"

#include "Camera/SRCameraPawn.h"
#include "Camera/SRPlayerController.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "GameFramework/Actor.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace StarRovers::Assembly
{
	bool FSRAssemblySurfaceCursorQuery::TryGetCursorRay(const ASRPlayerController* PlayerController, FVector& OutRayOrigin, FVector& OutRayDirection)
	{
		OutRayOrigin = FVector::ZeroVector;
		OutRayDirection = FVector::ZeroVector;

		return PlayerController
			&& PlayerController->DeprojectMousePositionToWorld(OutRayOrigin, OutRayDirection)
			&& !OutRayDirection.IsNearlyZero();
	}

	bool FSRAssemblySurfaceCursorQuery::TryGetFocusedSurfaceGrid(
		const ASRPlayerController* PlayerController,
		AActor*& OutFocusedActor,
		USRPlanetSurfaceGrid*& OutSurfaceGrid)
	{
		OutFocusedActor = nullptr;
		OutSurfaceGrid = nullptr;

		const ASRCameraPawn* CameraPawn = PlayerController ? Cast<ASRCameraPawn>(PlayerController->GetPawn()) : nullptr;
		if (!CameraPawn)
		{
			return false;
		}

		AActor* FocusedActor = CameraPawn->GetFocusedActor();
		if (!IsValid(FocusedActor))
		{
			return false;
		}

		USRPlanetSurfaceGrid* SurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(FocusedActor);
		if (!IsValid(SurfaceGrid))
		{
			return false;
		}

		OutFocusedActor = FocusedActor;
		OutSurfaceGrid = SurfaceGrid;
		return true;
	}

	bool FSRAssemblySurfaceCursorQuery::TryProjectCursorToSurfaceCell(
		const ASRPlayerController* PlayerController,
		USRPlanetSurfaceGrid* SurfaceGrid,
		FSRPlanetSurfaceGridCell& OutCell,
		FVector& OutHitLocation)
	{
		OutCell = FSRPlanetSurfaceGridCell();
		OutHitLocation = FVector::ZeroVector;

		if (!IsValid(SurfaceGrid))
		{
			return false;
		}

		if (AActor* OwnerActor = SurfaceGrid->GetOwner())
		{
			OwnerActor->UpdateComponentTransforms();
		}
		SurfaceGrid->UpdateComponentToWorld();

		FVector RayOrigin = FVector::ZeroVector;
		FVector RayDirection = FVector::ZeroVector;
		if (!TryGetCursorRay(PlayerController, RayOrigin, RayDirection))
		{
			return false;
		}

		return SurfaceGrid->RaycastCell(RayOrigin, RayDirection, OutCell, OutHitLocation);
	}

	bool FSRAssemblySurfaceCursorQuery::TryResolveSurfaceCell(const ASRPlayerController* PlayerController, FSRAssemblySurfaceCursorTarget& OutTarget)
	{
		OutTarget = FSRAssemblySurfaceCursorTarget();

		AActor* FocusedActor = nullptr;
		USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
		if (!TryGetFocusedSurfaceGrid(PlayerController, FocusedActor, SurfaceGrid))
		{
			return false;
		}

		FSRPlanetSurfaceGridCell TargetCell;
		FVector HitLocation = FVector::ZeroVector;
		if (!TryProjectCursorToSurfaceCell(PlayerController, SurfaceGrid, TargetCell, HitLocation))
		{
			return false;
		}

		OutTarget.FocusedActor = FocusedActor;
		OutTarget.SurfaceGrid = SurfaceGrid;
		OutTarget.Cell = TargetCell;
		OutTarget.HitLocation = HitLocation;
		return true;
	}
}
