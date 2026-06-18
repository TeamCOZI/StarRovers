#include "Automation/SRFacilityDataAsset.h"

USRFacilityDataAsset::USRFacilityDataAsset()
{
	FacilityId = FName(TEXT("Facility"));
	DisplayName = NSLOCTEXT("StarRoversFacility", "DefaultFacilityDisplayName", "Facility");
	Description = NSLOCTEXT("StarRoversFacility", "DefaultFacilityDescription", "Automation facility.");
	Rarity = ESRFacilityRarity::Basic;
	OperationKind = ESRFacilityOperationKind::Process;
	InputResourceCount = 1;
	SplitOutputCount = 2;
	BaseProcessSeconds = 1.0f;
	InputCapacity = 8;
	OutputCapacity = 8;
	bRequiresColdTemperature = false;
	bRequiresHotTemperature = false;
}
