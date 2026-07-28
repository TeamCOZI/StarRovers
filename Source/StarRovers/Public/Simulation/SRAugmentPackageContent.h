#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityDataAsset.h"
#include "SRAugmentPackageContent.generated.h"

UENUM(BlueprintType)
enum class ESRAugmentChoiceKind : uint8
{
	LegacyStructure UMETA(DisplayName = "Legacy Facility Unlock"),
	ResourceV2Package UMETA(DisplayName = "Resource V2 Package"),
};

UENUM(BlueprintType)
enum class ESRAugmentPackageRoleV2 : uint8
{
	Enabler UMETA(DisplayName = "Enabler"),
	Engine UMETA(DisplayName = "Engine"),
	Payoff UMETA(DisplayName = "Payoff"),
	Pivot UMETA(DisplayName = "Pivot"),
	MacroDoctrine UMETA(DisplayName = "Macro Doctrine"),
	Capstone UMETA(DisplayName = "Capstone"),
};

UENUM(BlueprintType)
enum class ESRAugmentOfferRoleV2 : uint8
{
	Legacy UMETA(DisplayName = "Legacy"),
	Immediate UMETA(DisplayName = "Immediate"),
	Synergy UMETA(DisplayName = "Synergy"),
	Pivot UMETA(DisplayName = "Pivot"),
	Capstone UMETA(DisplayName = "Capstone"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRAugmentPackageDefinitionV2
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2")
	FName PackageId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2")
	FName StrategyId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2")
	ESRAugmentPackageRoleV2 PackageRole = ESRAugmentPackageRoleV2::Enabler;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2")
	ESRFacilityRarity Rarity = ESRFacilityRarity::Basic;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Eligibility")
	TArray<ESRResourceFamily> CompatibleFamilies;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Eligibility")
	TArray<ESRResourceSpectrum> CompatibleSpectra;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Eligibility")
	bool bRequiresAllCompatibleSpectra = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Eligibility")
	TArray<int32> RequiredGrades;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Eligibility")
	TArray<FName> RequiredPackageIds;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Eligibility")
	TArray<FName> RequiredFacilityContentIds;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Eligibility")
	int32 MinimumHubEndpointCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Grants")
	TArray<FName> GrantedProcessTagIds;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Grants")
	TArray<FName> GrantedFuelImprintIds;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Grants")
	TArray<FName> GrantedFacilityContentIds;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Grants")
	TArray<FName> GrantedLogisticsModuleIds;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Grants")
	TArray<FName> GrantedRouteProfileIds;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Doctrine")
	FName DoctrineExclusionGroup = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2")
	FText ExampleLinePreview;

	bool IsMacroDoctrine() const
	{
		return PackageRole == ESRAugmentPackageRoleV2::MacroDoctrine;
	}
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRAugmentBuildContextV2
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment V2|Context")
	TArray<ESRResourceFamily> AccessibleFamilies;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment V2|Context")
	TArray<ESRResourceSpectrum> AccessibleSpectra;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment V2|Context")
	TArray<int32> AccessibleGrades;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment V2|Context")
	TArray<FName> AvailableFacilityContentIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment V2|Context")
	TArray<FName> SelectedPackageIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment V2|Context")
	FName ActiveMacroDoctrineId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment V2|Context", meta = (ClampMin = "0"))
	int32 HubEndpointCount = 0;
};

/**
 * Authoritative, read-only explanation of one Package eligibility check.
 * Offer generation and UI presentation consume this same report so a card
 * cannot advertise a prerequisite as ready while simulation rejects it.
 */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRAugmentPackageEligibilityReportV2
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Eligibility")
	bool bPackageSelectionReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Eligibility")
	bool bRequiredPackagesReady = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Eligibility")
	bool bRequiredFacilitiesReady = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Eligibility")
	bool bRequiredGradesReady = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Eligibility")
	bool bCompatibleFamilyReady = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Eligibility")
	bool bCompatibleSpectrumReady = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Eligibility")
	bool bHubNetworkReady = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Eligibility")
	bool bDoctrineSlotReady = true;

	// Selection-time guard: at least one grant must still be unavailable. This is
	// not Run-fit evidence, so it is intentionally excluded from READY n/n.
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Eligibility")
	bool bNovelGrantReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Eligibility")
	bool bEligible = false;

	// Counts only authored prerequisite groups. Package validity and stale
	// selection state are guards, not Run-fit evidence.
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Eligibility")
	int32 SatisfiedRequirementGroupCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Eligibility")
	int32 TotalRequirementGroupCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Eligibility")
	FString FailureReason;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRAugmentOfferGenerationRulesV2
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment V2|Offer", meta = (ClampMin = "1"))
	int32 ChoiceCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment V2|Offer", meta = (ClampMin = "0.0"))
	float BasicWeight = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment V2|Offer", meta = (ClampMin = "0.0"))
	float AdvancedWeight = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment V2|Offer", meta = (ClampMin = "0.0"))
	float HighTechWeight = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment V2|Offer")
	int32 RandomSeed = 47219;

	// Soft exclusion. Recently rejected cards stay out while equally valid fresh
	// choices exist, but can return when filtering them would shrink the offer.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment V2|Offer")
	TArray<FName> RecentlyOfferedPackageIds;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRAugmentPackageOfferV2
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Offer")
	FName PackageId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Offer")
	ESRAugmentOfferRoleV2 OfferRole = ESRAugmentOfferRoleV2::Immediate;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2|Offer")
	bool bImmediatelyUsable = false;
};

class STARROVERS_API FSRAugmentPackageContentV2 final
{
public:
	static void GetAllDefinitions(TArray<FSRAugmentPackageDefinitionV2>& OutDefinitions);
	static bool TryGetDefinition(FName PackageId, FSRAugmentPackageDefinitionV2& OutDefinition);
	static bool ValidateCatalog(FString& OutFailureReason);

	static bool IsDefinitionEligible(
		const FSRAugmentPackageDefinitionV2& Definition,
		const FSRAugmentBuildContextV2& Context,
		FString* OutFailureReason = nullptr);
	static FSRAugmentPackageEligibilityReportV2 EvaluateEligibility(
		const FSRAugmentPackageDefinitionV2& Definition,
		const FSRAugmentBuildContextV2& Context);
	static void BuildEligibleDefinitions(
		const FSRAugmentBuildContextV2& Context,
		TArray<FSRAugmentPackageDefinitionV2>& OutDefinitions);
	static void GenerateOffer(
		const FSRAugmentBuildContextV2& Context,
		const FSRAugmentOfferGenerationRulesV2& Rules,
		TArray<FSRAugmentPackageOfferV2>& OutOffers);

	static void GetTechnologyFacilityContentIds(TArray<FName>& OutContentIds);
	static void GetTechnologyProcessTagIds(TArray<FName>& OutTagIds);
	static bool IsProcessTagRecipeUnlocked(FName TagId, const TArray<FName>& SelectedPackageIds);
	static bool IsFuelImprintRecipeUnlocked(FName ImprintId, const TArray<FName>& SelectedPackageIds);
	static bool IsFacilityContentUnlocked(FName ContentId, const TArray<FName>& SelectedPackageIds);
	static bool IsLogisticsModuleUnlocked(FName ModuleId, const TArray<FName>& SelectedPackageIds);
	static bool IsRouteProfileUnlocked(FName ProfileId, const TArray<FName>& SelectedPackageIds);
	static FName ResolveActiveMacroDoctrineId(const TArray<FName>& SelectedPackageIds);
	static FString BuildGrantSummary(const FSRAugmentPackageDefinitionV2& Definition);
};
