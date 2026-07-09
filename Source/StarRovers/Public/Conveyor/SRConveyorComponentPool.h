#pragma once

#include "CoreMinimal.h"

class AActor;
class UDynamicMeshComponent;
class ULineBatchComponent;
class USceneComponent;
class USplineComponent;

namespace StarRovers::Conveyor
{
	struct STARROVERS_API FSRConveyorComponentPool
	{
		static UDynamicMeshComponent* EnsureBeltMeshComponent(
			AActor* OwnerActor,
			USceneComponent* AttachParent,
			TObjectPtr<UDynamicMeshComponent>& BeltMeshComponent);

		static void ClearBeltMeshComponent(
			UDynamicMeshComponent* BeltMeshComponent,
			bool bHideComponent);

		static ULineBatchComponent* EnsurePathDebugLineBatchComponent(
			AActor* OwnerActor,
			USceneComponent* AttachParent,
			TObjectPtr<ULineBatchComponent>& PathDebugLineBatchComponent);

		static USplineComponent* EnsurePCGSplineComponent(
			AActor* OwnerActor,
			USceneComponent* AttachParent,
			FName PCGSplineComponentTag,
			TArray<TObjectPtr<USplineComponent>>& PCGSplineComponents,
			int32 SplineIndex);

		static void ClearUnusedPCGSplineComponents(
			TArray<TObjectPtr<USplineComponent>>& PCGSplineComponents,
			int32 FirstUnusedSplineIndex);
	};
}
