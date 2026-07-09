#include "Celestial/SRCelestialBodyDynamicMeshPipeline.h"

#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace StarRovers::Celestial::DynamicMesh
{
namespace
{
	UDynamicMeshComponent* ResolveDynamicMeshFaceComponent(
		UDynamicMeshComponent* PrimaryDynamicMeshComponent,
		const TArray<TObjectPtr<UDynamicMeshComponent>>& FaceDynamicMeshComponents,
		int32 FaceIndex)
	{
		if (FaceDynamicMeshComponents.IsValidIndex(FaceIndex) && IsValid(FaceDynamicMeshComponents[FaceIndex]))
		{
			return FaceDynamicMeshComponents[FaceIndex];
		}

		return FaceIndex == 0 ? PrimaryDynamicMeshComponent : nullptr;
	}

	void InitializeEmptyDynamicMeshFace(UE::Geometry::FDynamicMesh3& Mesh)
	{
		Mesh.EnableAttributes();
		Mesh.Attributes()->EnablePrimaryColors();
		Mesh.Attributes()->SetNumUVLayers(2);
	}

	UE::Geometry::FDynamicMesh3 MakeEmptySurfaceGridMesh()
	{
		UE::Geometry::FDynamicMesh3 GridMesh;
		GridMesh.EnableAttributes();
		GridMesh.Attributes()->EnablePrimaryColors();
		return GridMesh;
	}
}

double ApplyPreparedDynamicMeshFaceMeshes(
	UDynamicMeshComponent* PrimaryDynamicMeshComponent,
	const TArray<TObjectPtr<UDynamicMeshComponent>>& FaceDynamicMeshComponents,
	TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes)
{
	const double StageStart = GetDynamicMeshTimingSeconds();
	for (int32 FaceIndex = 0; FaceIndex < FaceDynamicMeshes.Num(); ++FaceIndex)
	{
		if (UDynamicMeshComponent* FaceDynamicMeshComponent = ResolveDynamicMeshFaceComponent(
			PrimaryDynamicMeshComponent,
			FaceDynamicMeshComponents,
			FaceIndex))
		{
			FaceDynamicMeshComponent->SetMesh(MoveTemp(FaceDynamicMeshes[FaceIndex]));
		}
	}
	return GetDynamicMeshTimingElapsedMilliseconds(StageStart);
}

double ApplyCachedDynamicMeshFaceMeshes(
	UDynamicMeshComponent* PrimaryDynamicMeshComponent,
	const TArray<TObjectPtr<UDynamicMeshComponent>>& FaceDynamicMeshComponents,
	const TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes)
{
	const double StageStart = GetDynamicMeshTimingSeconds();
	for (int32 FaceIndex = 0; FaceIndex < CubeSphereFaceComponentCount; ++FaceIndex)
	{
		if (UDynamicMeshComponent* FaceDynamicMeshComponent = ResolveDynamicMeshFaceComponent(
			PrimaryDynamicMeshComponent,
			FaceDynamicMeshComponents,
			FaceIndex))
		{
			UE::Geometry::FDynamicMesh3 MeshCopy;
			if (FaceDynamicMeshes.IsValidIndex(FaceIndex))
			{
				MeshCopy = FaceDynamicMeshes[FaceIndex];
			}
			else
			{
				InitializeEmptyDynamicMeshFace(MeshCopy);
			}
			FaceDynamicMeshComponent->SetMesh(MoveTemp(MeshCopy));
		}
	}
	return GetDynamicMeshTimingElapsedMilliseconds(StageStart);
}

double ApplyPreparedDynamicMeshSurfaceGridBuild(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCell>& SurfaceGridCells,
	TArray<int32>&& CellIndexByFlatId)
{
	if (!IsValid(SurfaceGrid))
	{
		return 0.0;
	}

	const double StageStart = GetDynamicMeshTimingSeconds();
	TArray<FSRPlanetSurfaceGridCell> GeneratedGridCells = SurfaceGridCells;
	SurfaceGrid->ApplyGeneratedGridBuild(
		MoveTemp(GeneratedGridCells),
		MakeEmptySurfaceGridMesh(),
		MoveTemp(CellIndexByFlatId));
	return GetDynamicMeshTimingElapsedMilliseconds(StageStart);
}

double ApplyCachedDynamicMeshSurfaceGridBuild(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCell>& SurfaceGridCells)
{
	if (!IsValid(SurfaceGrid) || SurfaceGridCells.IsEmpty())
	{
		return 0.0;
	}

	const double StageStart = GetDynamicMeshTimingSeconds();
	TArray<FSRPlanetSurfaceGridCell> GeneratedGridCells = SurfaceGridCells;
	SurfaceGrid->ApplyGeneratedGridBuild(
		MoveTemp(GeneratedGridCells),
		MakeEmptySurfaceGridMesh(),
		TMap<FSRPlanetSurfaceGridCellId, int32>());
	return GetDynamicMeshTimingElapsedMilliseconds(StageStart);
}
}
