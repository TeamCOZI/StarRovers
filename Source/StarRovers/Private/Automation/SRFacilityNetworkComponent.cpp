#include "Automation/SRFacilityNetworkComponent.h"

#include "Utility/SRLog.h"
#include "SRFacilityCellTemperatureEffectApplier.h"
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
	RefreshOperationalCapacity();
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
			SR_LOG(FacilityNetwork, LogTemp,
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
	RuntimeState.bFacilitySchedulerOrderDirty = true;
	FacilityInstance.OccupantId = OccupantId;
	FacilityInstance.StructureDataAsset = StructureDataAsset;
	FacilityInstance.FacilityDataAsset = StructureData.FacilityDataAsset;
	FacilityInstance.OriginCellId = OriginCellId;
	FacilityInstance.FootprintCellIds = FootprintCellIds;
	FacilityInstance.PlacementRotationSteps = StarRovers::Structure::NormalizePlacementRotationSteps(PlacementRotationSteps);
	FacilityInstance.TemperatureState = ESRFacilityTemperatureState::Normal;
	FacilityInstance.ProcessProgressSeconds = 0.0f;
	FacilityInstance.ResolvedProcessSeconds = 0.0f;
	FacilityInstance.bHasResolvedProcessSeconds = false;
	FacilityInstance.bProcessing = false;
	FacilityInstance.bProcessEnabled = StructureData.bProcessReady;
	FacilityInstance.bDeliverEnabled = StructureData.bDeliveryReady;
	FacilityInstance.OperationalPriority = StructureData.FacilityDataAsset->DefaultOperationalPriority;
	FacilityInstance.OperationalSpeedFactor = 1.0f;
	FacilityInstance.MiningTargetDepositOccupantId = NAME_None;
	FacilityInstance.ProcessingInventory.Reset();
	FacilityInstance.SelectedProcessTagRecipeId = NAME_None;
	FacilityInstance.SelectedFuelImprintRecipeId = NAME_None;
	FSRFacilityPortInventoryBuilder::Initialize(FacilityInstance);

	// A package-gated authored default must not make the guaranteed Technology
	// recipe undiscoverable. Select the first available fallback on placement.
	FName InitialRecipeId;
	TArray<FName> AvailableRecipeIds;
	FString RecipeFailure;
	if (GetFacilityResourceV2RecipeState(
			OccupantId,
			InitialRecipeId,
			AvailableRecipeIds,
			RecipeFailure)
		&& !AvailableRecipeIds.IsEmpty()
		&& !AvailableRecipeIds.Contains(InitialRecipeId))
	{
		SetFacilityResourceV2Recipe(OccupantId, AvailableRecipeIds[0]);
	}
	RefreshFacilityTemperatureFromSurface(OccupantId);
	const int32 AppliedCellTemperatureEffects = FSRFacilityCellTemperatureEffectApplier::ApplyInstallationEffects(
		this,
		FacilityInstance);
	if (AppliedCellTemperatureEffects > 0)
	{
		RefreshFacilityTemperaturesFromSurface();
	}
	RefreshOperationalCapacity();

	SetComponentTickEnabled(bAutoProcessFacilities);
	if (bLogFacilityNetworkEvents)
	{
		SR_LOG(FacilityNetwork, LogTemp,
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
	FSRFacilityInstance RemovedFacilityInstance;
	const bool bRemoved = RuntimeState.FacilityInstancesByOccupantId.RemoveAndCopyValue(
		OccupantId,
		RemovedFacilityInstance);
	if (bRemoved)
	{
		RuntimeState.bFacilitySchedulerOrderDirty = true;
	}
	const int32 RemovedCellTemperatureEffects = bRemoved
		? FSRFacilityCellTemperatureEffectApplier::RemoveInstallationEffects(this, RemovedFacilityInstance)
		: 0;
	if (RuntimeState.FacilityInstancesByOccupantId.IsEmpty())
	{
		SetComponentTickEnabled(false);
	}
	else if (RemovedCellTemperatureEffects > 0)
	{
		RefreshFacilityTemperaturesFromSurface();
	}
	RefreshOperationalCapacity();
	if (bRemoved && bLogFacilityNetworkEvents)
	{
		SR_LOG(FacilityNetwork, LogTemp,
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
	for (TPair<FName, FSRFacilityInstance>& FacilityPair : RuntimeState.FacilityInstancesByOccupantId)
	{
		FSRFacilityCellTemperatureEffectApplier::RemoveInstallationEffects(this, FacilityPair.Value);
	}
	RuntimeState.FacilityInstancesByOccupantId.Reset();
	RuntimeState.OperationalCapacityReport = FSROperationalCapacityReportV2();
	RuntimeState.FacilitySchedulerOrder.Reset();
	RuntimeState.NextFacilitySchedulerOccupantId = NAME_None;
	RuntimeState.bFacilitySchedulerOrderDirty = false;
	SetComponentTickEnabled(false);
	if (bLogFacilityNetworkEvents)
	{
		SR_LOG(FacilityNetwork, LogTemp,
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

FSRFacilityResourceProducedSignature& USRFacilityNetworkComponent::OnResourceProduced()
{
	return ResourceProducedEvent;
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
