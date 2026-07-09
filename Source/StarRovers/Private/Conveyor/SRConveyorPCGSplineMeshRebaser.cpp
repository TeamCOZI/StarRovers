#include "Conveyor/SRConveyorPCGSplineMeshRebaser.h"

#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Conveyor/SRConveyorRibbonBuilder.h"
#include "GameFramework/Actor.h"
#include "PCGComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	struct FSRConveyorPCGSplineMeshRebaseSegment
	{
		FVector LocalStart = FVector::ZeroVector;
		FVector LocalEnd = FVector::ZeroVector;
		FVector LocalTangent = FVector::ZeroVector;
		FVector LocalUpDirection = FVector::UpVector;
	};
}

void StarRovers::Conveyor::FSRConveyorPCGSplineMeshRebaser::Rebase(
	UPCGComponent* PCGComponent,
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRConveyorBeltPath>& BeltPaths,
	const FSRConveyorPCGSplineMeshRebaseSettings& Settings)
{
	if (!IsValid(PCGComponent) || !IsValid(SurfaceGrid) || !Settings.AttachParent.IsValid())
	{
		return;
	}

	AActor* OwnerActor = PCGComponent->GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	TArray<FSRConveyorPCGSplineMeshRebaseSegment> ExpectedSegments;
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

			FVector WorldUpDirection = (WorldNormals[SegmentIndex] + WorldNormals[SegmentIndex + 1]).GetSafeNormal();
			if (WorldUpDirection.IsNearlyZero())
			{
				WorldUpDirection = WorldNormals[SegmentIndex].GetSafeNormal();
			}

			FSRConveyorPCGSplineMeshRebaseSegment Segment;
			Segment.LocalStart = Settings.ComponentTransform.InverseTransformPosition(SegmentStart);
			Segment.LocalEnd = Settings.ComponentTransform.InverseTransformPosition(SegmentEnd);
			Segment.LocalTangent = Settings.ComponentTransform.InverseTransformVectorNoScale(SegmentVector);
			Segment.LocalUpDirection = Settings.ComponentTransform.InverseTransformVectorNoScale(WorldUpDirection).GetSafeNormal();
			if (Segment.LocalUpDirection.IsNearlyZero())
			{
				Segment.LocalUpDirection = FVector::UpVector;
			}
			ExpectedSegments.Add(Segment);
		}
	}

	if (ExpectedSegments.IsEmpty())
	{
		return;
	}

	TArray<USplineMeshComponent*> GeneratedSplineMeshes;
	OwnerActor->GetComponents<USplineMeshComponent>(GeneratedSplineMeshes);
	GeneratedSplineMeshes.RemoveAll([PCGComponent](const USplineMeshComponent* SplineMeshComponent)
	{
		return !IsValid(SplineMeshComponent)
			|| !SplineMeshComponent->ComponentTags.Contains(PCGComponent->GetFName());
	});
	GeneratedSplineMeshes.Sort([](const USplineMeshComponent& Left, const USplineMeshComponent& Right)
	{
		return Left.GetFName().LexicalLess(Right.GetFName());
	});

	USceneComponent* AttachParent = Settings.AttachParent.Get();
	const int32 SegmentCount = FMath::Min(ExpectedSegments.Num(), GeneratedSplineMeshes.Num());
	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		USplineMeshComponent* SplineMeshComponent = GeneratedSplineMeshes[SegmentIndex];
		if (!IsValid(SplineMeshComponent))
		{
			continue;
		}

		const FSRConveyorPCGSplineMeshRebaseSegment& Segment = ExpectedSegments[SegmentIndex];
		SplineMeshComponent->SetMobility(EComponentMobility::Movable);
		SplineMeshComponent->AttachToComponent(AttachParent, FAttachmentTransformRules::KeepRelativeTransform);
		SplineMeshComponent->SetRelativeTransform(FTransform::Identity);
		SplineMeshComponent->SetSplineUpDir(Segment.LocalUpDirection, false);
		SplineMeshComponent->SetStartAndEnd(Segment.LocalStart, Segment.LocalTangent, Segment.LocalEnd, Segment.LocalTangent, false);
		SplineMeshComponent->SetStartRollDegrees(0.0f, false);
		SplineMeshComponent->SetEndRollDegrees(0.0f, false);
		SplineMeshComponent->SetVisibility(true);
		SplineMeshComponent->SetHiddenInGame(false);
		SplineMeshComponent->UpdateMesh();
	}
}
