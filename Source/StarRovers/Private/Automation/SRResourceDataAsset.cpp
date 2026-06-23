#include "Automation/SRResourceDataAsset.h"

USRResourceDataAsset::USRResourceDataAsset()
{
	ResourceId = FName(TEXT("Resource"));
	DisplayName = NSLOCTEXT("StarRoversResource", "DefaultResourceDisplayName", "Resource");
	Description = NSLOCTEXT("StarRoversResource", "DefaultResourceDescription", "Automation resource.");
	ResourceKind = ESRResourceKind::Energy;
	BaseEnergyValue = 1.0;
	CatalystOperator = ESRResourceCatalystOperator::None;
	BaseProcessLimit = 1;
	bCountsAsStellarFuel = false;
	StellarFuelValueMultiplier = 1.0;
}

FSRResourceInstance USRResourceDataAsset::BuildDefaultInstance() const
{
	FSRResourceInstance Result;
	Result.ResourceDataAsset = const_cast<USRResourceDataAsset*>(this);
	Result.ResourceId = ResourceId;
	Result.ResourceKind = ResourceKind;
	Result.EnergyValue = BaseEnergyValue;
	Result.CatalystOperator = CatalystOperator;
	Result.RemainingProcessLimit = FMath::Max(0, BaseProcessLimit);
	Result.ProcessCount = 0;
	Result.Tags = DefaultTags;
	Result.StackCount = 1;
	Result.bCountsAsStellarFuel = bCountsAsStellarFuel;
	Result.StellarFuelValueMultiplier = FMath::Max(0.0, StellarFuelValueMultiplier);
	return Result;
}
