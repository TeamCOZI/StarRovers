#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityDataAsset.h"
#include "Pattern/SRPatternEnvironmentResolver.h"
#include "Simulation/SRRunModifierTypes.h"
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

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "ProcessingInventory"))
	TArray<FSRResourceInstance> ProcessingInventory;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "MiningTargetDepositOccupantId"))
	FName MiningTargetDepositOccupantId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "TemperatureState"))
	ESRFacilityTemperatureState TemperatureState = ESRFacilityTemperatureState::Normal;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility|Pattern", meta = (DisplayName = "PatternEnvironment"))
	FSRPatternEnvironmentSpec PatternEnvironment;

	// Snapshotted when a batch starts. Runtime completion and its preview use the
	// same revision even if a Technology, Augment, or Trial changes mid-process.
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility|Run Modifier", meta = (DisplayName = "RunModifierContext"))
	FSRRunModifierContext RunModifierContext;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "ProcessProgressSeconds"))
	float ProcessProgressSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "bProcessing"))
	bool bProcessing = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "bProcessEnabled"))
	bool bProcessEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "bDeliverEnabled"))
	bool bDeliverEnabled = true;
};

namespace StarRovers::Save::FacilityNetwork
{
	inline constexpr int32 CurrentVersion = 2;
	inline bool IsSupportedVersion(int32 Version) { return Version == CurrentVersion; }
}

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFacilityNetworkSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Facility")
	int32 Version = StarRovers::Save::FacilityNetwork::CurrentVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Facility")
	TArray<FSRFacilityInstance> Facilities;
};
