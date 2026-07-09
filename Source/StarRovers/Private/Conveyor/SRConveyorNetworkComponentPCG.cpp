#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Conveyor/SRConveyorPCGGenerationCoordinator.h"
#include "Conveyor/SRConveyorPCGSplineInputBuilder.h"
#include "Conveyor/SRConveyorPCGSplineMeshRebaser.h"
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
	StarRovers::Conveyor::FSRConveyorPCGSplineInputBuildSettings InputSettings;
	InputSettings.bBuildPCGSplineInputs = bBuildPCGSplineInputs;
	InputSettings.BeltWidth = BeltWidth;
	InputSettings.BeltSurfaceOffset = BeltSurfaceOffset;
	InputSettings.PCGSplineHeightOffset = PCGSplineHeightOffset;
	InputSettings.PCGSplineComponentTag = PCGSplineComponentTag;
	StarRovers::Conveyor::FSRConveyorPCGSplineInputBuilder::Refresh(
		GetOwner(),
		this,
		SurfaceGrid,
		BeltPaths,
		InputSettings,
		PCGSplineComponents);
}

void USRConveyorNetworkComponent::RequestPCGGeneration()
{
	if (!bAutoGeneratePCG)
	{
		return;
	}

	StarRovers::Conveyor::FSRConveyorPCGGenerationCoordinator::RequestGeneration(
		GetOwner(),
		[this](UPCGComponent* PCGComponent)
	{
		PCGComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
		PCGComponent->OnPCGGraphGeneratedDelegate.AddUObject(this, &USRConveyorNetworkComponent::HandlePCGGraphGenerated);
	});
}

void USRConveyorNetworkComponent::BindPCGGenerationDelegates()
{
	StarRovers::Conveyor::FSRConveyorPCGGenerationCoordinator::BindGenerationDelegates(
		GetOwner(),
		[this](UPCGComponent* PCGComponent)
	{
		PCGComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
		PCGComponent->OnPCGGraphGeneratedDelegate.AddUObject(this, &USRConveyorNetworkComponent::HandlePCGGraphGenerated);
	});
}

void USRConveyorNetworkComponent::HandlePCGGraphGenerated(UPCGComponent* PCGComponent)
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

	StarRovers::Conveyor::FSRConveyorPCGSplineMeshRebaseSettings RebaseSettings;
	RebaseSettings.BeltWidth = BeltWidth;
	RebaseSettings.BeltSurfaceOffset = BeltSurfaceOffset;
	RebaseSettings.PCGSplineHeightOffset = PCGSplineHeightOffset;
	RebaseSettings.ComponentTransform = GetComponentTransform();
	RebaseSettings.AttachParent = this;
	StarRovers::Conveyor::FSRConveyorPCGSplineMeshRebaser::Rebase(
		PCGComponent,
		SurfaceGrid,
		BeltPaths,
		RebaseSettings);
}
