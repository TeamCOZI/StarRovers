#pragma once

#include "CoreMinimal.h"
#include "Automation/SRResourceDataAsset.h"
#include "Celestial/SRCelestialBody.h"
#include "SRStar.generated.h"

class UPointLightComponent;
class USRFacilityNetworkComponent;
class USRTimeControlSubsystem;

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStellarFuelState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "StoredFuel"))
	double StoredFuel = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "RequiredFuelPerCycle"))
	double RequiredFuelPerCycle = 10.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "RequirementGrowthPerCycle"))
	double RequirementGrowthPerCycle = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "RedGiantPressure"))
	double RedGiantPressure = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "RedGiantPressurePerMissingFuel"))
	double RedGiantPressurePerMissingFuel = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "LastSettledCycleIndex"))
	int32 LastSettledCycleIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "LastCycleFuelConsumed"))
	double LastCycleFuelConsumed = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "LastCycleFuelDeficit"))
	double LastCycleFuelDeficit = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "bLastCycleMetRequirement"))
	bool bLastCycleMetRequirement = true;
};

UCLASS(Blueprintable)
class STARROVERS_API ASRStar : public ASRCelestialBody
{
	GENERATED_BODY()

public:
	ASRStar();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetData(const FSRCelestialBodyData& NewData) override;
	virtual void ApplyData() override;
	virtual FSRCelestialBodyData GetData() const override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Star|Fuel")
	void AddStellarFuel(double FuelAmount);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Star|Fuel")
	bool CanAcceptStellarFuelResource(const FSRResourceInstance& ResourceInstance) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Star|Fuel")
	double CalculateStellarFuelValueForResource(const FSRResourceInstance& ResourceInstance) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Star|Fuel")
	bool DeliverStellarFuelResource(const FSRResourceInstance& ResourceInstance, double& OutFuelAmount);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Star|Fuel|Debug")
	bool DebugDeliverStellarFuelFromFacilityOutput(
		USRFacilityNetworkComponent* FacilityNetwork,
		FName OccupantId,
		double& OutFuelAmount,
		FSRResourceInstance& OutResourceInstance);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Star|Fuel")
	void SetStoredStellarFuel(double NewStoredFuel);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Star|Fuel")
	void SetStellarFuelRequirement(double NewRequiredFuelPerCycle, double NewRequirementGrowthPerCycle);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Star|Fuel")
	void SetRedGiantPressure(double NewRedGiantPressure);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Star|Fuel")
	void SettleStellarFuelCycle(int32 CurrentCycleIndex);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Star|Fuel")
	FSRStellarFuelState GetStellarFuelState() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "StarPointLight"))
	TObjectPtr<UPointLightComponent> StarPointLight;

	float StarPointLightIntensity = 100.0f;

	FLinearColor StarPointLightColor = FLinearColor(1.0f, 0.956f, 0.84f, 1.0f);

private:
	UFUNCTION()
	void HandleGameCycleAdvanced(int32 CurrentCycleIndex);

	void BindToTimeControlSubsystem();
	void UnbindFromTimeControlSubsystem();
	void ApplyStarAppearance();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "StoredStellarFuel", AllowPrivateAccess = "true"))
	double StoredStellarFuel = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "RequiredStellarFuelPerCycle", AllowPrivateAccess = "true"))
	double RequiredStellarFuelPerCycle = 10.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "StellarFuelRequirementGrowthPerCycle", AllowPrivateAccess = "true"))
	double StellarFuelRequirementGrowthPerCycle = 1.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "RedGiantPressure", AllowPrivateAccess = "true"))
	double RedGiantPressure = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "RedGiantPressurePerMissingFuel", AllowPrivateAccess = "true"))
	double RedGiantPressurePerMissingFuel = 1.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "LastSettledCycleIndex", AllowPrivateAccess = "true"))
	int32 LastSettledCycleIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "LastCycleFuelConsumed", AllowPrivateAccess = "true"))
	double LastCycleFuelConsumed = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "LastCycleFuelDeficit", AllowPrivateAccess = "true"))
	double LastCycleFuelDeficit = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "bLastCycleMetRequirement", AllowPrivateAccess = "true"))
	bool bLastCycleMetRequirement = true;

	UPROPERTY(Transient)
	TWeakObjectPtr<USRTimeControlSubsystem> BoundTimeControlSubsystem;
};
