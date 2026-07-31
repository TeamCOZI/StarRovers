#include "Conveyor/SRConveyorItemLabelResolver.h"

#include "Conveyor/SRConveyorConnectionQuery.h"
#include "Conveyor/SRConveyorNetworkGeometry.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	ESRGlyphType ResolveDominantGlyph(const FSRPattern& Pattern)
	{
		int32 GlyphCounts[6] = {};
		for (const ESRGlyphType Glyph : Pattern.Cells)
		{
			const int32 GlyphIndex = static_cast<int32>(Glyph);
			if (GlyphIndex > 0 && GlyphIndex < UE_ARRAY_COUNT(GlyphCounts))
			{
				++GlyphCounts[GlyphIndex];
			}
		}

		int32 BestGlyphIndex = 0;
		for (int32 GlyphIndex = 1; GlyphIndex < UE_ARRAY_COUNT(GlyphCounts); ++GlyphIndex)
		{
			if (GlyphCounts[GlyphIndex] > GlyphCounts[BestGlyphIndex])
			{
				BestGlyphIndex = GlyphIndex;
			}
		}
		return static_cast<ESRGlyphType>(BestGlyphIndex);
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
	return FText::FromString(FString::Printf(
		TEXT("P %08X"),
		ResourceInstance.Pattern.GetStableHash()));
}

FColor StarRovers::Conveyor::FSRConveyorItemLabelResolver::ResolveLabelColor(
	const FSRResourceInstance& ResourceInstance,
	const FSRConveyorItemLabelSettings& Settings)
{
	switch (ResolveDominantGlyph(ResourceInstance.Pattern))
	{
	case ESRGlyphType::Metal:
		return Settings.ItemPatternSparseColor.ToFColor(true);
	case ESRGlyphType::Organic:
		return FLinearColor::LerpUsingHSV(
			Settings.ItemPatternSparseColor,
			Settings.ItemPatternDenseColor,
			0.33f).ToFColor(true);
	case ESRGlyphType::Crystal:
		return FLinearColor::LerpUsingHSV(
			Settings.ItemPatternSparseColor,
			Settings.ItemPatternDenseColor,
			0.66f).ToFColor(true);
	case ESRGlyphType::Fluid:
		return Settings.ItemPatternDenseColor.ToFColor(true);
	case ESRGlyphType::Plasma:
		return Settings.ItemPatternSpecialColor.ToFColor(true);
	default:
		return FLinearColor::White.ToFColor(true);
	}
}

float StarRovers::Conveyor::FSRConveyorItemLabelResolver::ResolveLabelWorldSize(
	const FSRConveyorLaneKey& LaneKey,
	const FSRResourceInstance& ResourceInstance,
	float TimeSeconds,
	const FSRConveyorItemLabelSettings& Settings)
{
	const float DensityAlpha = FMath::Clamp(
		static_cast<float>(ResourceInstance.Pattern.GetOccupiedCellCount())
			/ static_cast<float>(StarRovers::Pattern::CellCount),
		0.0f,
		1.0f);
	const float PatternScale = FMath::Clamp(
		FMath::Lerp(1.0f, Settings.ItemPatternLabelMaxScale, DensityAlpha),
		1.0f,
		FMath::Max(1.0f, Settings.ItemPatternLabelMaxScale));
	const float PulseStrength = FMath::Clamp(
		(PatternScale - 1.0f)
			/ FMath::Max(0.01f, Settings.ItemPatternLabelMaxScale - 1.0f),
		0.0f,
		1.0f);
	const float LanePhase = static_cast<float>(GetTypeHash(LaneKey) % 97) * 0.17f;
	const float PulseScale = 1.0f + FMath::Sin(TimeSeconds * 6.0f + LanePhase) * 0.08f * PulseStrength;
	return FMath::Max(1.0f, Settings.ItemPatternLabelWorldSize * PatternScale * PulseScale);
}
