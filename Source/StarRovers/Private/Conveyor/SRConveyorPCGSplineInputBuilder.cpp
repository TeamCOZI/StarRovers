#include "Conveyor/SRConveyorPCGSplineInputBuilder.h"

#include "Components/SplineComponent.h"
#include "Conveyor/SRConveyorComponentPool.h"
#include "Conveyor/SRConveyorRibbonBuilder.h"
#include "Surface/SRPlanetSurfaceGrid.h"

void StarRovers::Conveyor::FSRConveyorPCGSplineInputBuilder::Refresh(
	AActor* OwnerActor,
	USceneComponent* AttachParent,
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRConveyorBeltPath>& BeltPaths,
	const FSRConveyorPCGSplineInputBuildSettings& Settings,
	TArray<TObjectPtr<USplineComponent>>& PCGSplineComponents)
{
	if (!Settings.bBuildPCGSplineInputs || !IsValid(SurfaceGrid))
	{
		FSRConveyorComponentPool::ClearUnusedPCGSplineComponents(PCGSplineComponents, 0);
		return;
	}

	int32 UsedSplineCount = 0;
	TArray<FVector> WorldPoints;
	TArray<FVector> WorldNormals;
	FSRConveyorRibbonBuildSettings RibbonSettings;
	RibbonSettings.BeltWidth = Settings.BeltWidth;
	RibbonSettings.BeltSurfaceOffset = Settings.BeltSurfaceOffset;
	RibbonSettings.PCGSplineHeightOffset = Settings.PCGSplineHeightOffset;
	for (const FSRConveyorBeltPath& BeltPath : BeltPaths)
	{
		if (!FSRConveyorRibbonBuilder::BuildPathSplinePoints(SurfaceGrid, BeltPath, RibbonSettings, WorldPoints, WorldNormals))
		{
			continue;
		}

		for (int32 SegmentIndex = 0; SegmentIndex + 1 < WorldPoints.Num(); ++SegmentIndex)
		{
			const FVector SegmentStart = WorldPoints[SegmentIndex];
			const FVector SegmentEnd = WorldPoints[SegmentIndex + 1];
			const FVector SegmentVector = SegmentEnd - SegmentStart;
			if (SegmentVector.SizeSquared() <= FMath::Square(KINDA_SMALL_NUMBER))
			{
				continue;
			}

			USplineComponent* SplineComponent = FSRConveyorComponentPool::EnsurePCGSplineComponent(
				OwnerActor,
				AttachParent,
				Settings.PCGSplineComponentTag,
				PCGSplineComponents,
				UsedSplineCount);
			if (!IsValid(SplineComponent))
			{
				continue;
			}

			const FTransform SplineTransform = SplineComponent->GetComponentTransform();
			const FVector LocalSegmentStart = SplineTransform.InverseTransformPosition(SegmentStart);
			const FVector LocalSegmentEnd = SplineTransform.InverseTransformPosition(SegmentEnd);
			const FVector LocalSegmentVector = SplineTransform.InverseTransformVectorNoScale(SegmentVector);
			const FVector LocalStartNormal = SplineTransform.InverseTransformVectorNoScale(WorldNormals[SegmentIndex].GetSafeNormal()).GetSafeNormal();
			const FVector LocalEndNormal = SplineTransform.InverseTransformVectorNoScale(WorldNormals[SegmentIndex + 1].GetSafeNormal()).GetSafeNormal();

			SplineComponent->ClearSplinePoints(false);
			SplineComponent->AddSplinePoint(LocalSegmentStart, ESplineCoordinateSpace::Local, false);
			SplineComponent->AddSplinePoint(LocalSegmentEnd, ESplineCoordinateSpace::Local, false);
			SplineComponent->SetSplinePointType(0, ESplinePointType::Linear, false);
			SplineComponent->SetSplinePointType(1, ESplinePointType::Linear, false);
			SplineComponent->SetTangentAtSplinePoint(0, LocalSegmentVector, ESplineCoordinateSpace::Local, false);
			SplineComponent->SetTangentAtSplinePoint(1, LocalSegmentVector, ESplineCoordinateSpace::Local, false);
			SplineComponent->SetUpVectorAtSplinePoint(0, LocalStartNormal, ESplineCoordinateSpace::Local, false);
			SplineComponent->SetUpVectorAtSplinePoint(1, LocalEndNormal, ESplineCoordinateSpace::Local, false);
			SplineComponent->SetClosedLoop(false, false);
			SplineComponent->UpdateSpline();
			SplineComponent->SetVisibility(false);
			SplineComponent->SetHiddenInGame(true);
			++UsedSplineCount;
		}
	}

	FSRConveyorComponentPool::ClearUnusedPCGSplineComponents(PCGSplineComponents, UsedSplineCount);
}
