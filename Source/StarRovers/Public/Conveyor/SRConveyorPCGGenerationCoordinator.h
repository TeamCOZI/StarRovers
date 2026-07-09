#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class AActor;
class UPCGComponent;

namespace StarRovers::Conveyor
{
	struct STARROVERS_API FSRConveyorPCGGenerationCoordinator
	{
		static void ConfigureGenerationTriggers(AActor* OwnerActor);

		static void BindGenerationDelegates(
			AActor* OwnerActor,
			TFunctionRef<void(UPCGComponent* PCGComponent)> BindDelegate);

		static void RequestGeneration(
			AActor* OwnerActor,
			TFunctionRef<void(UPCGComponent* PCGComponent)> BindDelegate);
	};
}
