#include "Automation/SRFacilityDataAsset.h"

#include "Automation/SRResourceSystemContent.h"

USRFacilityDataAsset::USRFacilityDataAsset()
{
	FacilityKind = ESRFacilityKind::Standard;
	Rarity = ESRFacilityRarity::Basic;
	OperationKind = ESRFacilityOperationKind::Process;
	BaseProcessSeconds = 1.0f;
	InputInventory.SlotCount = 0;
	InputInventory.SlotCapacity = 8;
	OutputInventory.SlotCount = 0;
	OutputInventory.SlotCapacity = 8;
}

void USRFacilityDataAsset::PostLoad()
{
	Super::PostLoad();

	if (InputInventory.SlotCapacity == 8 && InputCapacity != 8)
	{
		InputInventory.SlotCapacity = FMath::Max(1, InputCapacity);
	}
	if (OutputInventory.SlotCapacity == 8 && OutputCapacity != 8)
	{
		OutputInventory.SlotCapacity = FMath::Max(1, OutputCapacity);
	}
}

bool USRFacilityDataAsset::UsesResourceV2Definition() const
{
	return FacilityDefinitionVersion >= StarRovers::Facilities::CurrentFacilityDefinitionVersion;
}

void USRFacilityDataAsset::ApplyResourceV2Preset()
{
	if (ResourceV2Preset == ESRFacilityContentPresetV2::Custom)
	{
		return;
	}

	Modify();
	if (FSRResourceSystemContent::ApplyFacilityPreset(*this, ResourceV2Preset))
	{
		MarkPackageDirty();
	}
}
