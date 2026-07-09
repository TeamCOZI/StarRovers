#include "Conveyor/SRConveyorPCGGenerationCoordinator.h"

#include "GameFramework/Actor.h"
#include "PCGComponent.h"

void StarRovers::Conveyor::FSRConveyorPCGGenerationCoordinator::ConfigureGenerationTriggers(AActor* OwnerActor)
{
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
			PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
		}
	}
}

void StarRovers::Conveyor::FSRConveyorPCGGenerationCoordinator::BindGenerationDelegates(
	AActor* OwnerActor,
	TFunctionRef<void(UPCGComponent* PCGComponent)> BindDelegate)
{
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
			BindDelegate(PCGComponent);
		}
	}
}

void StarRovers::Conveyor::FSRConveyorPCGGenerationCoordinator::RequestGeneration(
	AActor* OwnerActor,
	TFunctionRef<void(UPCGComponent* PCGComponent)> BindDelegate)
{
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
			BindDelegate(PCGComponent);
			PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
			PCGComponent->Generate(true);
		}
	}
}
