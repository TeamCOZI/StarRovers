#include "Conveyor/SRConveyorComponentPool.h"

#include "Components/DynamicMeshComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/SplineComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "GameFramework/Actor.h"

namespace
{
	const FName ConveyorVisualSplineNameBase(TEXT("ConveyorVisualSpline"));
}

UDynamicMeshComponent* StarRovers::Conveyor::FSRConveyorComponentPool::EnsureBeltMeshComponent(
	AActor* OwnerActor,
	USceneComponent* AttachParent,
	TObjectPtr<UDynamicMeshComponent>& BeltMeshComponent)
{
	if (IsValid(BeltMeshComponent))
	{
		return BeltMeshComponent;
	}

	if (!IsValid(OwnerActor) || !IsValid(AttachParent))
	{
		return nullptr;
	}

	const FName BeltMeshComponentName = MakeUniqueObjectName(OwnerActor, UDynamicMeshComponent::StaticClass(), FName(TEXT("ConveyorBeltMesh")));
	BeltMeshComponent = NewObject<UDynamicMeshComponent>(OwnerActor, BeltMeshComponentName);
	if (!IsValid(BeltMeshComponent))
	{
		return nullptr;
	}

	BeltMeshComponent->SetupAttachment(AttachParent);
	BeltMeshComponent->SetRelativeTransform(FTransform::Identity);
	BeltMeshComponent->SetMobility(EComponentMobility::Movable);
	BeltMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeltMeshComponent->SetGenerateOverlapEvents(false);
	BeltMeshComponent->SetCastShadow(false);
	BeltMeshComponent->SetVisibility(true);
	BeltMeshComponent->SetHiddenInGame(false);
	OwnerActor->AddInstanceComponent(BeltMeshComponent);
	BeltMeshComponent->RegisterComponent();
	return BeltMeshComponent;
}

void StarRovers::Conveyor::FSRConveyorComponentPool::ClearBeltMeshComponent(
	UDynamicMeshComponent* BeltMeshComponent,
	bool bHideComponent)
{
	if (!IsValid(BeltMeshComponent))
	{
		return;
	}

	UE::Geometry::FDynamicMesh3 EmptyMesh;
	EmptyMesh.EnableAttributes();
	EmptyMesh.Attributes()->EnablePrimaryColors();
	EmptyMesh.Attributes()->SetNumUVLayers(1);
	BeltMeshComponent->SetMesh(MoveTemp(EmptyMesh));

	if (bHideComponent)
	{
		BeltMeshComponent->SetVisibility(false);
		BeltMeshComponent->SetHiddenInGame(true);
	}
}

ULineBatchComponent* StarRovers::Conveyor::FSRConveyorComponentPool::EnsurePathDebugLineBatchComponent(
	AActor* OwnerActor,
	USceneComponent* AttachParent,
	TObjectPtr<ULineBatchComponent>& PathDebugLineBatchComponent)
{
	if (IsValid(PathDebugLineBatchComponent))
	{
		return PathDebugLineBatchComponent;
	}

	if (!IsValid(OwnerActor) || !IsValid(AttachParent))
	{
		return nullptr;
	}

	const FName DebugLineBatchName = MakeUniqueObjectName(OwnerActor, ULineBatchComponent::StaticClass(), FName(TEXT("ConveyorPathDebugLineBatch")));
	PathDebugLineBatchComponent = NewObject<ULineBatchComponent>(OwnerActor, DebugLineBatchName);
	if (!IsValid(PathDebugLineBatchComponent))
	{
		return nullptr;
	}

	PathDebugLineBatchComponent->SetupAttachment(AttachParent);
	PathDebugLineBatchComponent->SetMobility(EComponentMobility::Movable);
	PathDebugLineBatchComponent->SetUsingAbsoluteLocation(true);
	PathDebugLineBatchComponent->SetUsingAbsoluteRotation(true);
	PathDebugLineBatchComponent->SetUsingAbsoluteScale(true);
	PathDebugLineBatchComponent->ComponentTags.AddUnique(TEXT("StarRovers.ConveyorPathDebugLine"));
	OwnerActor->AddInstanceComponent(PathDebugLineBatchComponent);
	PathDebugLineBatchComponent->RegisterComponent();
	return PathDebugLineBatchComponent;
}

USplineComponent* StarRovers::Conveyor::FSRConveyorComponentPool::EnsurePCGSplineComponent(
	AActor* OwnerActor,
	USceneComponent* AttachParent,
	FName PCGSplineComponentTag,
	TArray<TObjectPtr<USplineComponent>>& PCGSplineComponents,
	int32 SplineIndex)
{
	if (SplineIndex < 0)
	{
		return nullptr;
	}

	while (PCGSplineComponents.Num() <= SplineIndex)
	{
		PCGSplineComponents.Add(nullptr);
	}

	if (IsValid(PCGSplineComponents[SplineIndex]))
	{
		return PCGSplineComponents[SplineIndex];
	}

	if (!IsValid(OwnerActor) || !IsValid(AttachParent))
	{
		return nullptr;
	}

	const FName RequestedName(*FString::Printf(TEXT("%s_%d"), *ConveyorVisualSplineNameBase.ToString(), SplineIndex));
	const FName SplineComponentName = MakeUniqueObjectName(OwnerActor, USplineComponent::StaticClass(), RequestedName);
	USplineComponent* SplineComponent = NewObject<USplineComponent>(OwnerActor, SplineComponentName);
	if (!IsValid(SplineComponent))
	{
		return nullptr;
	}

	SplineComponent->SetupAttachment(AttachParent);
	SplineComponent->SetRelativeTransform(FTransform::Identity);
	SplineComponent->SetMobility(EComponentMobility::Movable);
	SplineComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SplineComponent->SetGenerateOverlapEvents(false);
	SplineComponent->SetHiddenInGame(true);
	SplineComponent->SetVisibility(false);
	if (!PCGSplineComponentTag.IsNone())
	{
		SplineComponent->ComponentTags.AddUnique(PCGSplineComponentTag);
	}
	SplineComponent->ComponentTags.AddUnique(TEXT("StarRovers.ConveyorVisualSpline"));

	OwnerActor->AddInstanceComponent(SplineComponent);
	SplineComponent->RegisterComponent();
	PCGSplineComponents[SplineIndex] = SplineComponent;
	return SplineComponent;
}

void StarRovers::Conveyor::FSRConveyorComponentPool::ClearUnusedPCGSplineComponents(
	TArray<TObjectPtr<USplineComponent>>& PCGSplineComponents,
	int32 FirstUnusedSplineIndex)
{
	for (int32 SplineIndex = FMath::Max(0, FirstUnusedSplineIndex); SplineIndex < PCGSplineComponents.Num(); ++SplineIndex)
	{
		USplineComponent* SplineComponent = PCGSplineComponents[SplineIndex];
		if (!IsValid(SplineComponent))
		{
			continue;
		}

		SplineComponent->ClearSplinePoints(false);
		SplineComponent->SetVisibility(false);
		SplineComponent->SetHiddenInGame(true);
		SplineComponent->UpdateSpline();
	}
}
