#include "Automation/SRFacilityNetworkComponent.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SROperationalCapacity.h"
#include "Simulation/SRSimulationSettings.h"
#include "SRFacilityProcessingRuleEvaluator.h"

bool USRFacilityNetworkComponent::SetFacilityOperationalPriority(
	FName OccupantId,
	ESROperationalPriorityV2 Priority)
{
	FSRFacilityInstance* Facility = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!Facility)
	{
		return false;
	}
	switch (Priority)
	{
	case ESROperationalPriorityV2::Critical:
	case ESROperationalPriorityV2::Normal:
	case ESROperationalPriorityV2::Background:
		Facility->OperationalPriority = Priority;
		RefreshOperationalCapacity();
		return true;
	default:
		return false;
	}
}

FSROperationalCapacityReportV2 USRFacilityNetworkComponent::GetOperationalCapacityReport() const
{
	return RuntimeState.OperationalCapacityReport;
}

FSROperationalFacilityStatusCountsV2
USRFacilityNetworkComponent::GetOperationalFacilityStatusCounts() const
{
	FSROperationalFacilityStatusCountsV2 Counts;
	Counts.RegisteredFacilityCount = RuntimeState.FacilityInstancesByOccupantId.Num();
	for (const TPair<FName, FSRFacilityInstance>& FacilityPair :
		RuntimeState.FacilityInstancesByOccupantId)
	{
		const FSRFacilityInstance& Facility = FacilityPair.Value;
		Counts.EnabledFacilityCount += Facility.bProcessEnabled ? 1 : 0;
		Counts.ProcessingFacilityCount += Facility.bProcessing ? 1 : 0;
		const USRFacilityDataAsset* Definition = Facility.FacilityDataAsset.Get();
		if (Facility.bProcessing
			&& IsValid(Definition)
			&& Definition->OperationalLoad > 0
			&& Facility.OperationalSpeedFactor < 1.0f - KINDA_SMALL_NUMBER)
		{
			++Counts.ThrottledFacilityCount;
		}
	}
	return Counts;
}

FSROperationalCapacityReportV2 USRFacilityNetworkComponent::RefreshOperationalCapacity()
{
	const USRSimulationSettings* Settings = GetDefault<USRSimulationSettings>();
	const bool bRulesActive = IsValid(Settings)
		&& Settings->ResourceRulesetVersion == ESRResourceRulesetVersion::ResourceV2;
	const int32 BaseCapacity = IsValid(Settings) ? Settings->BaseOperationalCapacityV2 : 30;
	const int32 ServiceCoreCapacity = IsValid(Settings)
		? Settings->ServiceCoreOperationalCapacityV2
		: 18;
	RuntimeState.OperationalCapacityReport = FSROperationalCapacity::BuildReport(
		RuntimeState,
		bRulesActive,
		BaseCapacity,
		ServiceCoreCapacity);

	for (TPair<FName, FSRFacilityInstance>& FacilityPair : RuntimeState.FacilityInstancesByOccupantId)
	{
		FSRFacilityInstance& Facility = FacilityPair.Value;
		const USRFacilityDataAsset* Definition = Facility.FacilityDataAsset.Get();
		Facility.OperationalSpeedFactor = bRulesActive
			&& IsValid(Definition)
			&& Definition->OperationalLoad > 0
			&& Facility.bProcessing
			&& FSRFacilityProcessingRuleEvaluator::CanAdvanceProcessing(Facility)
			&& Facility.ProcessProgressSeconds
				< FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(Facility)
			? RuntimeState.OperationalCapacityReport.GetSpeedFactor(Facility.OperationalPriority)
			: 1.0f;
	}
	return RuntimeState.OperationalCapacityReport;
}
