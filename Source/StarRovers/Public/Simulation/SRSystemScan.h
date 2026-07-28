#pragma once

#include "Automation/SRResourceDataAsset.h"
#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRSystemScan.generated.h"

class AActor;
class USRResourceDataAsset;

/** Visible reasons behind one start-system recommendation. The five parts sum to 100. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRSystemScanScoreBreakdown
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	int32 ResourceQuality = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	int32 StarProximity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	int32 CapacityHeadroom = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	int32 FamilyReadiness = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	int32 BuildAccess = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	int32 TotalScore = 0;
};

/** One concrete body + deposit candidate produced by the initial system scan. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRSystemScanCandidate
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	TObjectPtr<AActor> BodyActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	FText BodyDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	FName DepositOccupantId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	FSRPlanetSurfaceGridCellId DepositCellId;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	TObjectPtr<USRResourceDataAsset> ResourceDataAsset = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	FName ResourceId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	FText ResourceDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	ESRResourceFamily Family = ESRResourceFamily::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	ESRResourceSpectrum Spectrum = ESRResourceSpectrum::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	int32 Grade = StarRovers::Resources::MinimumGrade;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	double SeedEnergy = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	float DistanceToPrimaryStar = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	int32 OperationalLoad = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	int32 OperationalCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	int32 OperationalHeadroom = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	int32 DepositTotalAmount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	int32 DepositRemainingAmount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	bool bHasFamilyProcessorAccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	bool bHasAdjacentBuildAccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	float ResourceQualityNormalized = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	float StarProximityNormalized = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	float CapacityHeadroomNormalized = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	FSRSystemScanScoreBreakdown Score;

	/** Stable final tie-breaker; it never contributes points. */
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	FName StableCandidateId = NAME_None;

	bool IsViable() const
	{
		return BodyActor.Get() != nullptr
			&& ResourceDataAsset.Get() != nullptr
			&& !DepositOccupantId.IsNone()
			&& Family != ESRResourceFamily::None
			&& DepositRemainingAmount > 0
			&& bHasFamilyProcessorAccess
			&& bHasAdjacentBuildAccess;
	}
};

/** Frozen result of the first complete post-generation scan. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRSystemScanSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	bool bScanComplete = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	int32 ScannedConstructibleBodyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	int32 ScannedCardDepositCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	int32 MineableCardDepositCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	int32 DepletedCardDepositCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	int32 InaccessibleCardDepositCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	int32 ViableCandidateCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	int32 RequiredCardResourceCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	int32 AvailableRequiredCardResourceCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	TArray<FName> MissingRequiredCardResourceIds;

	/** At most three body/resource alternatives, ordered best first. */
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|System Scan")
	TArray<FSRSystemScanCandidate> RankedCandidates;

	bool HasRecommendation() const
	{
		return bScanComplete
			&& !RankedCandidates.IsEmpty()
			&& RankedCandidates[0].IsViable();
	}

	const FSRSystemScanCandidate* GetRecommendedCandidate() const
	{
		return HasRecommendation() ? &RankedCandidates[0] : nullptr;
	}

	bool HasCompleteFuelPortfolio() const
	{
		return bScanComplete
			&& RequiredCardResourceCount > 0
			&& AvailableRequiredCardResourceCount >= RequiredCardResourceCount
			&& MissingRequiredCardResourceIds.IsEmpty();
	}
};

/** One bounded pre-Card recovery. It is not a renewable-resource system. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRInitialProgressRecoverySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Initial Progress Recovery")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Initial Progress Recovery")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Initial Progress Recovery")
	bool bAttempted = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Initial Progress Recovery")
	bool bApplied = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Initial Progress Recovery")
	TObjectPtr<AActor> BodyActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Initial Progress Recovery")
	FName DepositOccupantId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Initial Progress Recovery")
	FName ResourceId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Initial Progress Recovery")
	int32 GrantedCardAmount = 0;
};

/** Pure normalization, weighting, and deterministic ordering shared with tests. */
class STARROVERS_API FSRSystemScanModel final
{
public:
	static constexpr int32 ResourceQualityWeight = 35;
	static constexpr int32 StarProximityWeight = 25;
	static constexpr int32 CapacityHeadroomWeight = 20;
	static constexpr int32 FamilyReadinessWeight = 10;
	static constexpr int32 BuildAccessWeight = 10;
	static constexpr int32 MaximumScore = 100;

	static float NormalizeHigherIsBetter(float Value, float Minimum, float Maximum);
	static float NormalizeLowerIsBetter(float Value, float Minimum, float Maximum);
	static FSRSystemScanScoreBreakdown ScoreCandidate(
		float ResourceQualityNormalized,
		float StarProximityNormalized,
		float CapacityHeadroomNormalized,
		bool bHasFamilyProcessorAccess,
		bool bHasAdjacentBuildAccess);
	static bool IsCandidatePreferred(
		const FSRSystemScanCandidate& Left,
		const FSRSystemScanCandidate& Right);
	static void SortCandidates(TArray<FSRSystemScanCandidate>& Candidates);
};
