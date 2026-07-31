#include "Automation/SRFacilityDataAsset.h"

USRFacilityDataAsset::USRFacilityDataAsset()
{
	FacilityKind = ESRFacilityKind::Standard;
	Rarity = ESRFacilityRarity::Basic;
	OperationKind = ESRFacilityOperationKind::Process;
	BaseProcessSeconds = 1.0f;
	TransformOperator.SelectionMask.Reset(false);
	for (int32 Column = 0; Column < StarRovers::Pattern::GridSize; ++Column)
	{
		TransformOperator.SelectionMask.SetCellActive(StarRovers::Pattern::GridSize / 2, Column, true);
	}
	SeparationOperator.PrimaryOutputMask.Reset(false);
	for (int32 Row = 0; Row < StarRovers::Pattern::GridSize; ++Row)
	{
		for (int32 Column = 0; Column < StarRovers::Pattern::GridSize / 2; ++Column)
		{
			SeparationOperator.PrimaryOutputMask.SetCellActive(Row, Column, true);
		}
	}
	InputInventory.SlotCount = 0;
	InputInventory.SlotCapacity = 8;
	OutputInventory.SlotCount = 0;
	OutputInventory.SlotCapacity = 8;
}

void USRFacilityDataAsset::PostLoad()
{
	Super::PostLoad();
	TransformOperator.SelectionMask.Normalize();
	SeparationOperator.PrimaryOutputMask.Normalize();
}
