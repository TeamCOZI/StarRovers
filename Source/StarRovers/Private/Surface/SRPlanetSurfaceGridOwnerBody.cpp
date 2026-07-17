#include "SRPlanetSurfaceGridOwnerBody.h"

#include "Celestial/SRCelestialBody.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "UDynamicMesh.h"

namespace StarRovers::SurfaceGridOwnerBody
{
	void PrepareDynamicMesh(AActor* Owner)
	{
		if (ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(Owner))
		{
			OwnerBody->PrepareCelestialBodyDynamicMesh();
		}
	}

	bool GetCachedSurfaceGridCells(const AActor* Owner, TArray<FSRPlanetSurfaceGridCell>& OutCells)
	{
		const ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(Owner);
		return IsValid(OwnerBody) && OwnerBody->GetCachedSurfaceGridCells(OutCells);
	}

	bool AppendDynamicMeshBoundaryWire(const AActor* Owner, FDynamicMeshBoundarySegmentAppender AppendSegment)
	{
		const ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(Owner);
		if (!IsValid(OwnerBody))
		{
			return false;
		}

		UDynamicMeshComponent* OwnerDynamicMeshComponent = OwnerBody->GetCelestialBodyDynamicMesh();
		UDynamicMesh* OwnerDynamicMeshObject = IsValid(OwnerDynamicMeshComponent)
			? OwnerDynamicMeshComponent->GetDynamicMesh()
			: nullptr;
		if (!IsValid(OwnerDynamicMeshObject))
		{
			return false;
		}

		bool bAppendedAnyEdge = false;
		const FTransform OwnerDynamicMeshRelativeTransform = OwnerDynamicMeshComponent->GetRelativeTransform();
		OwnerDynamicMeshObject->ProcessMesh([&AppendSegment, &bAppendedAnyEdge, &OwnerDynamicMeshRelativeTransform](const UE::Geometry::FDynamicMesh3& OwnerMesh)
		{
			for (const int32 EdgeId : OwnerMesh.EdgeIndicesItr())
			{
				const auto EdgeTriangles = OwnerMesh.GetEdgeT(EdgeId);
				if (EdgeTriangles.A >= 0 && EdgeTriangles.B >= 0)
				{
					continue;
				}

				const auto EdgeVertices = OwnerMesh.GetEdgeV(EdgeId);
				const FVector LocalPointA = OwnerDynamicMeshRelativeTransform.TransformPosition(FVector(OwnerMesh.GetVertex(EdgeVertices.A)));
				const FVector LocalPointB = OwnerDynamicMeshRelativeTransform.TransformPosition(FVector(OwnerMesh.GetVertex(EdgeVertices.B)));
				AppendSegment(LocalPointA, LocalPointB);
				bAppendedAnyEdge = true;
			}
		});

		return bAppendedAnyEdge;
	}

	bool ApplySurfaceCellHighlights(
		AActor* Owner,
		const TArray<FSRPlanetSurfaceGridCellId>& HoveredCellIds,
		const TArray<FSRPlanetSurfaceGridCellId>& SelectedCellIds,
		const TArray<FSRPlanetSurfaceGridCellId>& OccupiedPreviewCellIds,
		const FLinearColor& HoveredCellColor,
		const FLinearColor& SelectedCellColor,
		const FLinearColor& OccupiedCellColor)
	{
		ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(Owner);
		return IsValid(OwnerBody)
			&& OwnerBody->ApplySurfaceCellHighlights(
				HoveredCellIds,
				SelectedCellIds,
				OccupiedPreviewCellIds,
				HoveredCellColor,
				SelectedCellColor,
				OccupiedCellColor);
	}

	void ClearSurfaceCellHighlights(AActor* Owner)
	{
		if (ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(Owner))
		{
			OwnerBody->ClearSurfaceCellHighlights();
		}
	}

	bool ApplySurfaceTemperatureStateColor(
		AActor* Owner,
		const FSRPlanetSurfaceGridCellId& CellId,
		ESRFacilityTemperatureState TemperatureState)
	{
		ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(Owner);
		return IsValid(OwnerBody)
			&& OwnerBody->ApplySurfaceTemperatureStateColor(CellId, TemperatureState);
	}
}
