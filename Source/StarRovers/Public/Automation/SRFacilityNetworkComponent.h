#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityDataAsset.h"
#include "Components/SceneComponent.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRFacilityNetworkComponent.generated.h"

class USRStructureDataAsset;
class USRPlanetSurfaceGrid;

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

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "InputInventory"))
	TArray<FSRResourceInstance> InputInventory;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "OutputInventory"))
	TArray<FSRResourceInstance> OutputInventory;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "ProcessingInventory"))
	TArray<FSRResourceInstance> ProcessingInventory;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "TemperatureState"))
	ESRFacilityTemperatureState TemperatureState = ESRFacilityTemperatureState::Normal;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "ProcessProgressSeconds"))
	float ProcessProgressSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "bProcessing"))
	bool bProcessing = false;
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
		const TArray<FSRPlanetSurfaceGridCellId>& FootprintCellIds);

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
	bool ExtractOutputResource(FName OccupantId, FSRResourceInstance& OutResourceInstance);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility")
	bool SetFacilityTemperatureState(FName OccupantId, ESRFacilityTemperatureState TemperatureState);

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

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (DisplayName = "bAutoProcessFacilities"))
	bool bAutoProcessFacilities = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (DisplayName = "MaxFacilitiesProcessedPerTick", ClampMin = "1"))
	int32 MaxFacilitiesProcessedPerTick = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Debug", meta = (DisplayName = "bLogFacilityNetworkEvents"))
	bool bLogFacilityNetworkEvents = true;

private:
	int32 ProcessFacilities(float DeltaTime);
	bool IsConveyorCellConnectedToFacilityPort(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRFacilityInstance& FacilityInstance,
		const FSRPlanetSurfaceGridCellId& ConveyorCellId,
		ESRFacilityPortKind PortKind) const;
	bool IsConveyorCellConnectedToExplicitPort(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRFacilityInstance& FacilityInstance,
		const FSRFacilityPortSpec& PortSpec,
		const FSRPlanetSurfaceGridCellId& ConveyorCellId) const;
	bool IsConveyorCellAdjacentToFacilityFootprint(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRFacilityInstance& FacilityInstance,
		const FSRPlanetSurfaceGridCellId& ConveyorCellId) const;
	bool GetFootprintCellIdByOffset(
		const FSRFacilityInstance& FacilityInstance,
		int32 FootprintCellX,
		int32 FootprintCellY,
		FSRPlanetSurfaceGridCellId& OutCellId) const;
	bool GetNeighborCellIdByFacilityPortDirection(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId,
		ESRFacilityPortDirection Direction,
		TArray<FSRPlanetSurfaceGridCellId>& OutNeighborCellIds) const;
	bool CanFacilityRun(const FSRFacilityInstance& FacilityInstance) const;
	int32 ResolveRequiredOutputSlots(const USRFacilityDataAsset* FacilityDataAsset) const;
	float ResolveProcessSeconds(const FSRFacilityInstance& FacilityInstance) const;
	bool TryStartProcessing(FSRFacilityInstance& FacilityInstance);
	bool TryCompleteProcessing(FSRFacilityInstance& FacilityInstance);
	FSRResourceInstance BuildBaseOutputResource(const FSRFacilityInstance& FacilityInstance, const TArray<FSRResourceInstance>& ConsumedResources) const;
	void ApplyFacilityEffects(const USRFacilityDataAsset* FacilityDataAsset, FSRResourceInstance& ResourceInstance, TArray<FSRResourceInstance>& OutAdditionalOutputs) const;
	void AddTagStack(FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag, int32 Count) const;
	void RemoveTagStack(FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag, int32 Count) const;

	UPROPERTY(Transient)
	TMap<FName, FSRFacilityInstance> FacilityInstancesByOccupantId;
};
