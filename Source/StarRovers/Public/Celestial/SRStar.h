#pragma once

#include "CoreMinimal.h"
#include "Automation/SRResourceDataAsset.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRStellarEvolutionTypes.h"
#include "Pattern/SRStellarPatternContract.h"
#include "Simulation/SRRunModifierTypes.h"
#include "SRStar.generated.h"

class ASRStar;
class UPointLightComponent;
class USRFacilityNetworkComponent;
class USRTimeControlSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSRStellarEvolutionStageChangedSignature, ESRStellarEvolutionStage, PreviousStage, ESRStellarEvolutionStage, NewStage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSRStellarSupernovaGameOverSignature, ASRStar*, Star);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSRStellarPatternSubmittedSignature, const FSRStellarPatternScoreResult&, ScoreResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSRStellarContractCycleSettledSignature, const FSRStellarContractCycleSettlement&, Settlement);

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStellarContractState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "EvolutionStage"))
	ESRStellarEvolutionStage EvolutionStage = ESRStellarEvolutionStage::MainSequence;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "ContractId"))
	FName ContractId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "ActiveCycleIndex"))
	int32 ActiveCycleIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "RequiredScoreThisCycle"))
	int64 RequiredScoreThisCycle = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "CurrentCycleScore"))
	int64 CurrentCycleScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "CurrentCycleBaseScore"))
	int64 CurrentCycleBaseScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "CurrentCycleBonusScore"))
	int64 CurrentCycleBonusScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "SubmittedPatternCount"))
	int64 SubmittedPatternCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "RejectedPatternCount"))
	int64 RejectedPatternCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "CurrentStellarHealth"))
	double CurrentStellarHealth = 1000.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "MaximumStellarHealth"))
	double MaximumStellarHealth = 1000.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "CurrentStellarHealthDecreasePerSecond"))
	double CurrentStellarHealthDecreasePerSecond = 0.25;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "StellarHealthDecreaseMultiplierPerPeriod"))
	double StellarHealthDecreaseMultiplierPerPeriod = 1.05;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "LastSettledHealthSecondIndex"))
	int64 LastSettledHealthSecondIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "LastSecondStellarHealthDecrease"))
	double LastSecondStellarHealthDecrease = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "LastPatternStellarHealthRestored"))
	double LastPatternStellarHealthRestored = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "LastSubmission"))
	FSRStellarPatternScoreResult LastSubmission;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "LastCycleSettlement"))
	FSRStellarContractCycleSettlement LastCycleSettlement;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "SupernovaGameOver"))
	bool bSupernovaGameOver = false;
};

namespace StarRovers::Save::Star
{
	inline constexpr int32 CurrentVersion = 3;
	inline bool IsSupportedVersion(int32 Version) { return Version == CurrentVersion; }
}

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStarSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Star")
	int32 Version = StarRovers::Save::Star::CurrentVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Star")
	ESRStellarEvolutionStage EvolutionStage = ESRStellarEvolutionStage::MainSequence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Star")
	bool bSupernovaGameOver = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Star")
	FSRStellarPatternContract ActiveContract;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Star")
	int32 ActiveContractCycleIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Star")
	int64 CurrentCycleScore = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Star")
	int64 CurrentCycleBaseScore = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Star")
	int64 CurrentCycleBonusScore = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Star")
	int64 SubmittedPatternCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Star")
	int64 RejectedPatternCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Star")
	double CurrentStellarHealth = 1000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Star")
	int64 LastSettledHealthSecondIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Star")
	double StellarHealthSecondAccumulator = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Star")
	double LastSecondStellarHealthDecrease = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Star")
	double LastPatternStellarHealthRestored = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Star")
	FSRStellarPatternScoreResult LastPatternSubmission;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Star")
	FSRStellarContractCycleSettlement LastCycleSettlement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Star")
	FSRRunModifierContext ActiveContractRunModifierContext;
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

	UPROPERTY(BlueprintAssignable, Category = "StarRovers|Star|Contract")
	FSRStellarPatternSubmittedSignature OnStellarPatternSubmitted;

	UPROPERTY(BlueprintAssignable, Category = "StarRovers|Star|Contract")
	FSRStellarContractCycleSettledSignature OnStellarContractCycleSettled;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Star|Evolution")
	ESRStellarEvolutionStage GetStellarEvolutionStage() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Star|Evolution")
	bool HasTriggeredSupernovaGameOver() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Star|Contract")
	bool SetStellarPatternContract(const FSRStellarPatternContract& NewContract);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Star|Contract")
	FSRStellarPatternContract GetStellarPatternContract() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Star|Contract")
	FSRStellarContractState GetStellarContractState() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Star|Contract")
	FSRStellarPatternScoreResult PreviewStellarPatternSubmission(const FSRResourceInstance& ResourceInstance) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Star|Contract")
	bool CanAcceptStellarFuelResource(const FSRResourceInstance& ResourceInstance) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Star|Contract")
	bool SubmitStellarPatternResource(
		const FSRResourceInstance& ResourceInstance,
		FSRStellarPatternScoreResult& OutScoreResult);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Star|Contract")
	void SetCurrentStellarHealth(double NewHealth);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Star|Contract")
	void SettleStellarContractCycle(int32 NewCurrentCycleIndex);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Star|Contract|Debug")
	bool DebugSubmitStellarPatternFromFacilityOutput(
		USRFacilityNetworkComponent* FacilityNetwork,
		FName OccupantId,
		FSRStellarPatternScoreResult& OutScoreResult,
		FSRResourceInstance& OutResourceInstance);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Save|Star")
	void ExportSaveData(FSRStarSaveData& OutSaveData) const;

	bool CanImportSaveData(const FSRStarSaveData& SaveData, FString& OutFailureReason) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Save|Star")
	bool ImportSaveData(const FSRStarSaveData& SaveData);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "StarPointLight"))
	TObjectPtr<UPointLightComponent> StarPointLight;

	float StarPointLightIntensity = 100.0f;
	FLinearColor StarPointLightColor = FLinearColor(1.0f, 0.956f, 0.84f, 1.0f);

private:
	UFUNCTION()
	void HandleGameCycleAdvanced(int32 CurrentCycleIndex);

	UFUNCTION()
	void HandleRunModifierContextChanged(const FSRRunModifierContext& Context);

	void BindToTimeControlSubsystem();
	void UnbindFromTimeControlSubsystem();
	void BindToRunModifierSubsystem();
	void UnbindFromRunModifierSubsystem();
	void SnapshotContractRunModifierContext();
	FSRStellarPatternContractModifiers ResolveContractModifiers() const;
	void ApplyStarAppearance();
	float GetEffectiveStellarHealthDeltaSeconds(float DeltaSeconds) const;
	void AdvanceStellarHealthTimer(float DeltaSeconds);
	void SettleStellarHealthSecond();
	void ResetContractRuntimeState(int32 NewActiveCycleIndex = 0);
	void SettleOneContractCycle();
	void SetStellarEvolutionStage(ESRStellarEvolutionStage NewStage);
	void AdvanceStellarEvolutionStage();
	void TriggerSupernovaGameOver();
	static int64 SaturatingAddScore(int64 Left, int64 Right);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Evolution", meta = (DisplayName = "StellarEvolutionStage", AllowPrivateAccess = "true"))
	ESRStellarEvolutionStage StellarEvolutionStage = ESRStellarEvolutionStage::MainSequence;

	UPROPERTY(Transient)
	FSRRunModifierContext ActiveContractRunModifierContext;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Evolution", meta = (DisplayName = "bSupernovaGameOver", AllowPrivateAccess = "true"))
	bool bSupernovaGameOver = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "ActiveStellarPatternContract", AllowPrivateAccess = "true"))
	FSRStellarPatternContract ActiveStellarPatternContract;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "ActiveContractCycleIndex", AllowPrivateAccess = "true"))
	int32 ActiveContractCycleIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "CurrentCycleScore", AllowPrivateAccess = "true"))
	int64 CurrentCycleScore = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "CurrentCycleBaseScore", AllowPrivateAccess = "true"))
	int64 CurrentCycleBaseScore = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "CurrentCycleBonusScore", AllowPrivateAccess = "true"))
	int64 CurrentCycleBonusScore = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "CurrentCycleSubmittedPatternCount", AllowPrivateAccess = "true"))
	int64 CurrentCycleSubmittedPatternCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "CurrentCycleRejectedPatternCount", AllowPrivateAccess = "true"))
	int64 CurrentCycleRejectedPatternCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "CurrentStellarHealth", AllowPrivateAccess = "true"))
	double CurrentStellarHealth = 1000.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "LastSettledHealthSecondIndex", AllowPrivateAccess = "true"))
	int64 LastSettledHealthSecondIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "LastSecondStellarHealthDecrease", AllowPrivateAccess = "true"))
	double LastSecondStellarHealthDecrease = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "LastPatternStellarHealthRestored", AllowPrivateAccess = "true"))
	double LastPatternStellarHealthRestored = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "LastPatternSubmission", AllowPrivateAccess = "true"))
	FSRStellarPatternScoreResult LastPatternSubmission;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Contract", meta = (DisplayName = "LastContractCycleSettlement", AllowPrivateAccess = "true"))
	FSRStellarContractCycleSettlement LastContractCycleSettlement;

	UPROPERTY(Transient)
	TWeakObjectPtr<USRTimeControlSubsystem> BoundTimeControlSubsystem;

	UPROPERTY(Transient)
	double StellarHealthSecondAccumulator = 0.0;
};
