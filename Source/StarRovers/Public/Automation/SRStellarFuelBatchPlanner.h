#pragma once

#include "CoreMinimal.h"
#include "Automation/SRStellarFuelTypes.h"

struct FSRFacilityInstance;

enum class ESRStellarFuelBatchStateV2 : uint8
{
	Empty,
	Collecting,
	Ready,
	Reserved,
	Contaminated,
};

struct STARROVERS_API FSRStellarFuelBatchSlotV2
{
	int32 LaneIndex = INDEX_NONE;
	bool bOccupied = false;
	bool bValidCard = false;
	bool bDuplicateCardKey = false;
	FSRResourceInstance Resource;
	ESRStellarFuelFabricationOutcomeV2 ValidationOutcome =
		ESRStellarFuelFabricationOutcomeV2::Success;
	FString FailureReason;
};

/** Pure snapshot used by runtime admission tests and the Facility Inspector. */
struct STARROVERS_API FSRStellarFuelBatchStatusV2
{
	ESRStellarFuelBatchStateV2 State = ESRStellarFuelBatchStateV2::Empty;
	bool bUsesReservedInputs = false;
	bool bHasCurrentPattern = false;
	bool bRankedHand = false;
	bool bHasFinalPreview = false;
	int32 RequiredCardCount = StarRovers::StellarFuel::RequiredCardCount;
	int32 OccupiedLaneCount = 0;
	int32 ValidCardCount = 0;
	int32 UniqueCardKeyCount = 0;
	int32 DuplicateCardKeyCount = 0;
	double InputEnergySum = 0.0;
	ESRStellarFuelHandV2 Hand = ESRStellarFuelHandV2::Unranked;
	TArray<int32> EmptyLaneIndices;
	TArray<int32> ContaminatedLaneIndices;
	TArray<FSRStellarFuelBatchSlotV2> Slots;
	FSRStellarFuelFabricationResultV2 FinalPreview;
	FString Summary;
	FString Detail;
};

class STARROVERS_API FSRStellarFuelBatchPlanner final
{
public:
	// Returns false only when the Facility is not a Resource V2 Stellar Fuel
	// Fabricator. A malformed Fabricator still returns a Contaminated snapshot.
	static bool TryBuildStatus(
		const FSRFacilityInstance& FacilityInstance,
		FName FabricatorBodyId,
		FSRStellarFuelBatchStatusV2& OutStatus);
};
