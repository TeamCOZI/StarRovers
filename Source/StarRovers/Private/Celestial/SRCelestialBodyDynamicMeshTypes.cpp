#include "Celestial/SRCelestialBodyDynamicMeshTypes.h"

void FSRCelestialBodyDynamicMeshRuntimeState::Reset()
{
	ColorDataByFlatId.Reset();
	HighlightedColorElements.Reset();
	HighlightedBaseColorByElement.Reset();
	SurfaceGridCells.Reset();
	BuildHash = 0;
	bHasBuildHash = false;
}

bool FSRCelestialBodyDynamicMeshRuntimeState::HasBuild() const
{
	return bHasBuildHash;
}

bool FSRCelestialBodyDynamicMeshRuntimeState::HasBuildHash(uint32 InBuildHash) const
{
	return bHasBuildHash && BuildHash == InBuildHash;
}

void FSRCelestialBodyDynamicMeshRuntimeState::MarkBuilt(uint32 InBuildHash)
{
	BuildHash = InBuildHash;
	bHasBuildHash = true;
}

bool FSRCelestialBodyDynamicMeshRuntimeState::GetSurfaceGridCells(TArray<FSRPlanetSurfaceGridCell>& OutCells) const
{
	if (!bHasBuildHash || SurfaceGridCells.IsEmpty())
	{
		OutCells.Reset();
		return false;
	}

	OutCells = SurfaceGridCells;
	return true;
}

const FSRCelestialBodyDynamicMeshCellColorData* FSRCelestialBodyDynamicMeshRuntimeState::FindCellColorData(
	const FSRPlanetSurfaceGridCellId& CellId,
	int32 FaceResolution) const
{
	if (!CellId.IsValid(FaceResolution))
	{
		return nullptr;
	}

	const int32 FlatIndex = ((static_cast<int32>(CellId.Face) * FaceResolution) + CellId.CellY) * FaceResolution + CellId.CellX;
	return ColorDataByFlatId.IsValidIndex(FlatIndex) ? &ColorDataByFlatId[FlatIndex] : nullptr;
}
