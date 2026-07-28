#include "Automation/SROperationalCapacity.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SROperationalEconomyProcessor.h"
#include "SRFacilityProcessingRuleEvaluator.h"

namespace
{
	FSROperationalCapacityTierV2& GetTier(
		FSROperationalCapacityReportV2& Report,
		ESROperationalPriorityV2 Priority)
	{
		switch (Priority)
		{
		case ESROperationalPriorityV2::Critical: return Report.Critical;
		case ESROperationalPriorityV2::Background: return Report.Background;
		case ESROperationalPriorityV2::Normal:
		default: return Report.Normal;
		}
	}

	void AllocateTier(FSROperationalCapacityTierV2& Tier, float& RemainingCapacity)
	{
		if (Tier.Demand <= 0)
		{
			Tier.AllocatedCapacity = 0.0f;
			Tier.SpeedFactor = 1.0f;
			return;
		}
		Tier.AllocatedCapacity = FMath::Min(RemainingCapacity, static_cast<float>(Tier.Demand));
		Tier.SpeedFactor = FMath::Clamp(
			Tier.AllocatedCapacity / static_cast<float>(Tier.Demand),
			0.0f,
			1.0f);
		RemainingCapacity = FMath::Max(0.0f, RemainingCapacity - Tier.AllocatedCapacity);
	}
}

FSROperationalCapacityReportV2 FSROperationalCapacity::BuildReport(
	const FSRFacilityNetworkRuntimeState& RuntimeState,
	bool bRulesActive,
	int32 BaseCapacity,
	int32 ServiceCoreCapacity,
	int32 AugmentCapacity)
{
	FSROperationalCapacityReportV2 Report;
	Report.bRulesActive = bRulesActive;
	Report.BaseCapacity = FMath::Max(0, BaseCapacity);
	Report.AugmentCapacity = FMath::Max(0, AugmentCapacity);

	for (const TPair<FName, FSRFacilityInstance>& FacilityPair : RuntimeState.FacilityInstancesByOccupantId)
	{
		const FSRFacilityInstance& Facility = FacilityPair.Value;
		if (bRulesActive && FSROperationalEconomyProcessor::IsServiceCoreSupplied(Facility))
		{
			++Report.ActiveServiceCoreCount;
		}

		const USRFacilityDataAsset* Definition = Facility.FacilityDataAsset.Get();
		if (!IsValid(Definition)
			|| Definition->OperationalLoad <= 0
			|| !Facility.bProcessing
			|| !FSRFacilityProcessingRuleEvaluator::CanAdvanceProcessing(Facility)
			|| Facility.ProcessProgressSeconds
				>= FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(Facility))
		{
			continue;
		}
		GetTier(Report, Facility.OperationalPriority).Demand += Definition->OperationalLoad;
		Report.TotalDemand += Definition->OperationalLoad;
	}

	Report.ServiceCoreCapacity = Report.ActiveServiceCoreCount * FMath::Max(0, ServiceCoreCapacity);
	Report.TotalCapacity = Report.BaseCapacity + Report.ServiceCoreCapacity + Report.AugmentCapacity;
	Report.RemainingCapacity = static_cast<float>(Report.TotalCapacity);
	if (!bRulesActive)
	{
		Report.Critical.AllocatedCapacity = static_cast<float>(Report.Critical.Demand);
		Report.Normal.AllocatedCapacity = static_cast<float>(Report.Normal.Demand);
		Report.Background.AllocatedCapacity = static_cast<float>(Report.Background.Demand);
		Report.Critical.SpeedFactor = 1.0f;
		Report.Normal.SpeedFactor = 1.0f;
		Report.Background.SpeedFactor = 1.0f;
		Report.RemainingCapacity = FMath::Max(
			0.0f,
			static_cast<float>(Report.TotalCapacity - Report.TotalDemand));
		return Report;
	}

	AllocateTier(Report.Critical, Report.RemainingCapacity);
	AllocateTier(Report.Normal, Report.RemainingCapacity);
	AllocateTier(Report.Background, Report.RemainingCapacity);
	return Report;
}
