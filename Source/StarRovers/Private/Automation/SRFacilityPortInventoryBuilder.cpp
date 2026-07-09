#include "SRFacilityPortInventoryBuilder.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRFacilityRuntimeData.h"
#include "Structure/SRStructureDataAsset.h"

namespace
{
	const TCHAR* GetPortKindIdPrefix(ESRFacilityPortKind PortKind)
	{
		return PortKind == ESRFacilityPortKind::Output ? TEXT("Output") : TEXT("Input");
	}

	FName MakeDefaultPortId(ESRFacilityPortKind PortKind, int32 PortIndex)
	{
		return FName(*FString::Printf(TEXT("%s_%d"), GetPortKindIdPrefix(PortKind), FMath::Max(0, PortIndex)));
	}

	FSRStructurePortSpec MakeDisconnectedPortSpec(ESRFacilityPortKind PortKind, int32 PortIndex)
	{
		FSRStructurePortSpec PortSpec;
		PortSpec.PortId = MakeDefaultPortId(PortKind, PortIndex);
		PortSpec.CellOffsetX = INDEX_NONE;
		PortSpec.CellOffsetY = INDEX_NONE;
		return PortSpec;
	}

	int32 ResolveInventorySlotCount(const FSRFacilityInventorySpec& InventorySpec, int32 StructurePortCount)
	{
		return FMath::Max(FMath::Max(0, InventorySpec.SlotCount), FMath::Max(0, StructurePortCount));
	}

	void AppendPortInventory(
		TArray<FSRFacilityPortInventory>& PortInventories,
		const FSRStructurePortSpec& StructurePortSpec,
		ESRFacilityPortKind PortKind,
		int32 Capacity)
	{
		const int32 PortIndex = PortInventories.Num();
		FSRFacilityPortInventory& PortInventory = PortInventories.AddDefaulted_GetRef();
		PortInventory.PortId = StructurePortSpec.PortId.IsNone()
			? MakeDefaultPortId(PortKind, PortIndex)
			: StructurePortSpec.PortId;
		PortInventory.PortKind = PortKind;
		PortInventory.PortIndex = PortIndex;
		PortInventory.PortSpec = StructurePortSpec;
		PortInventory.PortSpec.PortId = PortInventory.PortId;
		PortInventory.Capacity = FMath::Max(1, Capacity);
		PortInventory.Inventory.Reset();
	}
}

void FSRFacilityPortInventoryBuilder::Initialize(FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	FacilityInstance.InputPortInventories.Reset();
	FacilityInstance.OutputPortInventories.Reset();
	FacilityInstance.InputInventory.Reset();
	FacilityInstance.OutputInventory.Reset();
	if (!IsValid(FacilityDataAsset))
	{
		return;
	}

	const int32 InputSlotCapacity = FMath::Max(1, FacilityDataAsset->InputInventory.SlotCapacity);
	const int32 OutputSlotCapacity = FMath::Max(1, FacilityDataAsset->OutputInventory.SlotCapacity);
	TArray<FSRStructurePortSpec> RotatedInputPorts;
	TArray<FSRStructurePortSpec> RotatedOutputPorts;

	if (IsValid(FacilityInstance.StructureDataAsset.Get()))
	{
		const FSRStructureData StructureData = FacilityInstance.StructureDataAsset->BuildData();
		for (const FSRStructurePortSpec& StructureInputPort : StructureData.InputPorts)
		{
			RotatedInputPorts.Add(StarRovers::Structure::RotateStructurePortSpec(
				StructureInputPort,
				StructureData,
				FacilityInstance.PlacementRotationSteps));
		}
		for (const FSRStructurePortSpec& StructureOutputPort : StructureData.OutputPorts)
		{
			RotatedOutputPorts.Add(StarRovers::Structure::RotateStructurePortSpec(
				StructureOutputPort,
				StructureData,
				FacilityInstance.PlacementRotationSteps));
		}
	}

	const int32 InputSlotCount = ResolveInventorySlotCount(FacilityDataAsset->InputInventory, RotatedInputPorts.Num());
	for (int32 InputSlotIndex = 0; InputSlotIndex < InputSlotCount; ++InputSlotIndex)
	{
		const FSRStructurePortSpec SlotPortSpec = RotatedInputPorts.IsValidIndex(InputSlotIndex)
			? RotatedInputPorts[InputSlotIndex]
			: MakeDisconnectedPortSpec(ESRFacilityPortKind::Input, InputSlotIndex);
		AppendPortInventory(
			FacilityInstance.InputPortInventories,
			SlotPortSpec,
			ESRFacilityPortKind::Input,
			InputSlotCapacity);
	}

	const int32 OutputSlotCount = ResolveInventorySlotCount(FacilityDataAsset->OutputInventory, RotatedOutputPorts.Num());
	for (int32 OutputSlotIndex = 0; OutputSlotIndex < OutputSlotCount; ++OutputSlotIndex)
	{
		const FSRStructurePortSpec SlotPortSpec = RotatedOutputPorts.IsValidIndex(OutputSlotIndex)
			? RotatedOutputPorts[OutputSlotIndex]
			: MakeDisconnectedPortSpec(ESRFacilityPortKind::Output, OutputSlotIndex);
		AppendPortInventory(
			FacilityInstance.OutputPortInventories,
			SlotPortSpec,
			ESRFacilityPortKind::Output,
			OutputSlotCapacity);
	}

	RefreshAggregateInventories(FacilityInstance);
}

void FSRFacilityPortInventoryBuilder::RefreshAggregateInventories(FSRFacilityInstance& FacilityInstance)
{
	FacilityInstance.InputInventory.Reset();
	for (const FSRFacilityPortInventory& InputPortInventory : FacilityInstance.InputPortInventories)
	{
		for (const FSRResourceInstance& ResourceInstance : InputPortInventory.Inventory)
		{
			if (ResourceInstance.StackCount > 0)
			{
				FacilityInstance.InputInventory.Add(ResourceInstance);
			}
		}
	}

	FacilityInstance.OutputInventory.Reset();
	for (const FSRFacilityPortInventory& OutputPortInventory : FacilityInstance.OutputPortInventories)
	{
		for (const FSRResourceInstance& ResourceInstance : OutputPortInventory.Inventory)
		{
			if (ResourceInstance.StackCount > 0)
			{
				FacilityInstance.OutputInventory.Add(ResourceInstance);
			}
		}
	}
}
