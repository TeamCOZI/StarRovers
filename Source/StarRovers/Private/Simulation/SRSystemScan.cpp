#include "Simulation/SRSystemScan.h"

float FSRSystemScanModel::NormalizeHigherIsBetter(
	float Value,
	float Minimum,
	float Maximum)
{
	if (!FMath::IsFinite(Value) || !FMath::IsFinite(Minimum) || !FMath::IsFinite(Maximum))
	{
		return 0.0f;
	}
	if (Maximum <= Minimum + KINDA_SMALL_NUMBER)
	{
		return 1.0f;
	}
	return FMath::Clamp((Value - Minimum) / (Maximum - Minimum), 0.0f, 1.0f);
}

float FSRSystemScanModel::NormalizeLowerIsBetter(
	float Value,
	float Minimum,
	float Maximum)
{
	if (!FMath::IsFinite(Value) || !FMath::IsFinite(Minimum) || !FMath::IsFinite(Maximum))
	{
		return 0.0f;
	}
	if (Maximum <= Minimum + KINDA_SMALL_NUMBER)
	{
		return 1.0f;
	}
	return FMath::Clamp((Maximum - Value) / (Maximum - Minimum), 0.0f, 1.0f);
}

FSRSystemScanScoreBreakdown FSRSystemScanModel::ScoreCandidate(
	float ResourceQualityNormalized,
	float StarProximityNormalized,
	float CapacityHeadroomNormalized,
	bool bHasFamilyProcessorAccess,
	bool bHasAdjacentBuildAccess)
{
	FSRSystemScanScoreBreakdown Score;
	Score.ResourceQuality = FMath::RoundToInt(
		FMath::Clamp(ResourceQualityNormalized, 0.0f, 1.0f) * ResourceQualityWeight);
	Score.StarProximity = FMath::RoundToInt(
		FMath::Clamp(StarProximityNormalized, 0.0f, 1.0f) * StarProximityWeight);
	Score.CapacityHeadroom = FMath::RoundToInt(
		FMath::Clamp(CapacityHeadroomNormalized, 0.0f, 1.0f) * CapacityHeadroomWeight);
	Score.FamilyReadiness = bHasFamilyProcessorAccess ? FamilyReadinessWeight : 0;
	Score.BuildAccess = bHasAdjacentBuildAccess ? BuildAccessWeight : 0;
	Score.TotalScore = FMath::Clamp(
		Score.ResourceQuality
			+ Score.StarProximity
			+ Score.CapacityHeadroom
			+ Score.FamilyReadiness
			+ Score.BuildAccess,
		0,
		MaximumScore);
	return Score;
}

bool FSRSystemScanModel::IsCandidatePreferred(
	const FSRSystemScanCandidate& Left,
	const FSRSystemScanCandidate& Right)
{
	if (Left.IsViable() != Right.IsViable())
	{
		return Left.IsViable();
	}
	if (Left.Score.TotalScore != Right.Score.TotalScore)
	{
		return Left.Score.TotalScore > Right.Score.TotalScore;
	}
	if (!FMath::IsNearlyEqual(Left.ResourceQualityNormalized, Right.ResourceQualityNormalized))
	{
		return Left.ResourceQualityNormalized > Right.ResourceQualityNormalized;
	}
	if (!FMath::IsNearlyEqual(Left.StarProximityNormalized, Right.StarProximityNormalized))
	{
		return Left.StarProximityNormalized > Right.StarProximityNormalized;
	}
	if (!FMath::IsNearlyEqual(Left.CapacityHeadroomNormalized, Right.CapacityHeadroomNormalized))
	{
		return Left.CapacityHeadroomNormalized > Right.CapacityHeadroomNormalized;
	}
	if (!FMath::IsNearlyEqual(Left.SeedEnergy, Right.SeedEnergy))
	{
		return Left.SeedEnergy > Right.SeedEnergy;
	}
	if (!FMath::IsNearlyEqual(Left.DistanceToPrimaryStar, Right.DistanceToPrimaryStar))
	{
		return Left.DistanceToPrimaryStar < Right.DistanceToPrimaryStar;
	}
	if (Left.OperationalHeadroom != Right.OperationalHeadroom)
	{
		return Left.OperationalHeadroom > Right.OperationalHeadroom;
	}
	return Left.StableCandidateId.LexicalLess(Right.StableCandidateId);
}

void FSRSystemScanModel::SortCandidates(TArray<FSRSystemScanCandidate>& Candidates)
{
	Candidates.StableSort(
		[](const FSRSystemScanCandidate& Left, const FSRSystemScanCandidate& Right)
		{
			return IsCandidatePreferred(Left, Right);
		});
}
