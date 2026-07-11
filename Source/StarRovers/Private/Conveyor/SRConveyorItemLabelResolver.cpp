#include "Conveyor/SRConveyorItemLabelResolver.h"

#include "Conveyor/SRConveyorConnectionQuery.h"
#include "Conveyor/SRConveyorNetworkGeometry.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	FString FormatEnergyValue(double Value)
	{
		const double AbsValue = FMath::Abs(Value);
		if (AbsValue >= 1000.0)
		{
			return FString::Printf(TEXT("%.0f"), Value);
		}
		if (AbsValue >= 100.0)
		{
			return FString::Printf(TEXT("%.1f"), Value);
		}
		return FString::Printf(TEXT("%.2f"), Value);
	}

	bool ResolveOutwardNormal(
		const USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellInfo& CellInfo,
		FVector& OutNormal)
	{
		if (!IsValid(SurfaceGrid))
		{
			OutNormal = FVector::UpVector;
			return false;
		}

		const FVector SurfaceCenter = SurfaceGrid->GetComponentTransform().GetLocation();
		OutNormal = CellInfo.WorldNormal.GetSafeNormal();
		if (OutNormal.IsNearlyZero())
		{
			OutNormal = (CellInfo.WorldCenter - SurfaceCenter).GetSafeNormal();
		}
		else if (FVector::DotProduct(OutNormal, CellInfo.WorldCenter - SurfaceCenter) < 0.0f)
		{
			OutNormal *= -1.0f;
		}

		if (OutNormal.IsNearlyZero())
		{
			OutNormal = FVector::UpVector;
		}
		return true;
	}

	float ResolvePathLayerHeight(
		const TArray<FSRConveyorBeltPath>& BeltPaths,
		const FSRConveyorLaneKey& LaneKey,
		float DefaultLayerHeight)
	{
		for (const FSRConveyorBeltPath& BeltPath : BeltPaths)
		{
			if (BeltPath.Layer == LaneKey.Layer && BeltPath.CellIds.Contains(LaneKey.CellId))
			{
				return BeltPath.LayerHeight;
			}
		}

		return DefaultLayerHeight;
	}

	bool HasDirection(
		ESRConveyorGridDirection Direction,
		ESRConveyorGridDirection FirstDirection,
		ESRConveyorGridDirection SecondDirection,
		ESRConveyorGridDirection ThirdDirection)
	{
		return Direction != ESRConveyorGridDirection::None
			&& (Direction == FirstDirection
				|| Direction == SecondDirection
				|| Direction == ThirdDirection);
	}

	int32 BuildSortedDirections(
		ESRConveyorGridDirection FirstDirection,
		ESRConveyorGridDirection SecondDirection,
		ESRConveyorGridDirection ThirdDirection,
		ESRConveyorGridDirection* OutDirections)
	{
		int32 DirectionCount = 0;
		if (HasDirection(ESRConveyorGridDirection::NegativeV, FirstDirection, SecondDirection, ThirdDirection))
		{
			OutDirections[DirectionCount++] = ESRConveyorGridDirection::NegativeV;
		}
		if (HasDirection(ESRConveyorGridDirection::PositiveU, FirstDirection, SecondDirection, ThirdDirection))
		{
			OutDirections[DirectionCount++] = ESRConveyorGridDirection::PositiveU;
		}
		if (HasDirection(ESRConveyorGridDirection::PositiveV, FirstDirection, SecondDirection, ThirdDirection))
		{
			OutDirections[DirectionCount++] = ESRConveyorGridDirection::PositiveV;
		}
		if (HasDirection(ESRConveyorGridDirection::NegativeU, FirstDirection, SecondDirection, ThirdDirection))
		{
			OutDirections[DirectionCount++] = ESRConveyorGridDirection::NegativeU;
		}

		return DirectionCount;
	}

	ESRConveyorGridDirection ResolveVisualOutputDirection(const FSRConveyorSegment& Segment)
	{
		ESRConveyorGridDirection OutputDirections[3];
		const int32 OutputDirectionCount = BuildSortedDirections(
			Segment.OutputDirection,
			Segment.BranchOutputDirection,
			Segment.SecondBranchOutputDirection,
			OutputDirections);
		if (OutputDirectionCount <= 0)
		{
			return ESRConveyorGridDirection::None;
		}

		const int32 OutputDirectionIndex = Segment.NextOutputDirectionIndex >= 0 && Segment.NextOutputDirectionIndex < OutputDirectionCount
			? Segment.NextOutputDirectionIndex
			: 0;
		return OutputDirections[OutputDirectionIndex];
	}

	ESRConveyorGridDirection ResolveFirstInputDirection(const FSRConveyorSegment& Segment)
	{
		ESRConveyorGridDirection InputDirections[3];
		return BuildSortedDirections(
			Segment.InputDirection,
			Segment.MergeInputDirection,
			Segment.SecondMergeInputDirection,
			InputDirections) > 0
			? InputDirections[0]
			: ESRConveyorGridDirection::None;
	}
}

bool StarRovers::Conveyor::FSRConveyorItemLabelResolver::ResolveWorldLocation(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	const TArray<FSRConveyorBeltPath>& BeltPaths,
	const FSRConveyorItem& Item,
	const FSRConveyorItemLabelSettings& Settings,
	FVector& OutWorldLocation,
	FVector& OutWorldNormal)
{
	OutWorldLocation = FVector::ZeroVector;
	OutWorldNormal = FVector::UpVector;
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	const FSRConveyorLaneKey LaneKey = Item.CurrentLane;
	FSRPlanetSurfaceGridCellInfo CurrentCellInfo;
	if (!SurfaceGrid->GetCellInfoById(LaneKey.CellId, CurrentCellInfo))
	{
		return false;
	}

	const float LayerHeight = ResolvePathLayerHeight(BeltPaths, LaneKey, Settings.DefaultLayerHeight);
	FVector CurrentNormal = FVector::UpVector;
	ResolveOutwardNormal(SurfaceGrid, CurrentCellInfo, CurrentNormal);
	const float HeightOffset = static_cast<float>(FMath::Max(0, LaneKey.Layer)) * FMath::Max(0.0f, LayerHeight)
		+ FMath::Max(0.0f, Settings.BeltSurfaceOffset)
		+ FMath::Max(0.0f, Settings.ItemLabelHeightOffset);
	const FVector CurrentPoint = CurrentCellInfo.WorldCenter + CurrentNormal * HeightOffset;

	FVector StartPoint = CurrentPoint;
	FVector EndPoint = CurrentPoint;
	FVector EndNormal = CurrentNormal;
	if (const FSRConveyorSegment* Segment = Segments.Find(LaneKey))
	{
		FSRConveyorLaneKey NextLaneKey;
		const ESRConveyorGridDirection VisualOutputDirection = ResolveVisualOutputDirection(*Segment);
		bool bHasVisualOutput = false;
		if (FSRConveyorConnectionQuery::TryResolveLaneByDirection(SurfaceGrid, *Segment, VisualOutputDirection, NextLaneKey) && Segments.Contains(NextLaneKey))
		{
			FSRPlanetSurfaceGridCellInfo NextCellInfo;
			if (SurfaceGrid->GetCellInfoById(NextLaneKey.CellId, NextCellInfo))
			{
				FVector NextNormal = FVector::UpVector;
				ResolveOutwardNormal(SurfaceGrid, NextCellInfo, NextNormal);
				StartPoint = CurrentPoint;
				EndPoint = NextCellInfo.WorldCenter + NextNormal * HeightOffset;
				EndNormal = NextNormal;
				bHasVisualOutput = true;
			}
		}
		if (!bHasVisualOutput)
		{
			const ESRConveyorGridDirection VisualInputDirection = ResolveFirstInputDirection(*Segment);
			if (VisualInputDirection != ESRConveyorGridDirection::None)
			{
				FSRPlanetSurfaceGridCellNeighbors Neighbors;
				FSRPlanetSurfaceGridCellId PreviousCellId;
				if (SurfaceGrid->GetCellNeighbors(LaneKey.CellId, Neighbors)
					&& FSRConveyorNetworkGeometry::GetNeighborCellIdByDirection(Neighbors, VisualInputDirection, PreviousCellId))
				{
					FSRPlanetSurfaceGridCellInfo PreviousCellInfo;
					if (SurfaceGrid->GetCellInfoById(PreviousCellId, PreviousCellInfo))
					{
						FVector PreviousNormal = FVector::UpVector;
						ResolveOutwardNormal(SurfaceGrid, PreviousCellInfo, PreviousNormal);
						const FVector PreviousPoint = PreviousCellInfo.WorldCenter + PreviousNormal * HeightOffset;
						const FVector TravelDirection = (CurrentPoint - PreviousPoint).GetSafeNormal();
						if (!TravelDirection.IsNearlyZero())
						{
							const float HalfCellTravelDistance = FVector::Distance(CurrentPoint, PreviousPoint) * 0.35f;
							StartPoint = CurrentPoint - TravelDirection * HalfCellTravelDistance;
							EndPoint = CurrentPoint + TravelDirection * HalfCellTravelDistance;
						}
					}
				}
			}
		}
	}

	const float Alpha = FMath::Clamp(Item.Progress, 0.0f, 1.0f);
	OutWorldLocation = FMath::Lerp(StartPoint, EndPoint, Alpha);
	OutWorldNormal = FMath::Lerp(CurrentNormal, EndNormal, Alpha).GetSafeNormal();
	if (OutWorldNormal.IsNearlyZero())
	{
		OutWorldNormal = CurrentNormal;
	}
	return true;
}

FText StarRovers::Conveyor::FSRConveyorItemLabelResolver::BuildLabelText(
	const FSRResourceInstance& ResourceInstance)
{
	return FText::FromString(FString::Printf(TEXT("E %s"), *FormatEnergyValue(ResourceInstance.EnergyValue)));
}

FColor StarRovers::Conveyor::FSRConveyorItemLabelResolver::ResolveLabelColor(
	const FSRResourceInstance& ResourceInstance,
	const FSRConveyorItemLabelSettings& Settings)
{
	if (ResourceInstance.EnergyValue < 0.0)
	{
		return Settings.ItemEnergyNegativeColor.ToFColor(true);
	}

	const double EnergyMagnitude = FMath::Abs(ResourceInstance.EnergyValue);
	const float Alpha = FMath::Clamp(static_cast<float>(FMath::Loge(1.0 + EnergyMagnitude) / FMath::Loge(101.0)), 0.0f, 1.0f);
	return FLinearColor::LerpUsingHSV(Settings.ItemEnergyLowColor, Settings.ItemEnergyHighColor, Alpha).ToFColor(true);
}

float StarRovers::Conveyor::FSRConveyorItemLabelResolver::ResolveLabelWorldSize(
	const FSRConveyorLaneKey& LaneKey,
	const FSRResourceInstance& ResourceInstance,
	float TimeSeconds,
	const FSRConveyorItemLabelSettings& Settings)
{
	const double EnergyMagnitude = FMath::Abs(ResourceInstance.EnergyValue);
	const float EnergyScale = FMath::Clamp(
		1.0f + static_cast<float>(FMath::Loge(1.0 + EnergyMagnitude)) * 0.18f,
		1.0f,
		FMath::Max(1.0f, Settings.ItemEnergyLabelMaxScale));
	const float PulseStrength = FMath::Clamp((EnergyScale - 1.0f) / FMath::Max(0.01f, Settings.ItemEnergyLabelMaxScale - 1.0f), 0.0f, 1.0f);
	const float LanePhase = static_cast<float>(GetTypeHash(LaneKey) % 97) * 0.17f;
	const float PulseScale = 1.0f + FMath::Sin(TimeSeconds * 6.0f + LanePhase) * 0.08f * PulseStrength;
	return FMath::Max(1.0f, Settings.ItemEnergyLabelWorldSize * EnergyScale * PulseScale);
}
