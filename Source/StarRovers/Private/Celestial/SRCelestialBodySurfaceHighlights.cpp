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
	if (!CellId.IsValid(FaceResolution))
	{
		return nullptr;
	}

	const int32 FlatIndex = ((static_cast<int32>(CellId.Face) * FaceResolution) + CellId.CellY) * FaceResolution + CellId.CellX;
	return DynamicMeshColorDataByFlatId.IsValidIndex(FlatIndex) ? &DynamicMeshColorDataByFlatId[FlatIndex] : nullptr;
}

bool ASRCelestialBody::GetCachedSurfaceGridCells(TArray<FSRPlanetSurfaceGridCell>& OutCells) const
{
	if (!bHasCachedDynamicMeshBuildHash || CachedSurfaceGridCells.IsEmpty())
	{
		OutCells.Reset();
		return false;
	}

	OutCells = CachedSurfaceGridCells;
	return true;
}

bool ASRCelestialBody::ApplySurfaceCellHighlights(
	const FSRPlanetSurfaceGridCellId& HoveredCellId,
	bool bHasHoveredCell,
	const FSRPlanetSurfaceGridCellId& SelectedCellId,
	bool bHasSelectedCell,
	const FLinearColor& HoveredCellColor,
	const FLinearColor& SelectedCellColor)
{
	if (!IsValid(CelestialBodyDynamicMesh.Get()) || DynamicMeshColorDataByFlatId.IsEmpty())
	{
		return false;
	}

	TMap<uint64, FLinearColor> TargetColorsByElement;
	TMap<uint64, FLinearColor> NextHighlightedBaseColorByElement;
	auto BlendHighlightColor = [](const FLinearColor& BaseColor, const FLinearColor& HighlightColor)
	{
		constexpr float HighlightIntensity = 0.45f;
		return FLinearColor(
			FMath::Clamp(BaseColor.R + (HighlightColor.R * HighlightIntensity), 0.0f, 1.0f),
			FMath::Clamp(BaseColor.G + (HighlightColor.G * HighlightIntensity), 0.0f, 1.0f),
			FMath::Clamp(BaseColor.B + (HighlightColor.B * HighlightIntensity), 0.0f, 1.0f),
			BaseColor.A);
	};
	auto AddCellHighlight = [this, &TargetColorsByElement, &NextHighlightedBaseColorByElement, &BlendHighlightColor](const FSRPlanetSurfaceGridCellId& CellId, const FLinearColor& HighlightColor)
	{
		const FSRCelestialBodyDynamicMeshCellColorData* CellColorData = FindDynamicMeshCellColorData(CellId);
		if (!CellColorData)
		{
			return;
		}

		auto AddElements = [&TargetColorsByElement, &NextHighlightedBaseColorByElement, &HighlightColor, &BlendHighlightColor](const auto& Elements)
		{
			for (const FSRCelestialBodyDynamicMeshColorElement& Element : Elements)
			{
				if (Element.MeshComponentIndex != INDEX_NONE && Element.ElementId != INDEX_NONE)
				{
					const uint64 ElementKey = BuildDynamicMeshColorElementKey(Element.MeshComponentIndex, Element.ElementId);
					TargetColorsByElement.Add(
						ElementKey,
						BlendHighlightColor(Element.BaseColor, HighlightColor));
					NextHighlightedBaseColorByElement.Add(ElementKey, Element.BaseColor);
				}
			}
		};

		AddElements(CellColorData->SurfaceColorElements);
		AddElements(CellColorData->SideColorElements);
	};

	if (bHasHoveredCell)
	{
		AddCellHighlight(HoveredCellId, HoveredCellColor);
	}
	if (bHasSelectedCell)
	{
		AddCellHighlight(SelectedCellId, SelectedCellColor);
	}

	TSet<uint64> NextHighlightedElements;
	for (const TPair<uint64, FLinearColor>& TargetColorPair : TargetColorsByElement)
	{
		NextHighlightedElements.Add(TargetColorPair.Key);
	}
	bool bHasAnyColorChange = false;
	for (const uint64 PreviousElementKey : HighlightedDynamicMeshColorElements)
	{
		if (!NextHighlightedElements.Contains(PreviousElementKey))
		{
			bHasAnyColorChange = true;
			break;
		}
	}
	if (!bHasAnyColorChange)
	{
		for (const uint64 NextElementKey : NextHighlightedElements)
		{
			if (!HighlightedDynamicMeshColorElements.Contains(NextElementKey))
			{
				bHasAnyColorChange = true;
				break;
			}
		}
	}
	if (!bHasAnyColorChange && NextHighlightedElements.IsEmpty())
	{
		return true;
	}

	TMap<int32, TMap<int32, FLinearColor>> TargetColorsByMesh;
	for (const TPair<uint64, FLinearColor>& TargetColorPair : TargetColorsByElement)
	{
		const int32 MeshComponentIndex = static_cast<int32>(TargetColorPair.Key >> 32);
		const int32 ElementId = static_cast<int32>(TargetColorPair.Key & 0xffffffff);
		TargetColorsByMesh.FindOrAdd(MeshComponentIndex).Add(ElementId, TargetColorPair.Value);
	}

	TMap<int32, TSet<int32>> NextHighlightedElementsByMesh;
	for (const uint64 NextElementKey : NextHighlightedElements)
	{
		const int32 MeshComponentIndex = static_cast<int32>(NextElementKey >> 32);
		const int32 ElementId = static_cast<int32>(NextElementKey & 0xffffffff);
		NextHighlightedElementsByMesh.FindOrAdd(MeshComponentIndex).Add(ElementId);
	}

	TSet<int32> MeshIndicesToEdit;
	for (const uint64 PreviousElementKey : HighlightedDynamicMeshColorElements)
	{
		MeshIndicesToEdit.Add(static_cast<int32>(PreviousElementKey >> 32));
	}
	for (const uint64 NextElementKey : NextHighlightedElements)
	{
		MeshIndicesToEdit.Add(static_cast<int32>(NextElementKey >> 32));
	}

	for (const int32 MeshComponentIndex : MeshIndicesToEdit)
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

		const TMap<int32, FLinearColor>* MeshTargetColors = TargetColorsByMesh.Find(MeshComponentIndex);
		const TSet<int32>* MeshNextHighlightedElements = NextHighlightedElementsByMesh.Find(MeshComponentIndex);
		DynamicMeshObject->EditMesh(
			[this, MeshComponentIndex, MeshTargetColors, MeshNextHighlightedElements](UE::Geometry::FDynamicMesh3& Mesh)
			{
				if (!Mesh.HasAttributes())
				{
					return;
				}

				auto* ColorOverlay = Mesh.Attributes()->PrimaryColors();
				auto ToVectorColor = [](const FLinearColor& Color)
				{
					return FVector4f(Color.R, Color.G, Color.B, Color.A);
				};

				if (ColorOverlay)
				{
					for (const uint64 PreviousElementKey : HighlightedDynamicMeshColorElements)
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

						if (const FLinearColor* BaseColor = HighlightedDynamicMeshBaseColorByElement.Find(PreviousElementKey))
						{
							ColorOverlay->SetElement(PreviousElementId, ToVectorColor(*BaseColor));
						}
					}

					if (MeshTargetColors)
					{
						for (const TPair<int32, FLinearColor>& TargetColorPair : *MeshTargetColors)
						{
							ColorOverlay->SetElement(TargetColorPair.Key, ToVectorColor(TargetColorPair.Value));
						}
					}
				}
			},
			EDynamicMeshChangeType::DeformationEdit,
			EDynamicMeshAttributeChangeFlags::VertexColors,
			false);
	}

	HighlightedDynamicMeshColorElements = MoveTemp(NextHighlightedElements);
	HighlightedDynamicMeshBaseColorByElement = MoveTemp(NextHighlightedBaseColorByElement);
	return !TargetColorsByElement.IsEmpty() || bHasAnyColorChange;
}

void ASRCelestialBody::ClearSurfaceCellHighlights()
{
	if (HighlightedDynamicMeshColorElements.IsEmpty())
	{
		HighlightedDynamicMeshColorElements.Reset();
		HighlightedDynamicMeshBaseColorByElement.Reset();
		return;
	}

	TMap<int32, TArray<uint64>> HighlightedElementsByMesh;
	for (const uint64 ElementKey : HighlightedDynamicMeshColorElements)
	{
		HighlightedElementsByMesh.FindOrAdd(static_cast<int32>(ElementKey >> 32)).Add(ElementKey);
	}

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
			[this, &MeshHighlightedElements](UE::Geometry::FDynamicMesh3& Mesh)
			{
				if (!Mesh.HasAttributes())
				{
					return;
				}

				auto* ColorOverlay = Mesh.Attributes()->PrimaryColors();
				if (ColorOverlay)
				{
					for (const uint64 ElementKey : MeshHighlightedElements)
					{
						const int32 ElementId = static_cast<int32>(ElementKey & 0xffffffff);
						if (const FLinearColor* BaseColor = HighlightedDynamicMeshBaseColorByElement.Find(ElementKey))
						{
							ColorOverlay->SetElement(ElementId, FVector4f(BaseColor->R, BaseColor->G, BaseColor->B, BaseColor->A));
						}
					}
				}
			},
			EDynamicMeshChangeType::DeformationEdit,
			EDynamicMeshAttributeChangeFlags::VertexColors,
			false);
	}

	HighlightedDynamicMeshColorElements.Reset();
	HighlightedDynamicMeshBaseColorByElement.Reset();
}
