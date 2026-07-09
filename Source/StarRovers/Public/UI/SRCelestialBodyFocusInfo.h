#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityDataAsset.h"
#include "Celestial/SRStellarEvolutionTypes.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRCelestialBodyFocusInfo.generated.h"

class AActor;
class USRStructureDataAsset;

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFocusedFacilityPortInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "PortKind"))
	ESRStructurePortKind PortKind = ESRStructurePortKind::Input;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "Direction"))
	ESRStructurePortDirection Direction = ESRStructurePortDirection::Left;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "FootprintCellX"))
	int32 FootprintCellX = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "FootprintCellY"))
	int32 FootprintCellY = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "FootprintCellId"))
	FSRPlanetSurfaceGridCellId FootprintCellId;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "ConnectionCellIds"))
	TArray<FSRPlanetSurfaceGridCellId> ConnectionCellIds;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFocusedFacilityRuntimeInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bIsValid"))
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "FacilityId"))
	FName FacilityId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "OperationKind"))
	ESRFacilityOperationKind OperationKind = ESRFacilityOperationKind::Process;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "TemperatureState"))
	ESRFacilityTemperatureState TemperatureState = ESRFacilityTemperatureState::Normal;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "ProcessProgressSeconds"))
	float ProcessProgressSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "ProcessSeconds"))
	float ProcessSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bProcessing"))
	bool bProcessing = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "InputInventory"))
	TArray<FSRResourceInstance> InputInventory;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "ProcessingInventory"))
	TArray<FSRResourceInstance> ProcessingInventory;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "OutputInventory"))
	TArray<FSRResourceInstance> OutputInventory;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFocusedStarFuelInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bIsValid"))
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "EvolutionStage"))
	ESRStellarEvolutionStage EvolutionStage = ESRStellarEvolutionStage::MainSequence;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "StoredFuel"))
	double StoredFuel = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "InitialStageFuel"))
	double InitialStageFuel = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "InitialFuelDecreasePerSecond"))
	double InitialFuelDecreasePerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "CurrentFuelDecreasePerSecond"))
	double RequiredFuelPerCycle = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "NextFuelDecreaseMultiplier"))
	double RequirementGrowthPerCycle = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "LastFuelDecreaseRateCycleIndex"))
	int32 LastFuelDecreaseRateCycleIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "RedGiantPressure"))
	double RedGiantPressure = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "RedGiantPressurePerMissingFuel"))
	double RedGiantPressurePerMissingFuel = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "LastSettledSecondIndex"))
	int32 LastSettledSecondIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "LastSecondFuelConsumed"))
	double LastSecondFuelConsumed = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "LastSecondFuelDecrease"))
	double LastSecondFuelDecrease = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "LastSecondFuelDeficit"))
	double LastSecondFuelDeficit = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bLastSecondSurvived"))
	bool bLastSecondSurvived = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bSupernovaGameOver"))
	bool bSupernovaGameOver = false;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFocusedSurfaceStructureInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bIsValid"))
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "OccupantId"))
	FName OccupantId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "StructureId"))
	FName StructureId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "Description"))
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "ClickedCellInfo"))
	FSRPlanetSurfaceGridCellInfo ClickedCellInfo;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "OriginCellId"))
	FSRPlanetSurfaceGridCellId OriginCellId;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "FootprintCellIds"))
	TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "StructureDataAsset"))
	TObjectPtr<USRStructureDataAsset> StructureDataAsset = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "BuildKind"))
	ESRStructureBuildKind BuildKind = ESRStructureBuildKind::Structure;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bNaturalStructure"))
	bool bNaturalStructure = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bHasFacilityDataAsset"))
	bool bHasFacilityDataAsset = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "FacilityPorts"))
	TArray<FSRFocusedFacilityPortInfo> FacilityPorts;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bHasFacilityRuntimeInfo"))
	bool bHasFacilityRuntimeInfo = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "FacilityRuntimeInfo"))
	FSRFocusedFacilityRuntimeInfo FacilityRuntimeInfo;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRCelestialBodyFocusInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "Actor"))
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "VariableName"))
	FText VariableName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bCanConstruct"))
	bool bCanConstruct = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bHasSurfaceGrid"))
	bool bHasSurfaceGrid = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bHasHoveredSurfaceCell"))
	bool bHasHoveredSurfaceCell = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "HoveredSurfaceCellInfo"))
	FSRPlanetSurfaceGridCellInfo HoveredSurfaceCellInfo;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "HoveredSurfaceGridPatchCellIds"))
	TArray<FSRPlanetSurfaceGridCellId> HoveredSurfaceGridPatchCellIds;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bHasSelectedSurfaceStructure"))
	bool bHasSelectedSurfaceStructure = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "SelectedSurfaceStructureInfo"))
	FSRFocusedSurfaceStructureInfo SelectedSurfaceStructureInfo;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bHasStarFuelInfo"))
	bool bHasStarFuelInfo = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "StarFuelInfo"))
	FSRFocusedStarFuelInfo StarFuelInfo;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bIsValid"))
	bool bIsValid = false;
};
