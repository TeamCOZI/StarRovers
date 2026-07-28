#pragma once

#include "CoreMinimal.h"
#include "Automation/SRResourceDataAsset.h"
#include "SRStellarFuelTypes.generated.h"

namespace StarRovers::StellarFuel
{
	inline constexpr int32 RequiredCardCount = 5;
}

UENUM(BlueprintType)
enum class ESRStellarFuelHandV2 : uint8
{
	Unranked UMETA(DisplayName = "Unranked"),
	OnePair UMETA(DisplayName = "One Pair"),
	TwoPair UMETA(DisplayName = "Two Pair"),
	ThreeOfAKind UMETA(DisplayName = "Three of a Kind"),
	FiveGradeSequence UMETA(DisplayName = "Five Grade Sequence"),
	FullHouse UMETA(DisplayName = "Full House"),
	FourOfAKind UMETA(DisplayName = "Four of a Kind"),
};

UENUM(BlueprintType)
enum class ESRStellarFuelFabricationOutcomeV2 : uint8
{
	Success UMETA(DisplayName = "Success"),
	InvalidRules UMETA(DisplayName = "Invalid Rules"),
	WrongCardCount UMETA(DisplayName = "Wrong Card Count"),
	InvalidCard UMETA(DisplayName = "Invalid Card"),
	UnsupportedSchema UMETA(DisplayName = "Unsupported Schema"),
	InvalidEnergy UMETA(DisplayName = "Invalid Energy"),
	InvalidFuelImprint UMETA(DisplayName = "Invalid Fuel Imprint"),
	NonFiniteResult UMETA(DisplayName = "Non-finite Result"),
};

UENUM(BlueprintType)
enum class ESRStellarFuelReferenceTopologyV2 : uint8
{
	DistributedConvergence UMETA(DisplayName = "Distributed Convergence"),
	CentralFoundry UMETA(DisplayName = "Central Foundry"),
	PilgrimCircuit UMETA(DisplayName = "Pilgrim Circuit"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStellarFuelHandBonusV2
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Fuel|Hand")
	double B = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Fuel|Hand")
	double C = 0.0;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStellarFuelFabricationRulesV2
{
	GENERATED_BODY()

	FSRStellarFuelFabricationRulesV2();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Fuel|Formula")
	FName OutputResourceId = FName(TEXT("StellarFuel"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Fuel|Formula")
	double BaseEnergyA = 0.0;

	// The non-Full-House values are prototype defaults and remain authorable.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Fuel|Hand")
	FSRStellarFuelHandBonusV2 OnePairBonus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Fuel|Hand")
	FSRStellarFuelHandBonusV2 TwoPairBonus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Fuel|Hand")
	FSRStellarFuelHandBonusV2 ThreeOfAKindBonus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Fuel|Hand")
	FSRStellarFuelHandBonusV2 FiveGradeSequenceBonus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Fuel|Hand", meta = (
		ToolTip = "Confirmed reference value: Full House adds B +30 and C +3."))
	FSRStellarFuelHandBonusV2 FullHouseBonus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Fuel|Hand")
	FSRStellarFuelHandBonusV2 FourOfAKindBonus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Fuel|Fuel Imprint")
	double TwinSealEnergyB = 6.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Fuel|Fuel Imprint")
	double TopologySealEnergyB = 12.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Fuel|Fuel Imprint")
	double PrismaticCatalystC = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Fuel|Formula")
	bool bClampFinalEnergyAtZero = true;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStellarFuelCardContributionV2
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	int32 InputIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	FName ResourceId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	ESRResourceSpectrum Spectrum = ESRResourceSpectrum::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	int32 Grade = StarRovers::Resources::MinimumGrade;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	double CurrentEnergy = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	FName FuelImprintId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	bool bUniqueCardKey = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	bool bTwinSealContributed = false;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStellarFuelFabricationResultV2
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	ESRStellarFuelFabricationOutcomeV2 Outcome = ESRStellarFuelFabricationOutcomeV2::InvalidRules;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	FString FailureReason;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	TArray<FSRStellarFuelCardContributionV2> CardContributions;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	int32 UniqueCardKeyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	ESRStellarFuelHandV2 Hand = ESRStellarFuelHandV2::Unranked;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	double InputEnergySum = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	double HandEnergyB = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	double HandCatalystC = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	int32 EffectiveTwinSealCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	double TwinSealEnergyB = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	TArray<FName> SatisfiedTopologySealIds;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	FName AppliedTopologySealId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	int32 SuppressedTopologySealCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	double TopologySealEnergyB = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	bool bPrismaticSpectrumConditionMet = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	int32 EffectivePrismaticCatalystCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	int32 SuppressedPrismaticCatalystCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	double PrismaticCatalystC = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	double FormulaA = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	double FormulaB = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	double FormulaC = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	double UnclampedFuelEnergy = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	double ClampEnergyDelta = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	double FuelEnergy = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Fuel")
	FSRResourceInstance OutputFuel;

	bool IsSuccess() const
	{
		return Outcome == ESRStellarFuelFabricationOutcomeV2::Success;
	}
};
