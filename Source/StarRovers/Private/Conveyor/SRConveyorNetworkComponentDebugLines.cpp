#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Components/LineBatchComponent.h"
#include "SceneManagement.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	constexpr uint8 ConveyorPathDebugLineDepthPriority = SDPG_Foreground;
}

void USRConveyorNetworkComponent::RefreshPathDebugLines(USRPlanetSurfaceGrid* SurfaceGrid)
{
	if (IsValid(PathDebugLineBatchComponent))
	{
		PathDebugLineBatchComponent->Flush();
	}

	if (!bShowPathDebugLine || !IsValid(SurfaceGrid) || VisualPaths.IsEmpty())
	{
		return;
	}

	EnsurePathDebugLineBatchComponent();
	if (!IsValid(PathDebugLineBatchComponent))
	{
		return;
	}

	const FVector SurfaceCenter = SurfaceGrid->GetComponentTransform().GetLocation();
	const FColor LineColor = PathDebugLineColor.ToFColor(true);
	for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
	{
		if (VisualPath.CellIds.IsEmpty())
		{
			continue;
		}

		FSRPlanetSurfaceGridCellInfo StartCellInfo;
		FSRPlanetSurfaceGridCellInfo EndCellInfo;
		if (!SurfaceGrid->GetCellInfoById(VisualPath.CellIds[0], StartCellInfo)
			|| !SurfaceGrid->GetCellInfoById(VisualPath.CellIds.Last(), EndCellInfo))
		{
			continue;
		}

		FVector StartNormal = StartCellInfo.WorldNormal.GetSafeNormal();
		FVector EndNormal = EndCellInfo.WorldNormal.GetSafeNormal();
		if (StartNormal.IsNearlyZero())
		{
			StartNormal = (StartCellInfo.WorldCenter - SurfaceCenter).GetSafeNormal();
		}
		else if (FVector::DotProduct(StartNormal, StartCellInfo.WorldCenter - SurfaceCenter) < 0.0f)
		{
			StartNormal *= -1.0f;
		}
		if (EndNormal.IsNearlyZero())
		{
			EndNormal = (EndCellInfo.WorldCenter - SurfaceCenter).GetSafeNormal();
		}
		else if (FVector::DotProduct(EndNormal, EndCellInfo.WorldCenter - SurfaceCenter) < 0.0f)
		{
			EndNormal *= -1.0f;
		}

		const float HeightOffset = static_cast<float>(FMath::Max(0, VisualPath.Layer)) * FMath::Max(0.0f, VisualPath.LayerHeight) + BeltSurfaceOffset + 40.0f;
		const FVector StartPoint = StartCellInfo.WorldCenter + StartNormal * HeightOffset;
		const FVector EndPoint = EndCellInfo.WorldCenter + EndNormal * HeightOffset;
		PathDebugLineBatchComponent->DrawLine(
			StartPoint,
			EndPoint,
			LineColor,
			ConveyorPathDebugLineDepthPriority,
			FMath::Max(0.0f, PathDebugLineThickness),
			0.0f);
	}
}
