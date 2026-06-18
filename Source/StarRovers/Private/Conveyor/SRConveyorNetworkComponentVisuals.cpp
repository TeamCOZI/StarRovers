#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Components/DynamicMeshComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	const FName ConveyorVisualSplineNameBase(TEXT("ConveyorVisualSpline"));
}

void USRConveyorNetworkComponent::EnsureBeltMeshComponent()
{
	if (IsValid(BeltMeshComponent))
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	const FName BeltMeshComponentName = MakeUniqueObjectName(OwnerActor, UDynamicMeshComponent::StaticClass(), FName(TEXT("ConveyorBeltMesh")));
	BeltMeshComponent = NewObject<UDynamicMeshComponent>(OwnerActor, BeltMeshComponentName);
	if (!IsValid(BeltMeshComponent))
	{
		return;
	}

	BeltMeshComponent->SetupAttachment(this);
	BeltMeshComponent->SetRelativeTransform(FTransform::Identity);
	BeltMeshComponent->SetMobility(EComponentMobility::Movable);
	BeltMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeltMeshComponent->SetGenerateOverlapEvents(false);
	BeltMeshComponent->SetCastShadow(false);
	BeltMeshComponent->SetVisibility(true);
	BeltMeshComponent->SetHiddenInGame(false);
	OwnerActor->AddInstanceComponent(BeltMeshComponent);
	BeltMeshComponent->RegisterComponent();
}

void USRConveyorNetworkComponent::EnsurePathDebugLineBatchComponent()
{
	if (IsValid(PathDebugLineBatchComponent))
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	const FName DebugLineBatchName = MakeUniqueObjectName(OwnerActor, ULineBatchComponent::StaticClass(), FName(TEXT("ConveyorPathDebugLineBatch")));
	PathDebugLineBatchComponent = NewObject<ULineBatchComponent>(OwnerActor, DebugLineBatchName);
	if (!IsValid(PathDebugLineBatchComponent))
	{
		return;
	}

	PathDebugLineBatchComponent->SetupAttachment(this);
	PathDebugLineBatchComponent->SetMobility(EComponentMobility::Movable);
	PathDebugLineBatchComponent->SetUsingAbsoluteLocation(true);
	PathDebugLineBatchComponent->SetUsingAbsoluteRotation(true);
	PathDebugLineBatchComponent->SetUsingAbsoluteScale(true);
	PathDebugLineBatchComponent->ComponentTags.AddUnique(TEXT("StarRovers.ConveyorPathDebugLine"));
	OwnerActor->AddInstanceComponent(PathDebugLineBatchComponent);
	PathDebugLineBatchComponent->RegisterComponent();
}

USplineComponent* USRConveyorNetworkComponent::EnsurePCGSplineComponent(int32 SplineIndex)
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

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
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

	SplineComponent->SetupAttachment(this);
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

void USRConveyorNetworkComponent::ClearUnusedPCGSplineComponents(int32 FirstUnusedSplineIndex)
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
