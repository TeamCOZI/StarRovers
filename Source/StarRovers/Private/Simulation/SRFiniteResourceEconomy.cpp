#include "Simulation/SRFiniteResourceEconomy.h"

#include "Automation/SRResourceSystemContent.h"
#include "Automation/SRStellarFuelFabricator.h"

namespace
{
	bool EvaluateReferenceFuel(
		bool bOptimized,
		double& OutFuelEnergy,
		FString& OutFailureReason)
	{
		OutFuelEnergy = 0.0;
		TArray<FSRResourceInstance> Cards;
		if (!FSRResourceSystemContent::MakeReferenceStellarFuelBatch(
			ESRStellarFuelReferenceTopologyV2::DistributedConvergence,
			FName(TEXT("FiniteEconomyFoundry")),
			Cards))
		{
			OutFailureReason = TEXT("Reference Card batch could not be built.");
			return false;
		}
		if (!bOptimized)
		{
			for (FSRResourceInstance& Card : Cards)
			{
				Card.FuelImprintSlot = FSRResourceFuelImprintSlot();
			}
		}

		const FSRStellarFuelFabricationResultV2 Result =
			FSRStellarFuelFabricator::EvaluateCards(
				Cards,
				FSRStellarFuelFabricationRulesV2(),
				FName(TEXT("FiniteEconomyFoundry")));
		if (!Result.IsSuccess() || !FMath::IsFinite(Result.FuelEnergy) || Result.FuelEnergy <= 0.0)
		{
			OutFailureReason = FString::Printf(
				TEXT("Reference Fuel evaluation failed: %s"),
				*Result.FailureReason);
			return false;
		}
		OutFuelEnergy = Result.FuelEnergy;
		return true;
	}
}

FSRFiniteResourceEconomyContract
FSRFiniteResourceEconomyModel::BuildReferenceContract()
{
	FSRFiniteResourceEconomyContract Contract;
	TArray<FSRReferenceResourceDefinitionV2> CardDefinitions;
	FSRResourceSystemContent::GetAllReferenceResourceDefinitions(CardDefinitions);
	Contract.RequiredCardTypeCount = CardDefinitions.Num();
	if (CardDefinitions.IsEmpty())
	{
		Contract.FailureReason = TEXT("Reference Card catalog is empty.");
		return Contract;
	}

	Contract.BatchesPerCardDepositSet = CardDefinitions[0].DepositTotalAmount;
	for (const FSRReferenceResourceDefinitionV2& Definition : CardDefinitions)
	{
		if (Definition.DepositTotalAmount <= 0
			|| Definition.DepositTotalAmount != Contract.BatchesPerCardDepositSet)
		{
			Contract.FailureReason = TEXT("Reference Card deposits do not share one synchronized batch budget.");
			return Contract;
		}
	}

	FSRUtilityResourceDefinitionV2 CommonOre;
	FSRUtilityResourceDefinitionV2 Biomass;
	if (!FSRResourceSystemContent::TryGetUtilityResourceDefinition(
			ESRResourceContentPresetV2::CommonOre,
			CommonOre)
		|| !FSRResourceSystemContent::TryGetUtilityResourceDefinition(
			ESRResourceContentPresetV2::BiomassFeedstock,
			Biomass)
		|| CommonOre.DepositTotalAmount <= 0
		|| CommonOre.DepositTotalAmount != Biomass.DepositTotalAmount)
	{
		Contract.FailureReason = TEXT("Raw utility deposits do not share one supply-cycle budget.");
		return Contract;
	}
	Contract.RawUnitsPerUtilityDeposit = CommonOre.DepositTotalAmount;

	FSRFacilityContentDefinitionV2 Fabricator;
	if (!FSRResourceSystemContent::TryGetFacilityDefinition(
			ESRFacilityContentPresetV2::StellarFuelFabricator,
			Fabricator)
		|| !FMath::IsFinite(Fabricator.CycleSeconds)
		|| Fabricator.CycleSeconds <= 0.0f)
	{
		Contract.FailureReason = TEXT("Stellar Fuel Fabricator cycle is invalid.");
		return Contract;
	}
	Contract.FabricationCycleSeconds = Fabricator.CycleSeconds;

	if (!EvaluateReferenceFuel(
			false,
			Contract.BasicFuelEnergyPerBatch,
			Contract.FailureReason)
		|| !EvaluateReferenceFuel(
			true,
			Contract.OptimizedFuelEnergyPerBatch,
			Contract.FailureReason))
	{
		return Contract;
	}
	Contract.BasicFuelPerSecond = Contract.BasicFuelEnergyPerBatch
		/ Contract.FabricationCycleSeconds;
	Contract.OptimizedFuelPerSecond = Contract.OptimizedFuelEnergyPerBatch
		/ Contract.FabricationCycleSeconds;
	Contract.bIsValid = true;
	return Contract;
}

bool FSRFiniteResourceEconomyModel::BuildReferenceSupplyStage(
	bool bOptimized,
	int32 CardDepositSetCount,
	double StartTimeSeconds,
	double TransitDelaySeconds,
	FSRRunBalanceSupplyStage& OutStage,
	FString& OutFailureReason)
{
	OutStage = FSRRunBalanceSupplyStage();
	OutFailureReason.Reset();
	const FSRFiniteResourceEconomyContract Contract = BuildReferenceContract();
	if (!Contract.bIsValid)
	{
		OutFailureReason = Contract.FailureReason;
		return false;
	}
	if (CardDepositSetCount <= 0)
	{
		OutFailureReason = TEXT("At least one complete five-Card deposit set is required.");
		return false;
	}

	OutStage.StartTimeSeconds = FMath::Max(0.0, StartTimeSeconds);
	OutStage.TransitDelaySeconds = FMath::Max(0.0, TransitDelaySeconds);
	OutStage.DeliveryIntervalSeconds = Contract.FabricationCycleSeconds;
	OutStage.FuelPerSecond = bOptimized
		? Contract.OptimizedFuelPerSecond
		: Contract.BasicFuelPerSecond;
	const int64 MaximumDeliveries = static_cast<int64>(Contract.BatchesPerCardDepositSet)
		* static_cast<int64>(CardDepositSetCount);
	OutStage.MaximumDeliveryCount = static_cast<int32>(FMath::Min<int64>(
		MaximumDeliveries,
		MAX_int32));
	return true;
}
