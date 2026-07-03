#include "Automation/SRFacilityNetworkComponent.h"

#include "SRFacilityNetworkComponentInternal.h"
#include "GameFramework/Actor.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	USRPlanetSurfaceGrid* FindOwnerSurfaceGrid(const UActorComponent* Component)
	{
		const AActor* Owner = IsValid(Component) ? Component->GetOwner() : nullptr;
		return IsValid(Owner) ? Owner->FindComponentByClass<USRPlanetSurfaceGrid>() : nullptr;
	}

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

USRFacilityNetworkComponent::USRFacilityNetworkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void USRFacilityNetworkComponent::BeginPlay()
{
	Super::BeginPlay();

	BindToTimeControlSubsystem();
}

void USRFacilityNetworkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromTimeControlSubsystem();

	Super::EndPlay(EndPlayReason);
}

void USRFacilityNetworkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bAutoProcessFacilities || RuntimeState.FacilityInstancesByOccupantId.IsEmpty())
	{
		SetComponentTickEnabled(false);
		return;
	}

	RefreshFacilityTemperaturesFromSurface();

	float ProcessDeltaTime = FMath::Max(0.0f, DeltaTime);
	if (const UWorld* World = GetWorld())
	{
		if (const USRTimeControlSubsystem* TimeControlSubsystem = World->GetSubsystem<USRTimeControlSubsystem>())
		{
			ProcessDeltaTime *= FMath::Max(0.0f, TimeControlSubsystem->GetEffectiveTimeScale());
		}
	}
	if (ProcessDeltaTime <= 0.0f)
	{
		return;
	}

	ProcessFacilities(ProcessDeltaTime);
}

bool USRFacilityNetworkComponent::RegisterFacility(
	FName OccupantId,
	USRStructureDataAsset* StructureDataAsset,
	const FSRPlanetSurfaceGridCellId& OriginCellId,
	const TArray<FSRPlanetSurfaceGridCellId>& FootprintCellIds,
	int32 PlacementRotationSteps)
{
	if (OccupantId.IsNone() || !IsValid(StructureDataAsset))
	{
		if (bLogFacilityNetworkEvents)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[FacilityNetwork] Register failed: OccupantId=%s Structure=%s Owner=%s Reason=InvalidInput"),
				*OccupantId.ToString(),
				*GetNameSafe(StructureDataAsset),
				*GetNameSafe(GetOwner()));
		}
		return false;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	if (!IsValid(StructureData.FacilityDataAsset.Get()))
	{
		return false;
	}

	FSRFacilityInstance& FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.FindOrAdd(OccupantId);
	FacilityInstance.OccupantId = OccupantId;
	FacilityInstance.StructureDataAsset = StructureDataAsset;
	FacilityInstance.FacilityDataAsset = StructureData.FacilityDataAsset;
	FacilityInstance.OriginCellId = OriginCellId;
	FacilityInstance.FootprintCellIds = FootprintCellIds;
	FacilityInstance.PlacementRotationSteps = StarRovers::Structure::NormalizePlacementRotationSteps(PlacementRotationSteps);
	FacilityInstance.TemperatureState = ESRFacilityTemperatureState::Normal;
	FacilityInstance.ProcessProgressSeconds = 0.0f;
	FacilityInstance.bProcessing = false;
	FacilityInstance.bProcessEnabled = false;
	FacilityInstance.bDeliverEnabled = false;
	FacilityInstance.MiningTargetDepositOccupantId = NAME_None;
	FacilityInstance.ProcessingInventory.Reset();
	InitializeFacilityPortInventories(FacilityInstance);
	RefreshFacilityTemperatureFromSurface(OccupantId);

	SetComponentTickEnabled(bAutoProcessFacilities);
	if (bLogFacilityNetworkEvents)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[FacilityNetwork] Registered: OccupantId=%s Structure=%s Facility=%s Owner=%s Origin=(%s) FootprintCells=%d"),
			*OccupantId.ToString(),
			*GetNameSafe(StructureDataAsset),
			*GetNameSafe(StructureData.FacilityDataAsset.Get()),
			*GetNameSafe(GetOwner()),
			*StarRovers::FacilityNetwork::BuildFacilityCellDebugString(OriginCellId),
			FootprintCellIds.Num());
	}
	return true;
}

void USRFacilityNetworkComponent::InitializeFacilityPortInventories(FSRFacilityInstance& FacilityInstance)
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

	RefreshFacilityAggregateInventories(FacilityInstance);
}

void USRFacilityNetworkComponent::RefreshFacilityAggregateInventories(FSRFacilityInstance& FacilityInstance) const
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

bool USRFacilityNetworkComponent::UnregisterFacility(FName OccupantId)
{
	const bool bRemoved = RuntimeState.FacilityInstancesByOccupantId.Remove(OccupantId) > 0;
	if (RuntimeState.FacilityInstancesByOccupantId.IsEmpty())
	{
		SetComponentTickEnabled(false);
	}
	if (bRemoved && bLogFacilityNetworkEvents)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[FacilityNetwork] Unregistered: OccupantId=%s Owner=%s RemainingFacilities=%d"),
			*OccupantId.ToString(),
			*GetNameSafe(GetOwner()),
			RuntimeState.FacilityInstancesByOccupantId.Num());
	}
	return bRemoved;
}

void USRFacilityNetworkComponent::ClearFacilities()
{
	const int32 RemovedFacilityCount = RuntimeState.FacilityInstancesByOccupantId.Num();
	RuntimeState.FacilityInstancesByOccupantId.Reset();
	SetComponentTickEnabled(false);
	if (bLogFacilityNetworkEvents)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[FacilityNetwork] Cleared: Owner=%s RemovedFacilities=%d"),
			*GetNameSafe(GetOwner()),
			RemovedFacilityCount);
	}
}

bool USRFacilityNetworkComponent::HasFacilityInstance(FName OccupantId) const
{
	return RuntimeState.FacilityInstancesByOccupantId.Contains(OccupantId);
}

bool USRFacilityNetworkComponent::GetFacilityInstance(FName OccupantId, FSRFacilityInstance& OutFacilityInstance) const
{
	if (const FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId))
	{
		OutFacilityInstance = *FacilityInstance;
		return true;
	}

	OutFacilityInstance = FSRFacilityInstance();
	return false;
}

bool USRFacilityNetworkComponent::RefreshFacilityTemperatureFromSurface(FName OccupantId)
{
	FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance)
	{
		return false;
	}

	const USRPlanetSurfaceGrid* SurfaceGrid = FindOwnerSurfaceGrid(this);
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	FSRPlanetSurfaceGridCellInfo OriginCellInfo;
	if (!SurfaceGrid->GetCellInfoById(FacilityInstance->OriginCellId, OriginCellInfo))
	{
		return false;
	}

	FacilityInstance->TemperatureState = OriginCellInfo.TemperatureState;
	return true;
}

int32 USRFacilityNetworkComponent::RefreshFacilityTemperaturesFromSurface()
{
	const USRPlanetSurfaceGrid* SurfaceGrid = FindOwnerSurfaceGrid(this);
	if (!IsValid(SurfaceGrid) || RuntimeState.FacilityInstancesByOccupantId.IsEmpty())
	{
		return 0;
	}

	int32 ChangedTemperatureCount = 0;
	for (TPair<FName, FSRFacilityInstance>& FacilityPair : RuntimeState.FacilityInstancesByOccupantId)
	{
		FSRFacilityInstance& FacilityInstance = FacilityPair.Value;
		FSRPlanetSurfaceGridCellInfo OriginCellInfo;
		if (!SurfaceGrid->GetCellInfoById(FacilityInstance.OriginCellId, OriginCellInfo))
		{
			continue;
		}

		if (FacilityInstance.TemperatureState != OriginCellInfo.TemperatureState)
		{
			FacilityInstance.TemperatureState = OriginCellInfo.TemperatureState;
			++ChangedTemperatureCount;
		}
	}

	return ChangedTemperatureCount;
}

void USRFacilityNetworkComponent::BindToTimeControlSubsystem()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	USRTimeControlSubsystem* TimeControlSubsystem = World->GetSubsystem<USRTimeControlSubsystem>();
	if (!IsValid(TimeControlSubsystem))
	{
		return;
	}

	TimeControlSubsystem->OnGameCycleAdvanced.RemoveDynamic(this, &USRFacilityNetworkComponent::HandleGameCycleAdvanced);
	TimeControlSubsystem->OnGameCycleAdvanced.AddDynamic(this, &USRFacilityNetworkComponent::HandleGameCycleAdvanced);
	RuntimeState.BoundTimeControlSubsystem = TimeControlSubsystem;
}

void USRFacilityNetworkComponent::UnbindFromTimeControlSubsystem()
{
	USRTimeControlSubsystem* TimeControlSubsystem = RuntimeState.BoundTimeControlSubsystem.Get();
	if (IsValid(TimeControlSubsystem))
	{
		TimeControlSubsystem->OnGameCycleAdvanced.RemoveDynamic(this, &USRFacilityNetworkComponent::HandleGameCycleAdvanced);
	}
	RuntimeState.BoundTimeControlSubsystem.Reset();
}
