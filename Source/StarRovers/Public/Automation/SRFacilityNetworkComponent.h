#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityNetworkRuntimeState.h"
#include "Automation/SRFacilityNetworkSaveData.h"
#include "Automation/SRFacilityRuntimeData.h"
#include "Components/SceneComponent.h"
#include "SRFacilityNetworkComponent.generated.h"

class USRPlanetSurfaceGrid;
class USRResourceDataAsset;
class USRStructureDataAsset;
class USRFacilityNetworkComponent;

DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FSRFacilityResourceProducedSignature,
	USRFacilityNetworkComponent*,
	FName,
	const FSRResourceInstance&);

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

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility|Hub")
	bool IsHubFacility(FName OccupantId) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool TryTakeHubOutboundCargo(FName OccupantId, int32 MaxStackCount, FSRResourceInstance& OutCargo);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool TryTakeHubOutboundCargoByResource(FName OccupantId, FName ResourceId, int32 MaxStackCount, FSRResourceInstance& OutCargo);

	bool TryTakeHubOutboundCargoMatching(
		FName OccupantId,
		int32 MaxStackCount,
		TFunctionRef<bool(const FSRResourceInstance&)> CargoPredicate,
		FSRResourceInstance& OutCargo);

	bool TryTakeHubOutboundCargoMatchingFromInputPort(
		FName OccupantId,
		int32 InputPortIndex,
		int32 MaxStackCount,
		TFunctionRef<bool(const FSRResourceInstance&)> CargoPredicate,
		FSRResourceInstance& OutCargo);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility|Hub")
	void GetHubOutboundCargoResourceIds(FName OccupantId, TArray<FName>& OutResourceIds) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool SetHubStarFuelMissileAutoLaunchInputPort(FName OccupantId, int32 InputPortIndex, bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility|Hub")
	bool IsHubStarFuelMissileAutoLaunchInputPort(FName OccupantId, int32 InputPortIndex) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility|Hub")
	void GetHubStarFuelMissileAutoLaunchInputPorts(FName OccupantId, TArray<int32>& OutInputPortIndices) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility|Hub")
	bool CanStoreHubInboundCargo(FName OccupantId, const FSRResourceInstance& Cargo) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool TryStoreHubInboundCargo(FName OccupantId, const FSRResourceInstance& Cargo);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility")
	bool SetFacilityTemperatureState(FName OccupantId, ESRFacilityTemperatureState TemperatureState);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility")
	bool SetFacilityProcessEnabled(FName OccupantId, bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Operational Capacity")
	bool SetFacilityOperationalPriority(FName OccupantId, ESROperationalPriorityV2 Priority);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility|Operational Capacity")
	FSROperationalCapacityReportV2 GetOperationalCapacityReport() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility|Operational Capacity")
	FSROperationalFacilityStatusCountsV2 GetOperationalFacilityStatusCounts() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Operational Capacity")
	FSROperationalCapacityReportV2 RefreshOperationalCapacity();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility|Resource V2 Recipe")
	bool GetFacilityResourceV2RecipeState(
		FName OccupantId,
		FName& OutSelectedRecipeId,
		TArray<FName>& OutAvailableRecipeIds,
		FString& OutFailureReason) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Resource V2 Recipe")
	bool SetFacilityResourceV2Recipe(FName OccupantId, FName RecipeId);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Resource V2 Recipe")
	bool CycleFacilityResourceV2Recipe(FName OccupantId);

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
		int32& OutOutputCount,
		TArray<FString>& OutEnergyFormulaTexts) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility|Mining")
	bool GetFacilityMiningTarget(FName OccupantId, FSRResourceDepositInstance& OutResourceDeposit) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Debug")
	void SetFacilityDebugLoggingEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility|Debug")
	bool IsFacilityDebugLoggingEnabled() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility|Debug")
	void GetRegisteredFacilityOccupantIds(TArray<FName>& OutOccupantIds) const;

	/** Native runtime signal used by Run milestones and read-only telemetry. */
	FSRFacilityResourceProducedSignature& OnResourceProduced();

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
	bool DebugAddResourceV2Card(
		FName OccupantId,
		FName ResourceId,
		ESRResourceFamily Family,
		double CurrentEnergy,
		ESRResourceSpectrum Spectrum,
		int32 Grade = 1,
		int32 StackCount = 1);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Debug")
	bool DebugAddReferenceResourceV2Card(
		FName OccupantId,
		ESRResourceContentPresetV2 ResourcePreset,
		FName OriginBodyId = NAME_None,
		int32 StackCount = 1);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Debug")
	bool DebugAddReferenceResourceV2(
		FName OccupantId,
		ESRResourceContentPresetV2 ResourcePreset,
		FName OriginBodyId = NAME_None,
		int32 StackCount = 1);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Debug")
	bool DebugAddReferenceStellarFuelBatchV2(
		FName OccupantId,
		ESRStellarFuelReferenceTopologyV2 Topology = ESRStellarFuelReferenceTopologyV2::DistributedConvergence,
		FName FabricatorBodyId = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Debug")
	bool DebugStepFacilities(float DeltaTime = 1.0f, int32 StepCount = 1);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Debug")
	bool DebugDumpFacilityState(FName OccupantId) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Debug")
	bool DebugExtractAndLogOutputResource(FName OccupantId, FSRResourceInstance& OutResourceInstance);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Debug")
	int32 DebugApplyGameCyclesToResources(int32 CycleCount = 1);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Save")
	void ExportSaveData(FSRFacilityNetworkSaveData& OutSaveData) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Save")
	bool ImportSaveData(const FSRFacilityNetworkSaveData& SaveData);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (DisplayName = "bAutoProcessFacilities"))
	bool bAutoProcessFacilities = true;

	// Legacy property name retained for authored asset compatibility. This now
	// caps expensive start/completion transitions; active clocks all advance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (DisplayName = "MaxFacilityTransitionsPerTick", ClampMin = "1", ToolTip = "Maximum facility start/completion transitions per tick. Active process clocks are not capped."))
	int32 MaxFacilitiesProcessedPerTick = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Debug", meta = (DisplayName = "bLogFacilityNetworkEvents"))
	bool bLogFacilityNetworkEvents = true;

private:
	UFUNCTION()
	void HandleGameCycleAdvanced(int32 CurrentCycleIndex);

	void BindToTimeControlSubsystem();
	void UnbindFromTimeControlSubsystem();

	int32 ProcessFacilities(float DeltaTime);
	bool TryStartProcessing(FSRFacilityInstance& FacilityInstance);
	bool TryCompleteProcessing(FSRFacilityInstance& FacilityInstance);
	void TryAutoLaunchStarFuelMissilesFromInputPort(FSRFacilityInstance& FacilityInstance, int32 InputPortIndex);

	UPROPERTY(Transient)
	FSRFacilityNetworkRuntimeState RuntimeState;

	FSRFacilityResourceProducedSignature ResourceProducedEvent;
};
