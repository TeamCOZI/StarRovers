#include "Automation/SRFacilityDataAsset.h"

USRFacilityDataAsset::USRFacilityDataAsset()
{
	FacilityId = FName(TEXT("Facility"));
	DisplayName = NSLOCTEXT("StarRoversFacility", "DefaultFacilityDisplayName", "Facility");
	Description = NSLOCTEXT("StarRoversFacility", "DefaultFacilityDescription", "Automation facility.");
	FacilityKind = ESRFacilityKind::Standard;
	Rarity = ESRFacilityRarity::Basic;
	OperationKind = ESRFacilityOperationKind::Process;
	BaseProcessSeconds = 1.0f;
	InputInventory.SlotCount = 0;
	InputInventory.SlotCapacity = 8;
	OutputInventory.SlotCount = 0;
	OutputInventory.SlotCapacity = 8;
	bRequiresColdTemperature = false;
	bRequiresHotTemperature = false;
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
