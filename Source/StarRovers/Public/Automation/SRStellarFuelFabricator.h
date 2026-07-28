#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityRuntimeData.h"
#include "Automation/SRStellarFuelTypes.h"

class STARROVERS_API FSRStellarFuelFabricator final
{
public:
	static bool IsResourceV2RulesetActive();

	// Only an explicitly-authored V2 Stellar Fuel Fabricator uses this route.
	// Other Synthesize facilities retain their Legacy transition path.
	static bool ShouldRouteThroughResourceV2(const USRFacilityDataAsset* FacilityDataAsset);

	static bool ValidateFacilityDefinition(
		const USRFacilityDataAsset* FacilityDataAsset,
		FString& OutFailureReason);

	// Admission-safe single Card validation shared by the Fabricator evaluator,
	// direct inventory transfers, and conveyor transfers. This never mutates the
	// resource, so a rejected Card remains at its source.
	static bool ValidateInputCard(
		const FSRResourceInstance& InputCard,
		FString& OutFailureReason,
		ESRStellarFuelFabricationOutcomeV2* OutFailureOutcome = nullptr);

	// Pure, deterministic and preview-safe. Every input Energy is counted, while
	// duplicate Spectrum+Grade Card Keys count once for hand recognition.
	static FSRStellarFuelFabricationResultV2 EvaluateCards(
		const TArray<FSRResourceInstance>& InputCards,
		const FSRStellarFuelFabricationRulesV2& Rules,
		FName FabricatorBodyId = NAME_None);

	static FSRStellarFuelFabricationResultV2 Evaluate(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& InputCards,
		FName FabricatorBodyId = NAME_None);

	static FString BuildPreviewSummary(const FSRStellarFuelFabricationResultV2& Result);
};
