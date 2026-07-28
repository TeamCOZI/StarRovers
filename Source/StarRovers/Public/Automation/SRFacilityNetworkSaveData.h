#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityRuntimeData.h"
#include "SRFacilityNetworkSaveData.generated.h"

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFacilityInstanceSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	FName OccupantId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	TSoftObjectPtr<USRStructureDataAsset> StructureDataAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	TSoftObjectPtr<USRFacilityDataAsset> FacilityDataAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	FSRPlanetSurfaceGridCellId OriginCellId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	int32 PlacementRotationSteps = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	TArray<FSRFacilityPortInventory> InputPortInventories;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	TArray<FSRFacilityPortInventory> OutputPortInventories;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	TArray<int32> StarFuelMissileAutoLaunchInputPortIndices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	TArray<FSRResourceInstance> ProcessingInventory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	ESROperationalPriorityV2 OperationalPriority = ESROperationalPriorityV2::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	FName SelectedProcessTagRecipeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	FName SelectedFuelImprintRecipeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	FName MiningTargetDepositOccupantId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	ESRFacilityTemperatureState TemperatureState = ESRFacilityTemperatureState::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	float ProcessProgressSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	float ResolvedProcessSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	bool bHasResolvedProcessSeconds = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	bool bProcessing = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	bool bProcessEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	bool bDeliverEnabled = true;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFacilityNetworkSaveData
{
	GENERATED_BODY()

	static constexpr int32 InitialVersion = 1;
	static constexpr int32 CurrentVersion = InitialVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	int32 Version = CurrentVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	FName NextFacilitySchedulerOccupantId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Save")
	TArray<FSRFacilityInstanceSaveData> Facilities;

	bool IsSupportedVersion() const
	{
		return Version >= InitialVersion && Version <= CurrentVersion;
	}
};
