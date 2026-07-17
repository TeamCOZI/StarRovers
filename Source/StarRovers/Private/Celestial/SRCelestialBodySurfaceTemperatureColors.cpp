#include "Celestial/SRCelestialBody.h"

#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "UDynamicMesh.h"

namespace
{
	uint64 BuildDynamicMeshColorElementKey(int32 MeshComponentIndex, int32 ElementId)
	{
		return (static_cast<uint64>(static_cast<uint32>(MeshComponentIndex)) << 32)
			| static_cast<uint64>(static_cast<uint32>(ElementId));
	}

	FVector4f ToDynamicMeshVectorColor(const FLinearColor& Color)
	{
		return FVector4f(Color.R, Color.G, Color.B, Color.A);
	}

	int32 GetFlatCellIndex(const FSRPlanetSurfaceGridCellId& CellId, int32 FaceResolution)
	{
		return ((static_cast<int32>(CellId.Face) * FaceResolution) + CellId.CellY) * FaceResolution + CellId.CellX;
	}

	template <typename ElementArrayType>
	void AddTemperatureStateColorTargets(
		ElementArrayType& Elements,
		const FSRDynamicMeshGeneration& DynamicMeshGeneration,
		ESRFacilityTemperatureState TemperatureState,
		TSet<uint64>& HighlightedColorElements,
		TMap<uint64, FLinearColor>& HighlightedBaseColorByElement,
		TMap<int32, TMap<int32, FLinearColor>>& OutTargetColorsByMesh)
	{
		for (FSRCelestialBodyDynamicMeshColorElement& Element : Elements)
		{
			if (Element.MeshComponentIndex == INDEX_NONE || Element.ElementId == INDEX_NONE)
			{
				continue;
			}

			const FLinearColor NewBaseColor = DynamicMeshGeneration.ApplyTemperatureStateSurfaceColor(
				Element.TerrainBaseColor,
				TemperatureState);
			Element.BaseColor = NewBaseColor;
			OutTargetColorsByMesh.FindOrAdd(Element.MeshComponentIndex).Add(Element.ElementId, NewBaseColor);

			const uint64 ElementKey = BuildDynamicMeshColorElementKey(Element.MeshComponentIndex, Element.ElementId);
			if (HighlightedColorElements.Contains(ElementKey))
			{
				HighlightedBaseColorByElement.Add(ElementKey, NewBaseColor);
			}
		}
	}

	void ApplyTemperatureStateColorsToMesh(
		UE::Geometry::FDynamicMesh3& Mesh,
		const TMap<int32, FLinearColor>& TargetColors)
	{
		if (!Mesh.HasAttributes())
		{
			return;
		}

		auto* ColorOverlay = Mesh.Attributes()->PrimaryColors();
		if (!ColorOverlay)
		{
			return;
		}

		for (const TPair<int32, FLinearColor>& TargetColorPair : TargetColors)
		{
			ColorOverlay->SetElement(TargetColorPair.Key, ToDynamicMeshVectorColor(TargetColorPair.Value));
		}
	}
}

bool ASRCelestialBody::ApplySurfaceTemperatureStateColor(
	const FSRPlanetSurfaceGridCellId& CellId,
	ESRFacilityTemperatureState TemperatureState)
{
	if (!IsValid(DynamicMeshBaseDataAsset.Get()) || DynamicMeshState.ColorDataByFlatId.IsEmpty())
	{
		return false;
	}

	const int32 FaceResolution = DynamicMeshBaseDataAsset->GetClampedFaceResolution();
	if (!CellId.IsValid(FaceResolution))
	{
		return false;
	}

	const int32 FlatIndex = GetFlatCellIndex(CellId, FaceResolution);
	if (!DynamicMeshState.ColorDataByFlatId.IsValidIndex(FlatIndex))
	{
		return false;
	}

	for (FSRPlanetSurfaceGridCell& SurfaceGridCell : DynamicMeshState.SurfaceGridCells)
	{
		if (SurfaceGridCell.CellId == CellId)
		{
			SurfaceGridCell.TemperatureState = TemperatureState;
			break;
		}
	}

	FSRCelestialBodyDynamicMeshCellColorData& CellColorData = DynamicMeshState.ColorDataByFlatId[FlatIndex];
	TMap<int32, TMap<int32, FLinearColor>> TargetColorsByMesh;
	AddTemperatureStateColorTargets(
		CellColorData.SurfaceColorElements,
		DynamicMeshGeneration,
		TemperatureState,
		DynamicMeshState.HighlightedColorElements,
		DynamicMeshState.HighlightedBaseColorByElement,
		TargetColorsByMesh);
	AddTemperatureStateColorTargets(
		CellColorData.SideColorElements,
		DynamicMeshGeneration,
		TemperatureState,
		DynamicMeshState.HighlightedColorElements,
		DynamicMeshState.HighlightedBaseColorByElement,
		TargetColorsByMesh);

	if (TargetColorsByMesh.IsEmpty())
	{
		return false;
	}

	for (const TPair<int32, TMap<int32, FLinearColor>>& MeshTargetPair : TargetColorsByMesh)
	{
		UDynamicMeshComponent* DynamicMeshComponent = GetDynamicMeshFaceComponent(MeshTargetPair.Key);
		if (!IsValid(DynamicMeshComponent))
		{
			continue;
		}

		UDynamicMesh* DynamicMeshObject = DynamicMeshComponent->GetDynamicMesh();
		if (!IsValid(DynamicMeshObject))
		{
			continue;
		}

		const TMap<int32, FLinearColor>& MeshTargetColors = MeshTargetPair.Value;
		DynamicMeshObject->EditMesh(
			[&MeshTargetColors](UE::Geometry::FDynamicMesh3& Mesh)
			{
				ApplyTemperatureStateColorsToMesh(Mesh, MeshTargetColors);
			},
			EDynamicMeshChangeType::DeformationEdit,
			EDynamicMeshAttributeChangeFlags::VertexColors,
			false);
	}

	return true;
}
