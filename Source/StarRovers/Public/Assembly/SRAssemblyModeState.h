#pragma once

#include "CoreMinimal.h"
#include "Structure/SRStructureDataAsset.h"
#include "SRAssemblyModeState.generated.h"

USTRUCT()
struct STARROVERS_API FSRAssemblyModeState
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	bool bAssemblyModeActive = false;

	UPROPERTY(Transient)
	int32 StructurePlacementRotationSteps = 0;

	void ResetStructurePlacementRotation()
	{
		StructurePlacementRotationSteps = 0;
	}

	bool RotateStructurePlacement(int32 StepDelta)
	{
		const int32 PreviousRotationSteps = GetStructurePlacementRotationSteps();
		StructurePlacementRotationSteps = StarRovers::Structure::NormalizePlacementRotationSteps(StructurePlacementRotationSteps + StepDelta);
		return StructurePlacementRotationSteps != PreviousRotationSteps;
	}

	int32 GetStructurePlacementRotationSteps() const
	{
		return StarRovers::Structure::NormalizePlacementRotationSteps(StructurePlacementRotationSteps);
	}

	float GetStructurePlacementAdditionalYawDegrees() const
	{
		return StarRovers::Structure::PlacementRotationStepsToYawDegrees(StructurePlacementRotationSteps);
	}
};
