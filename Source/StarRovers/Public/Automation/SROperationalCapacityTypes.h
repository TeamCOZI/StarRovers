#pragma once

#include "CoreMinimal.h"
#include "SROperationalCapacityTypes.generated.h"

UENUM(BlueprintType)
enum class ESROperationalPriorityV2 : uint8
{
	Critical UMETA(DisplayName = "Critical"),
	Normal UMETA(DisplayName = "Normal"),
	Background UMETA(DisplayName = "Background"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSROperationalCapacityTierV2
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Operational Capacity")
	int32 Demand = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Operational Capacity")
	float AllocatedCapacity = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Operational Capacity")
	float SpeedFactor = 1.0f;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSROperationalCapacityReportV2
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Operational Capacity")
	bool bRulesActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Operational Capacity")
	int32 BaseCapacity = 30;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Operational Capacity")
	int32 ActiveServiceCoreCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Operational Capacity")
	int32 ServiceCoreCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Operational Capacity")
	int32 AugmentCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Operational Capacity")
	int32 TotalCapacity = 30;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Operational Capacity")
	int32 TotalDemand = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Operational Capacity")
	float RemainingCapacity = 30.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Operational Capacity")
	FSROperationalCapacityTierV2 Critical;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Operational Capacity")
	FSROperationalCapacityTierV2 Normal;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Operational Capacity")
	FSROperationalCapacityTierV2 Background;

	float GetSpeedFactor(ESROperationalPriorityV2 Priority) const
	{
		switch (Priority)
		{
		case ESROperationalPriorityV2::Critical:
			return Critical.SpeedFactor;
		case ESROperationalPriorityV2::Background:
			return Background.SpeedFactor;
		case ESROperationalPriorityV2::Normal:
		default:
			return Normal.SpeedFactor;
		}
	}
};

/** Lightweight counters for UI and diagnostics; inventories are never copied. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSROperationalFacilityStatusCountsV2
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Operational Capacity")
	int32 RegisteredFacilityCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Operational Capacity")
	int32 EnabledFacilityCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Operational Capacity")
	int32 ProcessingFacilityCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Operational Capacity")
	int32 ThrottledFacilityCount = 0;
};
