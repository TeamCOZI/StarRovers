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

	if (!bAutoProcessFacilities || FacilityInstancesByOccupantId.IsEmpty())
	{
		SetComponentTickEnabled(false);
		return;
	}

	RefreshFacilityTemperaturesFromSurface();
	ProcessFacilities(DeltaTime);
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

	FSRFacilityInstance& FacilityInstance = FacilityInstancesByOccupantId.FindOrAdd(OccupantId);
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

	const int32 InputCapacity = FMath::Max(1, FacilityDataAsset->InputCapacity);
	const int32 OutputCapacity = FMath::Max(1, FacilityDataAsset->OutputCapacity);

	if (IsValid(FacilityInstance.StructureDataAsset.Get()))
	{
		const FSRStructureData StructureData = FacilityInstance.StructureDataAsset->BuildData();
		for (const FSRStructurePortSpec& StructureInputPort : StructureData.InputPorts)
		{
			const FSRStructurePortSpec RotatedInputPort = StarRovers::Structure::RotateStructurePortSpec(
				StructureInputPort,
				StructureData,
				FacilityInstance.PlacementRotationSteps);
			AppendPortInventory(
				FacilityInstance.InputPortInventories,
				RotatedInputPort,
				ESRFacilityPortKind::Input,
				InputCapacity);
		}
		for (const FSRStructurePortSpec& StructureOutputPort : StructureData.OutputPorts)
		{
			const FSRStructurePortSpec RotatedOutputPort = StarRovers::Structure::RotateStructurePortSpec(
				StructureOutputPort,
				StructureData,
				FacilityInstance.PlacementRotationSteps);
			AppendPortInventory(
				FacilityInstance.OutputPortInventories,
				RotatedOutputPort,
				ESRFacilityPortKind::Output,
				OutputCapacity);
		}
	}

	RefreshFacilityAggregateInventories(FacilityInstance);
}

void USRFacilityNetworkComponent::RefreshFacilityAggregateInventories(FSRFacilityInstance& FacilityInstance) const
{
	FacilityInstance.InputInventory.Reset();
	for (const FSRFacilityPortInventory& InputPortInventory : FacilityInstance.InputPortInventories)
	{
		FacilityInstance.InputInventory.Append(InputPortInventory.Inventory);
	}

	FacilityInstance.OutputInventory.Reset();
	for (const FSRFacilityPortInventory& OutputPortInventory : FacilityInstance.OutputPortInventories)
	{
		FacilityInstance.OutputInventory.Append(OutputPortInventory.Inventory);
	}
}

bool USRFacilityNetworkComponent::UnregisterFacility(FName OccupantId)
{
	const bool bRemoved = FacilityInstancesByOccupantId.Remove(OccupantId) > 0;
	if (FacilityInstancesByOccupantId.IsEmpty())
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
			FacilityInstancesByOccupantId.Num());
	}
	return bRemoved;
}

void USRFacilityNetworkComponent::ClearFacilities()
{
	const int32 RemovedFacilityCount = FacilityInstancesByOccupantId.Num();
	FacilityInstancesByOccupantId.Reset();
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
	return FacilityInstancesByOccupantId.Contains(OccupantId);
}

bool USRFacilityNetworkComponent::GetFacilityInstance(FName OccupantId, FSRFacilityInstance& OutFacilityInstance) const
{
	if (const FSRFacilityInstance* FacilityInstance = FacilityInstancesByOccupantId.Find(OccupantId))
	{
		OutFacilityInstance = *FacilityInstance;
		return true;
	}

	OutFacilityInstance = FSRFacilityInstance();
	return false;
}

bool USRFacilityNetworkComponent::RefreshFacilityTemperatureFromSurface(FName OccupantId)
{
	FSRFacilityInstance* FacilityInstance = FacilityInstancesByOccupantId.Find(OccupantId);
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
	if (!IsValid(SurfaceGrid) || FacilityInstancesByOccupantId.IsEmpty())
	{
		return 0;
	}

	int32 ChangedTemperatureCount = 0;
	for (TPair<FName, FSRFacilityInstance>& FacilityPair : FacilityInstancesByOccupantId)
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
	BoundTimeControlSubsystem = TimeControlSubsystem;
}

void USRFacilityNetworkComponent::UnbindFromTimeControlSubsystem()
{
	USRTimeControlSubsystem* TimeControlSubsystem = BoundTimeControlSubsystem.Get();
	if (IsValid(TimeControlSubsystem))
	{
		TimeControlSubsystem->OnGameCycleAdvanced.RemoveDynamic(this, &USRFacilityNetworkComponent::HandleGameCycleAdvanced);
	}
	BoundTimeControlSubsystem.Reset();
}
