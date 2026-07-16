#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityNetworkRuntimeState.h"
#include "Automation/SRFacilityRuntimeData.h"
#include "Components/SceneComponent.h"
#include "SRFacilityNetworkComponent.generated.h"

class USRPlanetSurfaceGrid;
class USRResourceDataAsset;
class USRStructureDataAsset;

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

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility|Hub")
	void GetHubOutboundCargoResourceIds(FName OccupantId, TArray<FName>& OutResourceIds) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility|Hub")
	bool CanStoreHubInboundCargo(FName OccupantId, const FSRResourceInstance& Cargo) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool TryStoreHubInboundCargo(FName OccupantId, const FSRResourceInstance& Cargo);

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

	int32 ProcessFacilities(float DeltaTime);
	bool TryStartProcessing(FSRFacilityInstance& FacilityInstance);
	bool TryCompleteProcessing(FSRFacilityInstance& FacilityInstance);

	UPROPERTY(Transient)
	FSRFacilityNetworkRuntimeState RuntimeState;
};
