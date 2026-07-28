#include "Automation/SRResourceDataAsset.h"

#include "Automation/SRResourceInstanceOperations.h"
#include "Automation/SRResourceSystemContent.h"

USRResourceDataAsset::USRResourceDataAsset()
{
	ResourceId = FName(TEXT("Resource"));
	DisplayName = NSLOCTEXT("StarRoversResource", "DefaultResourceDisplayName", "Resource");
	Description = NSLOCTEXT("StarRoversResource", "DefaultResourceDescription", "Automation resource.");
	BaseEnergyValue = 1.0;
	BaseProcessLimit = 1;
}

FSRResourceInstance USRResourceDataAsset::BuildDefaultInstance() const
{
	FSRResourceInstance Result;
	Result.ResourceSchemaVersion = StarRovers::Resources::CurrentResourceSchemaVersion;
	Result.ResourceDataAsset = const_cast<USRResourceDataAsset*>(this);
	Result.ResourceId = ResourceId;
	const bool bUsesResourceV2Definition =
		ResourceDefinitionVersion >= StarRovers::Resources::CurrentResourceDefinitionVersion;
	const double InitialEnergy = bUsesResourceV2Definition ? SeedEnergy : BaseEnergyValue;
	Result.EnergyValue = InitialEnergy;
	Result.RemainingProcessLimit = FMath::Max(0, BaseProcessLimit);
	Result.ProcessCount = 0;
	Result.EnergyChangeCount = 0;
	Result.Tags = DefaultTags;
	Result.ResourceClass = bUsesResourceV2Definition ? ResourceClass : ESRResourceClass::Unknown;
	Result.Family = bUsesResourceV2Definition ? Family : ESRResourceFamily::None;
	Result.CurrentEnergy = InitialEnergy;
	Result.SeedEnergySnapshot = bUsesResourceV2Definition ? FMath::Max(0.0, SeedEnergy) : 0.0;
	Result.bHasSeedEnergySnapshot = bUsesResourceV2Definition;
	Result.Spectrum = bUsesResourceV2Definition ? NativeSpectrum : ESRResourceSpectrum::None;
	Result.Grade = bUsesResourceV2Definition
		? FMath::Clamp(NativeGrade, StarRovers::Resources::MinimumGrade, StarRovers::Resources::MaximumGrade)
		: StarRovers::Resources::MinimumGrade;
	Result.ActiveFamilyStateFlags = 0;
	Result.ProcessingMemory = FSRResourceProcessingMemory();
	Result.LogisticsMetadata = FSRResourceLogisticsMetadata();
	Result.StackCount = 1;
	StarRovers::Resources::UpgradeResourceInstanceToCurrentSchema(Result);
	return Result;
}

void USRResourceDataAsset::ApplyResourceV2Preset()
{
	if (ResourceV2Preset == ESRResourceContentPresetV2::Custom)
	{
		return;
	}

	Modify();
	if (FSRResourceSystemContent::ApplyResourcePreset(*this, ResourceV2Preset))
	{
		MarkPackageDirty();
	}
}
