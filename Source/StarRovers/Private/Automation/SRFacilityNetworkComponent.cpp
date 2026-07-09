#include "Automation/SRFacilityNetworkComponent.h"

#include "SRFacilityPortInventoryBuilder.h"
#include "SRFacilityTemperatureSynchronizer.h"
#include "SRFacilityResourceOperations.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Structure/SRStructureDataAsset.h"

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
	FSRFacilityPortInventoryBuilder::Initialize(FacilityInstance);
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
			*StarRovers::FacilityResources::BuildFacilityCellDebugString(OriginCellId),
			FootprintCellIds.Num());
	}
	return true;
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
	return FSRFacilityTemperatureSynchronizer::RefreshFacilityFromSurface(this, RuntimeState, OccupantId);
}

int32 USRFacilityNetworkComponent::RefreshFacilityTemperaturesFromSurface()
{
	return FSRFacilityTemperatureSynchronizer::RefreshFacilitiesFromSurface(this, RuntimeState);
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
