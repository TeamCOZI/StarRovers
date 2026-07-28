#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityDataAsset.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRFacilityRuntimeData.generated.h"

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFacilityPortInventory
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "PortId"))
	FName PortId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "PortKind"))
	ESRFacilityPortKind PortKind = ESRFacilityPortKind::Input;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "PortIndex"))
	int32 PortIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "PortSpec"))
	FSRStructurePortSpec PortSpec;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "Capacity"))
	int32 Capacity = 1;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "Inventory"))
	TArray<FSRResourceInstance> Inventory;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFacilityCellTemperatureAdjustment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "CellId"))
	FSRPlanetSurfaceGridCellId CellId;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "TemperatureStepDelta"))
	int32 TemperatureStepDelta = 0;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFacilityInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "OccupantId"))
	FName OccupantId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "StructureDataAsset"))
	TObjectPtr<USRStructureDataAsset> StructureDataAsset = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "FacilityDataAsset"))
	TObjectPtr<USRFacilityDataAsset> FacilityDataAsset = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "OriginCellId"))
	FSRPlanetSurfaceGridCellId OriginCellId;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "FootprintCellIds"))
	TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "PlacementRotationSteps"))
	int32 PlacementRotationSteps = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "InputPortInventories"))
	TArray<FSRFacilityPortInventory> InputPortInventories;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "OutputPortInventories"))
	TArray<FSRFacilityPortInventory> OutputPortInventories;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "InputInventory"))
	TArray<FSRResourceInstance> InputInventory;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "OutputInventory"))
	TArray<FSRResourceInstance> OutputInventory;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility|Hub", meta = (DisplayName = "StarFuelMissileAutoLaunchInputPortIndices"))
	TArray<int32> StarFuelMissileAutoLaunchInputPortIndices;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "CellTemperatureAdjustments"))
	TArray<FSRFacilityCellTemperatureAdjustment> CellTemperatureAdjustments;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "ProcessingInventory"))
	TArray<FSRResourceInstance> ProcessingInventory;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility|Operational Capacity", meta = (DisplayName = "OperationalPriority"))
	ESROperationalPriorityV2 OperationalPriority = ESROperationalPriorityV2::Normal;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility|Operational Capacity", meta = (DisplayName = "OperationalSpeedFactor"))
	float OperationalSpeedFactor = 1.0f;

	// NAME_None uses the Facility Data Asset default. Runtime selection lives on
	// the placed Facility so one authored Imprinter can serve every unlocked recipe.
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility|Resource V2 Recipe", meta = (DisplayName = "SelectedProcessTagRecipeId"))
	FName SelectedProcessTagRecipeId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility|Resource V2 Recipe", meta = (DisplayName = "SelectedFuelImprintRecipeId"))
	FName SelectedFuelImprintRecipeId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "MiningTargetDepositOccupantId"))
	FName MiningTargetDepositOccupantId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "TemperatureState"))
	ESRFacilityTemperatureState TemperatureState = ESRFacilityTemperatureState::Normal;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "ProcessProgressSeconds"))
	float ProcessProgressSeconds = 0.0f;

	// Captured after inputs are reserved. Runtime settings and authored balance
	// edits cannot move the finish line of an already-started operation.
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "ResolvedProcessSeconds"))
	float ResolvedProcessSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "bHasResolvedProcessSeconds"))
	bool bHasResolvedProcessSeconds = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "bProcessing"))
	bool bProcessing = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "bProcessEnabled"))
	bool bProcessEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "bDeliverEnabled"))
	bool bDeliverEnabled = true;
};
