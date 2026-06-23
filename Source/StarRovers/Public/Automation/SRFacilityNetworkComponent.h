#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityDataAsset.h"
#include "Components/SceneComponent.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRFacilityNetworkComponent.generated.h"

class USRPlanetSurfaceGrid;
class USRTimeControlSubsystem;

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFacilityPortInventory
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "PortId"))
	FName PortId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "PortKind"))
	ESRFacilityPortKind PortKind = ESRFacilityPortKind::Input;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "PortIndex"))
	int32 PortIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "PortSpec"))
	FSRStructurePortSpec PortSpec;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "Capacity"))
	int32 Capacity = 1;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "Inventory"))
	TArray<FSRResourceInstance> Inventory;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFacilityInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "OccupantId"))
	FName OccupantId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "StructureDataAsset"))
	TObjectPtr<USRStructureDataAsset> StructureDataAsset = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "FacilityDataAsset"))
	TObjectPtr<USRFacilityDataAsset> FacilityDataAsset = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "OriginCellId"))
	FSRPlanetSurfaceGridCellId OriginCellId;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "FootprintCellIds"))
	TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "PlacementRotationSteps"))
	int32 PlacementRotationSteps = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "InputPortInventories"))
	TArray<FSRFacilityPortInventory> InputPortInventories;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "OutputPortInventories"))
	TArray<FSRFacilityPortInventory> OutputPortInventories;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "InputInventory"))
	TArray<FSRResourceInstance> InputInventory;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "OutputInventory"))
	TArray<FSRResourceInstance> OutputInventory;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "ProcessingInventory"))
	TArray<FSRResourceInstance> ProcessingInventory;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "MiningTargetDepositOccupantId"))
	FName MiningTargetDepositOccupantId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "TemperatureState"))
	ESRFacilityTemperatureState TemperatureState = ESRFacilityTemperatureState::Normal;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "ProcessProgressSeconds"))
	float ProcessProgressSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "bProcessing"))
	bool bProcessing = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "bProcessEnabled"))
	bool bProcessEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "bDeliverEnabled"))
	bool bDeliverEnabled = false;
};

UCLASS(ClassGroup = (StarRovers), Blueprintable, meta = (BlueprintSpawnableComponent))
class STARROVERS_API USRFacilityNetworkComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USRFacilityNetworkComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility")
	bool RegisterFacility(
		FName OccupantId,
		USRStructureDataAsset* StructureDataAsset,
		const FSRPlanetSurfaceGridCellId& OriginCellId,
		const TArray<FSRPlanetSurfaceGridCellId>& FootprintCellIds,
		int32 PlacementRotationSteps = 0);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility")
	bool UnregisterFacility(FName OccupantId);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility")
	void ClearFacilities();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility")
	bool HasFacilityInstance(FName OccupantId) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility")
	bool GetFacilityInstance(FName OccupantId, FSRFacilityInstance& OutFacilityInstance) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility")
	bool AddInputResource(FName OccupantId, const FSRResourceInstance& ResourceInstance);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility")
	bool AddInputResourceToPort(FName OccupantId, int32 InputPortIndex, const FSRResourceInstance& ResourceInstance);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility")
	bool ExtractOutputResource(FName OccupantId, FSRResourceInstance& OutResourceInstance);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility")
	bool SetFacilityTemperatureState(FName OccupantId, ESRFacilityTemperatureState TemperatureState);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility")
	bool SetFacilityProcessEnabled(FName OccupantId, bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility")
	bool SetFacilityDeliverEnabled(FName OccupantId, bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Temperature")
	bool RefreshFacilityTemperatureFromSurface(FName OccupantId);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Temperature")
	int32 RefreshFacilityTemperaturesFromSurface();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Transfer")
	bool TryAcceptInputResourceFromConveyorCell(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& ConveyorCellId,
		const FSRResourceInstance& ResourceInstance,
		FName SourceFacilityOccupantId = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Transfer")
	bool TryPullOutputResourceToConveyorCell(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& ConveyorCellId,
		FSRResourceInstance& OutResourceInstance,
		FName& OutSourceFacilityOccupantId);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility|Transfer")
	bool HasConnectedConveyorForFacilityPort(FName OccupantId, ESRFacilityPortKind PortKind) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Preview")
	bool GetFacilityOutputPreview(
		FName OccupantId,
		FSRResourceInstance& OutPrimaryOutput,
		TArray<FSRResourceInstance>& OutAdditionalOutputs,
		int32& OutOutputCount) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility|Mining")
	bool GetFacilityMiningTarget(FName OccupantId, FSRResourceDepositInstance& OutResourceDeposit) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Debug")
	void SetFacilityDebugLoggingEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility|Debug")
	bool IsFacilityDebugLoggingEnabled() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility|Debug")
	void GetRegisteredFacilityOccupantIds(TArray<FName>& OutOccupantIds) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Debug")
	bool DebugAddInputResourceFromDataAsset(
		FName OccupantId,
		USRResourceDataAsset* ResourceDataAsset,
		int32 StackCount = 1);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Debug")
	bool DebugAddRawEnergyInputResource(
		FName OccupantId,
		FName ResourceId,
		double EnergyValue,
		int32 RemainingProcessLimit = 3,
		int32 StackCount = 1);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Debug")
	bool DebugStepFacilities(float DeltaTime = 1.0f, int32 StepCount = 1);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Debug")
	bool DebugDumpFacilityState(FName OccupantId) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Debug")
	bool DebugExtractAndLogOutputResource(FName OccupantId, FSRResourceInstance& OutResourceInstance);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Debug")
	int32 DebugApplyGameCyclesToResources(int32 CycleCount = 1);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (DisplayName = "bAutoProcessFacilities"))
	bool bAutoProcessFacilities = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (DisplayName = "MaxFacilitiesProcessedPerTick", ClampMin = "1"))
	int32 MaxFacilitiesProcessedPerTick = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Debug", meta = (DisplayName = "bLogFacilityNetworkEvents"))
	bool bLogFacilityNetworkEvents = true;

private:
	UFUNCTION()
	void HandleGameCycleAdvanced(int32 CurrentCycleIndex);

	void BindToTimeControlSubsystem();
	void UnbindFromTimeControlSubsystem();
	int32 ApplyGameCycleToResources();
	int32 ApplyGameCycleToInventory(TArray<FSRResourceInstance>& Inventory);
	void InitializeFacilityPortInventories(FSRFacilityInstance& FacilityInstance);
	void RefreshFacilityAggregateInventories(FSRFacilityInstance& FacilityInstance) const;
	bool GatherPendingInputResources(const FSRFacilityInstance& FacilityInstance, TArray<FSRResourceInstance>& OutInputResources) const;
	bool CanStoreOutputResources(const FSRFacilityInstance& FacilityInstance, int32 OutputResourceCount) const;
	void StoreOutputResources(FSRFacilityInstance& FacilityInstance, const TArray<FSRResourceInstance>& OutputResources);
	FSRFacilityPortInventory* FindInputPortInventoryForDirectAdd(FSRFacilityInstance& FacilityInstance, int32 PreferredPortIndex = INDEX_NONE);

	int32 ProcessFacilities(float DeltaTime);
	FSRFacilityPortInventory* FindConnectedInputPortInventory(
		USRPlanetSurfaceGrid* SurfaceGrid,
		FSRFacilityInstance& FacilityInstance,
		const FSRPlanetSurfaceGridCellId& ConveyorCellId);
	FSRFacilityPortInventory* FindConnectedOutputPortInventory(
		USRPlanetSurfaceGrid* SurfaceGrid,
		FSRFacilityInstance& FacilityInstance,
		const FSRPlanetSurfaceGridCellId& ConveyorCellId);
	bool IsConveyorCellConnectedToPortInventory(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRFacilityInstance& FacilityInstance,
		const FSRFacilityPortInventory& PortInventory,
		const FSRPlanetSurfaceGridCellId& ConveyorCellId) const;
	bool IsConveyorCellConnectedToFacilityPort(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRFacilityInstance& FacilityInstance,
		const FSRPlanetSurfaceGridCellId& ConveyorCellId,
		ESRFacilityPortKind PortKind) const;
	bool IsConveyorCellConnectedToExplicitPort(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRFacilityInstance& FacilityInstance,
		const FSRStructurePortSpec& PortSpec,
		const FSRPlanetSurfaceGridCellId& ConveyorCellId) const;
	bool GetNeighborCellIds(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId,
		TArray<FSRPlanetSurfaceGridCellId>& OutNeighborCellIds) const;
	bool CanFacilityRun(const FSRFacilityInstance& FacilityInstance) const;
	bool CanMiningFacilityRun(const FSRFacilityInstance& FacilityInstance) const;
	bool FindMiningTargetDeposit(const FSRFacilityInstance& FacilityInstance, FSRResourceDepositInstance& OutResourceDeposit) const;
	bool TryCompleteMining(FSRFacilityInstance& FacilityInstance);
	bool CanFacilityAdvanceProcessing(const FSRFacilityInstance& FacilityInstance) const;
	int32 CountProducedOutputResources(const USRFacilityDataAsset* FacilityDataAsset) const;
	int32 ResolvePrimaryOutputCount(const FSRFacilityInstance& FacilityInstance) const;
	int32 ResolveRequiredOutputSlots(const FSRFacilityInstance& FacilityInstance) const;
	float ResolveProcessSeconds(const FSRFacilityInstance& FacilityInstance) const;
	bool TryStartProcessing(FSRFacilityInstance& FacilityInstance);
	bool TryCompleteProcessing(FSRFacilityInstance& FacilityInstance);
	FSRResourceInstance BuildBaseOutputResource(const FSRFacilityInstance& FacilityInstance, const TArray<FSRResourceInstance>& ConsumedResources) const;
	void ApplyFacilityEffects(const USRFacilityDataAsset* FacilityDataAsset, FSRResourceInstance& ResourceInstance, TArray<FSRResourceInstance>& OutAdditionalOutputs) const;
	int32 ApplyFacilityCellTemperatureEffects(const FSRFacilityInstance& FacilityInstance);
	void AddTagStack(FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag, int32 Count) const;
	void RemoveTagStack(FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag, int32 Count) const;

	UPROPERTY(Transient)
	TWeakObjectPtr<USRTimeControlSubsystem> BoundTimeControlSubsystem;

	UPROPERTY(Transient)
	TMap<FName, FSRFacilityInstance> FacilityInstancesByOccupantId;
};
