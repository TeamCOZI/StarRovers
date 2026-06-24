#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "GameFramework/Actor.h"
#include "PCGComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

FName USRConveyorNetworkComponent::GetConveyorActorSplineComponentTag() const
{
	return PCGSplineComponentTag;
}

float USRConveyorNetworkComponent::GetConveyorActorSurfaceOffset() const
{
	return FMath::Max(0.0f, BeltSurfaceOffset + PCGSplineHeightOffset);
}

void USRConveyorNetworkComponent::RefreshPCGSplineInputs(USRPlanetSurfaceGrid* SurfaceGrid)
{
	if (!bBuildPCGSplineInputs || !IsValid(SurfaceGrid))
	{
		ClearUnusedPCGSplineComponents(0);
		return;
	}

	int32 UsedSplineCount = 0;
	TArray<FVector> WorldPoints;
	TArray<FVector> WorldNormals;
	for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
	{
		if (!BuildConveyorPathSplinePoints(SurfaceGrid, VisualPath, WorldPoints, WorldNormals))
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

			USplineComponent* SplineComponent = EnsurePCGSplineComponent(UsedSplineCount);
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

	ClearUnusedPCGSplineComponents(UsedSplineCount);
}

void USRConveyorNetworkComponent::RequestPCGGeneration()
{
	if (!bAutoGeneratePCG)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	TArray<UPCGComponent*> PCGComponents;
	OwnerActor->GetComponents<UPCGComponent>(PCGComponents);
	for (UPCGComponent* PCGComponent : PCGComponents)
	{
		if (IsValid(PCGComponent))
		{
			PCGComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
			PCGComponent->OnPCGGraphGeneratedDelegate.AddUObject(this, &USRConveyorNetworkComponent::HandlePCGGraphGenerated);
			PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
			PCGComponent->Generate(true);
		}
	}
}

void USRConveyorNetworkComponent::BindPCGGenerationDelegates()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	TArray<UPCGComponent*> PCGComponents;
	OwnerActor->GetComponents<UPCGComponent>(PCGComponents);
	for (UPCGComponent* PCGComponent : PCGComponents)
	{
		if (IsValid(PCGComponent))
		{
			PCGComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
			PCGComponent->OnPCGGraphGeneratedDelegate.AddUObject(this, &USRConveyorNetworkComponent::HandlePCGGraphGenerated);
		}
	}
}

void USRConveyorNetworkComponent::HandlePCGGraphGenerated(UPCGComponent* PCGComponent)
{
	RebaseGeneratedPCGSplineMeshes(PCGComponent);
}

void USRConveyorNetworkComponent::RebaseGeneratedPCGSplineMeshes(UPCGComponent* PCGComponent)
{
	if (!IsValid(PCGComponent))
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || PCGComponent->GetOwner() != OwnerActor)
	{
		return;
	}

	USRPlanetSurfaceGrid* SurfaceGrid = OwnerActor->FindComponentByClass<USRPlanetSurfaceGrid>();
	if (!IsValid(SurfaceGrid))
	{
		return;
	}

	struct FConveyorSplineMeshSegment
	{
		FVector LocalStart = FVector::ZeroVector;
		FVector LocalEnd = FVector::ZeroVector;
		FVector LocalTangent = FVector::ZeroVector;
		FVector LocalUpDirection = FVector::UpVector;
	};

	const FTransform ComponentTransform = GetComponentTransform();
	TArray<FConveyorSplineMeshSegment> ExpectedSegments;
	TArray<FVector> WorldPoints;
	TArray<FVector> WorldNormals;
	for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
	{
		if (!BuildConveyorPathSplinePoints(SurfaceGrid, VisualPath, WorldPoints, WorldNormals))
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

			FConveyorSplineMeshSegment Segment;
			Segment.LocalStart = ComponentTransform.InverseTransformPosition(SegmentStart);
			Segment.LocalEnd = ComponentTransform.InverseTransformPosition(SegmentEnd);
			Segment.LocalTangent = ComponentTransform.InverseTransformVectorNoScale(SegmentVector);
			Segment.LocalUpDirection = ComponentTransform.InverseTransformVectorNoScale(WorldUpDirection).GetSafeNormal();
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

	const int32 SegmentCount = FMath::Min(ExpectedSegments.Num(), GeneratedSplineMeshes.Num());
	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		USplineMeshComponent* SplineMeshComponent = GeneratedSplineMeshes[SegmentIndex];
		if (!IsValid(SplineMeshComponent))
		{
			continue;
		}

		const FConveyorSplineMeshSegment& Segment = ExpectedSegments[SegmentIndex];
		SplineMeshComponent->SetMobility(EComponentMobility::Movable);
		SplineMeshComponent->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
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
