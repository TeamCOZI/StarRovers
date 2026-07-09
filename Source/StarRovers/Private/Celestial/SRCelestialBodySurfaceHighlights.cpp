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

	FLinearColor BlendSurfaceHighlightColor(const FLinearColor& BaseColor, const FLinearColor& HighlightColor)
	{
		constexpr float HighlightIntensity = 0.45f;
		return FLinearColor(
			FMath::Clamp(BaseColor.R + (HighlightColor.R * HighlightIntensity), 0.0f, 1.0f),
			FMath::Clamp(BaseColor.G + (HighlightColor.G * HighlightIntensity), 0.0f, 1.0f),
			FMath::Clamp(BaseColor.B + (HighlightColor.B * HighlightIntensity), 0.0f, 1.0f),
			BaseColor.A);
	}

	struct FSRCelestialBodySurfaceHighlightTargets
	{
		TMap<uint64, FLinearColor> TargetColorsByElement;
		TMap<uint64, FLinearColor> BaseColorByElement;
		TSet<uint64> HighlightedElements;
		TMap<int32, TMap<int32, FLinearColor>> TargetColorsByMesh;
		TMap<int32, TSet<int32>> HighlightedElementsByMesh;
		TSet<int32> MeshIndicesToEdit;
	};

	template <typename ElementArrayType>
	void AddSurfaceHighlightColorElements(
		const ElementArrayType& Elements,
		const FLinearColor& HighlightColor,
		FSRCelestialBodySurfaceHighlightTargets& Targets)
	{
		for (const FSRCelestialBodyDynamicMeshColorElement& Element : Elements)
		{
			if (Element.MeshComponentIndex == INDEX_NONE || Element.ElementId == INDEX_NONE)
			{
				continue;
			}

			const uint64 ElementKey = BuildDynamicMeshColorElementKey(Element.MeshComponentIndex, Element.ElementId);
			Targets.TargetColorsByElement.Add(ElementKey, BlendSurfaceHighlightColor(Element.BaseColor, HighlightColor));
			Targets.BaseColorByElement.Add(ElementKey, Element.BaseColor);
		}
	}

	void AddSurfaceHighlightCell(
		const FSRCelestialBodyDynamicMeshCellColorData* CellColorData,
		const FLinearColor& HighlightColor,
		FSRCelestialBodySurfaceHighlightTargets& Targets)
	{
		if (!CellColorData)
		{
			return;
		}

		AddSurfaceHighlightColorElements(CellColorData->SurfaceColorElements, HighlightColor, Targets);
		AddSurfaceHighlightColorElements(CellColorData->SideColorElements, HighlightColor, Targets);
	}

	bool HasSurfaceHighlightElementChange(
		const TSet<uint64>& PreviousHighlightedElements,
		const TSet<uint64>& NextHighlightedElements)
	{
		for (const uint64 PreviousElementKey : PreviousHighlightedElements)
		{
			if (!NextHighlightedElements.Contains(PreviousElementKey))
			{
				return true;
			}
		}
		for (const uint64 NextElementKey : NextHighlightedElements)
		{
			if (!PreviousHighlightedElements.Contains(NextElementKey))
			{
				return true;
			}
		}
		return false;
	}

	void FinalizeSurfaceHighlightTargets(
		const TSet<uint64>& PreviousHighlightedElements,
		FSRCelestialBodySurfaceHighlightTargets& Targets)
	{
		for (const TPair<uint64, FLinearColor>& TargetColorPair : Targets.TargetColorsByElement)
		{
			Targets.HighlightedElements.Add(TargetColorPair.Key);

			const int32 MeshComponentIndex = static_cast<int32>(TargetColorPair.Key >> 32);
			const int32 ElementId = static_cast<int32>(TargetColorPair.Key & 0xffffffff);
			Targets.TargetColorsByMesh.FindOrAdd(MeshComponentIndex).Add(ElementId, TargetColorPair.Value);
		}

		for (const uint64 NextElementKey : Targets.HighlightedElements)
		{
			const int32 MeshComponentIndex = static_cast<int32>(NextElementKey >> 32);
			const int32 ElementId = static_cast<int32>(NextElementKey & 0xffffffff);
			Targets.HighlightedElementsByMesh.FindOrAdd(MeshComponentIndex).Add(ElementId);
		}

		for (const uint64 PreviousElementKey : PreviousHighlightedElements)
		{
			Targets.MeshIndicesToEdit.Add(static_cast<int32>(PreviousElementKey >> 32));
		}
		for (const uint64 NextElementKey : Targets.HighlightedElements)
		{
			Targets.MeshIndicesToEdit.Add(static_cast<int32>(NextElementKey >> 32));
		}
	}

	void ApplySurfaceHighlightColorsToMesh(
		UE::Geometry::FDynamicMesh3& Mesh,
		int32 MeshComponentIndex,
		const TSet<uint64>& PreviousHighlightedElements,
		const TMap<uint64, FLinearColor>& PreviousBaseColorByElement,
		const TMap<int32, FLinearColor>* MeshTargetColors,
		const TSet<int32>* MeshNextHighlightedElements)
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

		for (const uint64 PreviousElementKey : PreviousHighlightedElements)
		{
			if (static_cast<int32>(PreviousElementKey >> 32) != MeshComponentIndex)
			{
				continue;
			}

			const int32 PreviousElementId = static_cast<int32>(PreviousElementKey & 0xffffffff);
			if (MeshNextHighlightedElements && MeshNextHighlightedElements->Contains(PreviousElementId))
			{
				continue;
			}

			if (const FLinearColor* BaseColor = PreviousBaseColorByElement.Find(PreviousElementKey))
			{
				ColorOverlay->SetElement(PreviousElementId, ToDynamicMeshVectorColor(*BaseColor));
			}
		}

		if (MeshTargetColors)
		{
			for (const TPair<int32, FLinearColor>& TargetColorPair : *MeshTargetColors)
			{
				ColorOverlay->SetElement(TargetColorPair.Key, ToDynamicMeshVectorColor(TargetColorPair.Value));
			}
		}
	}

	TMap<int32, TArray<uint64>> GroupSurfaceHighlightElementsByMesh(const TSet<uint64>& HighlightedElements)
	{
		TMap<int32, TArray<uint64>> HighlightedElementsByMesh;
		for (const uint64 ElementKey : HighlightedElements)
		{
			HighlightedElementsByMesh.FindOrAdd(static_cast<int32>(ElementKey >> 32)).Add(ElementKey);
		}
		return HighlightedElementsByMesh;
	}

	void RestoreSurfaceHighlightColorsOnMesh(
		UE::Geometry::FDynamicMesh3& Mesh,
		const TArray<uint64>& HighlightedElements,
		const TMap<uint64, FLinearColor>& BaseColorByElement)
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

		for (const uint64 ElementKey : HighlightedElements)
		{
			const int32 ElementId = static_cast<int32>(ElementKey & 0xffffffff);
			if (const FLinearColor* BaseColor = BaseColorByElement.Find(ElementKey))
			{
				ColorOverlay->SetElement(ElementId, ToDynamicMeshVectorColor(*BaseColor));
			}
		}
	}
}

bool ASRCelestialBody::HasSurfaceCellRenderData(const FSRPlanetSurfaceGridCellId& CellId) const
{
	return FindDynamicMeshCellColorData(CellId) != nullptr;
}

const FSRCelestialBodyDynamicMeshCellColorData* ASRCelestialBody::FindDynamicMeshCellColorData(const FSRPlanetSurfaceGridCellId& CellId) const
{
	if (!IsValid(DynamicMeshBaseDataAsset.Get()))
	{
		return nullptr;
	}

	const int32 FaceResolution = DynamicMeshBaseDataAsset->GetClampedFaceResolution();
	return DynamicMeshState.FindCellColorData(CellId, FaceResolution);
}

bool ASRCelestialBody::GetCachedSurfaceGridCells(TArray<FSRPlanetSurfaceGridCell>& OutCells) const
{
	return DynamicMeshState.GetSurfaceGridCells(OutCells);
}

bool ASRCelestialBody::ApplySurfaceCellHighlights(
	const FSRPlanetSurfaceGridCellId& HoveredCellId,
	bool bHasHoveredCell,
	const FSRPlanetSurfaceGridCellId& SelectedCellId,
	bool bHasSelectedCell,
	const FLinearColor& HoveredCellColor,
	const FLinearColor& SelectedCellColor)
{
	if (!IsValid(CelestialBodyDynamicMesh.Get()) || DynamicMeshState.ColorDataByFlatId.IsEmpty())
	{
		return false;
	}

	FSRCelestialBodySurfaceHighlightTargets HighlightTargets;

	if (bHasHoveredCell)
	{
		AddSurfaceHighlightCell(FindDynamicMeshCellColorData(HoveredCellId), HoveredCellColor, HighlightTargets);
	}
	if (bHasSelectedCell)
	{
		AddSurfaceHighlightCell(FindDynamicMeshCellColorData(SelectedCellId), SelectedCellColor, HighlightTargets);
	}

	FinalizeSurfaceHighlightTargets(DynamicMeshState.HighlightedColorElements, HighlightTargets);
	const bool bHasAnyColorChange = HasSurfaceHighlightElementChange(
		DynamicMeshState.HighlightedColorElements,
		HighlightTargets.HighlightedElements);
	if (!bHasAnyColorChange && HighlightTargets.HighlightedElements.IsEmpty())
	{
		return true;
	}

	const TSet<uint64>& PreviousHighlightedElements = DynamicMeshState.HighlightedColorElements;
	const TMap<uint64, FLinearColor>& PreviousBaseColorByElement = DynamicMeshState.HighlightedBaseColorByElement;
	for (const int32 MeshComponentIndex : HighlightTargets.MeshIndicesToEdit)
	{
		UDynamicMeshComponent* DynamicMeshComponent = GetDynamicMeshFaceComponent(MeshComponentIndex);
		if (!IsValid(DynamicMeshComponent))
		{
			continue;
		}

		UDynamicMesh* DynamicMeshObject = DynamicMeshComponent->GetDynamicMesh();
		if (!IsValid(DynamicMeshObject))
		{
			continue;
		}

		const TMap<int32, FLinearColor>* MeshTargetColors = HighlightTargets.TargetColorsByMesh.Find(MeshComponentIndex);
		const TSet<int32>* MeshNextHighlightedElements = HighlightTargets.HighlightedElementsByMesh.Find(MeshComponentIndex);
		DynamicMeshObject->EditMesh(
			[MeshComponentIndex, MeshTargetColors, MeshNextHighlightedElements, &PreviousHighlightedElements, &PreviousBaseColorByElement](UE::Geometry::FDynamicMesh3& Mesh)
			{
				ApplySurfaceHighlightColorsToMesh(
					Mesh,
					MeshComponentIndex,
					PreviousHighlightedElements,
					PreviousBaseColorByElement,
					MeshTargetColors,
					MeshNextHighlightedElements);
			},
			EDynamicMeshChangeType::DeformationEdit,
			EDynamicMeshAttributeChangeFlags::VertexColors,
			false);
	}

	DynamicMeshState.HighlightedColorElements = MoveTemp(HighlightTargets.HighlightedElements);
	DynamicMeshState.HighlightedBaseColorByElement = MoveTemp(HighlightTargets.BaseColorByElement);
	return !HighlightTargets.TargetColorsByElement.IsEmpty() || bHasAnyColorChange;
}

void ASRCelestialBody::ClearSurfaceCellHighlights()
{
	if (DynamicMeshState.HighlightedColorElements.IsEmpty())
	{
		DynamicMeshState.HighlightedColorElements.Reset();
		DynamicMeshState.HighlightedBaseColorByElement.Reset();
		return;
	}

	TMap<int32, TArray<uint64>> HighlightedElementsByMesh = GroupSurfaceHighlightElementsByMesh(DynamicMeshState.HighlightedColorElements);
	const TMap<uint64, FLinearColor>& HighlightedBaseColorByElement = DynamicMeshState.HighlightedBaseColorByElement;

	for (const TPair<int32, TArray<uint64>>& HighlightedMeshPair : HighlightedElementsByMesh)
	{
		UDynamicMeshComponent* DynamicMeshComponent = GetDynamicMeshFaceComponent(HighlightedMeshPair.Key);
		if (!IsValid(DynamicMeshComponent))
		{
			continue;
		}

		UDynamicMesh* DynamicMeshObject = DynamicMeshComponent->GetDynamicMesh();
		if (!IsValid(DynamicMeshObject))
		{
			continue;
		}

		const TArray<uint64>& MeshHighlightedElements = HighlightedMeshPair.Value;
		DynamicMeshObject->EditMesh(
			[&MeshHighlightedElements, &HighlightedBaseColorByElement](UE::Geometry::FDynamicMesh3& Mesh)
			{
				RestoreSurfaceHighlightColorsOnMesh(Mesh, MeshHighlightedElements, HighlightedBaseColorByElement);
			},
			EDynamicMeshChangeType::DeformationEdit,
			EDynamicMeshAttributeChangeFlags::VertexColors,
			false);
	}

	DynamicMeshState.HighlightedColorElements.Reset();
	DynamicMeshState.HighlightedBaseColorByElement.Reset();
}
