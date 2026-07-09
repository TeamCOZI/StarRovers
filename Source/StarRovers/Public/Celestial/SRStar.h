#pragma once

#include "CoreMinimal.h"
#include "Automation/SRResourceDataAsset.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRStellarEvolutionTypes.h"
#include "SRStar.generated.h"

class ASRStar;
class UPointLightComponent;
class USRFacilityNetworkComponent;
class USRTimeControlSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSRStellarEvolutionStageChangedSignature, ESRStellarEvolutionStage, PreviousStage, ESRStellarEvolutionStage, NewStage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSRStellarSupernovaGameOverSignature, ASRStar*, Star);

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStellarFuelState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "EvolutionStage"))
	ESRStellarEvolutionStage EvolutionStage = ESRStellarEvolutionStage::MainSequence;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "StoredFuel"))
	double StoredFuel = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "InitialStageFuel"))
	double InitialStageFuel = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "InitialFuelDecreasePerSecond"))
	double InitialFuelDecreasePerSecond = 50.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "CurrentFuelDecreasePerSecond"))
	double RequiredFuelPerCycle = 10.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "NextFuelDecreaseMultiplier"))
	double RequirementGrowthPerCycle = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "LastFuelDecreaseRateCycleIndex"))
	int32 LastFuelDecreaseRateCycleIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "RedGiantPressure"))
	double RedGiantPressure = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "RedGiantPressurePerMissingFuel"))
	double RedGiantPressurePerMissingFuel = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "LastSettledSecondIndex"))
	int32 LastSettledSecondIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "LastSecondFuelConsumed"))
	double LastSecondFuelConsumed = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "LastSecondFuelDecrease"))
	double LastSecondFuelDecrease = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "LastSecondFuelDeficit"))
	double LastSecondFuelDeficit = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "bLastSecondSurvived"))
	bool bLastSecondSurvived = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "bSupernovaGameOver"))
	bool bSupernovaGameOver = false;
};

UCLASS(Blueprintable)
class STARROVERS_API ASRStar : public ASRCelestialBody
{
	GENERATED_BODY()

public:
	ASRStar();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetData(const FSRCelestialBodyData& NewData) override;
	virtual void ApplyData() override;
	virtual FSRCelestialBodyData GetData() const override;

	UPROPERTY(BlueprintAssignable, Category = "StarRovers|Star|Evolution")
	FSRStellarEvolutionStageChangedSignature OnStellarEvolutionStageChanged;

	UPROPERTY(BlueprintAssignable, Category = "StarRovers|Star|Evolution")
	FSRStellarSupernovaGameOverSignature OnStellarSupernovaGameOver;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Star|Evolution")
	ESRStellarEvolutionStage GetStellarEvolutionStage() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Star|Evolution")
	bool HasTriggeredSupernovaGameOver() const;

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
	void SettleStellarFuelSecond(int32 CurrentSecondIndex);

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
	float GetEffectiveStellarFuelDeltaSeconds(float DeltaSeconds) const;
	void AdvanceStellarFuelTimer(float DeltaSeconds);
	void UpdateStellarFuelDecreaseRateForCycle(int32 CurrentCycleIndex);
	double CalculateNextStellarFuelDecrease(double PreviousCycleFuelDecrease, int32 PreviousCycleIndex) const;
	void SetStellarEvolutionStage(ESRStellarEvolutionStage NewStage);
	void AdvanceStellarEvolutionStage();
	void TriggerSupernovaGameOver();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Evolution", meta = (DisplayName = "StellarEvolutionStage", AllowPrivateAccess = "true"))
	ESRStellarEvolutionStage StellarEvolutionStage = ESRStellarEvolutionStage::MainSequence;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Evolution", meta = (DisplayName = "bSupernovaGameOver", AllowPrivateAccess = "true"))
	bool bSupernovaGameOver = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "InitialStageStellarFuel", AllowPrivateAccess = "true"))
	double InitialStageStellarFuel = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "InitialStellarFuelDecreasePerSecond", AllowPrivateAccess = "true"))
	double InitialStellarFuelDecreasePerSecond = 50.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "StoredStellarFuel", AllowPrivateAccess = "true"))
	double StoredStellarFuel = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "CurrentStellarFuelDecreasePerSecond", AllowPrivateAccess = "true"))
	double RequiredStellarFuelPerCycle = 10.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "NextStellarFuelDecreaseMultiplier", AllowPrivateAccess = "true"))
	double StellarFuelRequirementGrowthPerCycle = 1.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "LastFuelDecreaseRateCycleIndex", AllowPrivateAccess = "true"))
	int32 LastFuelDecreaseRateCycleIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "RedGiantPressure", AllowPrivateAccess = "true"))
	double RedGiantPressure = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "RedGiantPressurePerMissingFuel", AllowPrivateAccess = "true"))
	double RedGiantPressurePerMissingFuel = 1.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "LastSettledSecondIndex", AllowPrivateAccess = "true"))
	int32 LastSettledSecondIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "LastSecondFuelConsumed", AllowPrivateAccess = "true"))
	double LastSecondFuelConsumed = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "LastSecondFuelDecrease", AllowPrivateAccess = "true"))
	double LastSecondFuelDecrease = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "LastSecondFuelDeficit", AllowPrivateAccess = "true"))
	double LastSecondFuelDeficit = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "bLastSecondSurvived", AllowPrivateAccess = "true"))
	bool bLastSecondSurvived = true;

	UPROPERTY(Transient)
	float StellarFuelSecondAccumulator = 0.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<USRTimeControlSubsystem> BoundTimeControlSubsystem;
};
