#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Celestial/SRCelestialBodyData.h"
#include "SRSolarSystemGenerator.generated.h"

class ASRCelestialBody;
class USRMoonDataAsset;
class USRPlanetDataAsset;
class USRStarDataAsset;
class USRLoadingScreenWidget;
class USceneComponent;

USTRUCT()
struct FSRCelestialBodyGenerateRequest
{
	GENERATED_BODY()

	UPROPERTY()
	FSRCelestialBodyData BodyData;

	UPROPERTY()
	TSubclassOf<ASRCelestialBody> BodyClass;
};

struct FSRPreparedBodyTimingDetail
{
	FString BodyName;
	double Milliseconds = 0.0;
	TArray<FString> DetailLines;
};

UCLASS(Blueprintable)
class STARROVERS_API ASRSolarSystemGenerator : public AActor
{
	GENERATED_BODY()

public:
	ASRSolarSystemGenerator();

	virtual void PostLoad() override;
	virtual void BeginPlay() override;
	virtual void Destroyed() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Generation")
	ASRCelestialBody* GenerateRuntimeSystem();

	/**
	 * Rebuilds the complete runtime system from an explicit root seed.
	 * The configured randomize flag and editor seed are restored afterwards.
	 * Intended for deterministic replay, balance soak tests, and future run bootstrap.
	 */
	UFUNCTION(BlueprintCallable, Category = "StarRovers|Generation")
	ASRCelestialBody* GenerateRuntimeSystemForSeed(int32 RuntimeGenerationSeed);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Generation")
	void ClearRuntimeGeneratedBodies();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Generation")
	bool IsRuntimeSystemGenerationInProgress() const
	{
		return bRuntimeGenerationInProgress;
	}

	const TArray<TObjectPtr<USRPlanetDataAsset>>& GetPlanetEnvironmentCatalog() const
	{
		return PlanetDataAssets;
	}

	int32 GetMinimumUniquePlanetTypes() const
	{
		return MinimumUniquePlanetTypes;
	}

	int32 GetMinimumPlanetCount() const
	{
		return FMath::Max(0, MinPlanet);
	}

	int32 GetMaximumPlanetCount() const
	{
		return FMath::Max(GetMinimumPlanetCount(), MaxPlanet);
	}

	int32 GetMinimumMoonCount() const
	{
		return FMath::Max(0, MinMoon);
	}

	int32 GetMaximumMoonCount() const
	{
		return FMath::Max(GetMinimumMoonCount(), MaxMoon);
	}

	UFUNCTION(BlueprintPure, Category = "StarRovers|Generation")
	int32 GetLastRuntimeGenerationSeed() const
	{
		return LastRuntimeGenerationSeed;
	}

	const TArray<FName>& GetRequiredSystemResourceRuleIds() const
	{
		return RequiredSystemResourceRuleIds;
	}

#if WITH_EDITOR
	void ConfigurePlanetEnvironmentCatalogForEditor(
		const TArray<USRPlanetDataAsset*>& InPlanetDataAssets,
		int32 InMinimumUniquePlanetTypes,
		int32 InMinPlanet,
		int32 InMaxPlanet,
		int32 InMinMoon,
		int32 InMaxMoon,
		const TArray<FName>& InRequiredSystemResourceRuleIds);
#endif

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "SceneRoot"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|GenerationSeed", meta = (DisplayName = "GenerationSeed", ClampMin = "0"))
	int32 GenerationSeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|GenerationSeed", meta = (DisplayName = "bRandomizeGenerationSeedEachRun"))
	bool bRandomizeGenerationSeedEachRun;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|CelestialBodyClass", meta = (DisplayName = "StarClass"))
	TSubclassOf<ASRCelestialBody> StarClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|CelestialBodyClass", meta = (DisplayName = "PlanetClass"))
	TSubclassOf<ASRCelestialBody> PlanetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|CelestialBodyDataAssets", meta = (DisplayName = "StarDataAssets"))
	TArray<TObjectPtr<USRStarDataAsset>> StarDataAssets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|CelestialBodyDataAssets", meta = (DisplayName = "PlanetDataAssets"))
	TArray<TObjectPtr<USRPlanetDataAsset>> PlanetDataAssets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|CelestialBodyDataAssets", meta = (DisplayName = "MinimumUniquePlanetTypes", ClampMin = "0", ToolTip = "한 Run에서 먼저 중복 없이 선택할 최소 행성 환경 수입니다. 사용 가능한 후보나 실제 행성 수보다 크면 가능한 범위로 자동 제한됩니다."))
	int32 MinimumUniquePlanetTypes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Natural Structures", meta = (DisplayName = "RequiredSystemResourceRuleIds", ToolTip = "Every generated Solar System must collectively expose each listed effective resource rule. Empty disables portfolio coverage selection."))
	TArray<FName> RequiredSystemResourceRuleIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|CelestialBodyDataAssets", meta = (DisplayName = "MoonDataAssets"))
	TArray<TObjectPtr<USRMoonDataAsset>> MoonDataAssets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|CelestialBodyCount", meta = (DisplayName = "MinPlanet", ClampMin = "0"))
	int32 MinPlanet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|CelestialBodyCount", meta = (DisplayName = "MaxPlanet", ClampMin = "0"))
	int32 MaxPlanet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|CelestialBodyCount", meta = (DisplayName = "MinMoon", ClampMin = "0"))
	int32 MinMoon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|CelestialBodyCount", meta = (DisplayName = "MaxMoon", ClampMin = "0"))
	int32 MaxMoon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Orbit", meta = (DisplayName = "PlanetInitialOrbit", ClampMin = "0.0"))
	float PlanetInitialOrbit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Orbit", meta = (DisplayName = "PlanetOrbitIncrease", ClampMin = "0.0"))
	float PlanetOrbitIncrease;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Orbit", meta = (DisplayName = "MoonInitialOrbit", ClampMin = "0.0"))
	float MoonInitialOrbit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Orbit", meta = (DisplayName = "MoonOrbitIncrease", ClampMin = "0.0"))
	float MoonOrbitIncrease;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Orbit", meta = (DisplayName = "PlanetOrbitPeriods", ClampMin = "0.0", ToolTip = "Orbit periods assigned by generated planet order. The array is sized from the resolved MaxPlanet count, and generation uses only the prefix for the actual planet count."))
	TArray<float> PlanetOrbitPeriods;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Orbit", meta = (DisplayName = "MoonOrbitPeriods", ClampMin = "0.0", ToolTip = "Orbit periods assigned by generated moon order for each parent planet. The array is sized from the resolved MaxMoon count, and generation uses only the prefix for the actual moon count."))
	TArray<float> MoonOrbitPeriods;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Natural Structures", meta = (DisplayName = "bGenerateNaturalStructures"))
	bool bGenerateNaturalStructures;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Loading", meta = (DisplayName = "LoadingScreenWidgetClass"))
	TSubclassOf<USRLoadingScreenWidget> LoadingScreenWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Loading", meta = (DisplayName = "LoadingScreenZOrder"))
	int32 LoadingScreenZOrder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Diagnostics", meta = (DisplayName = "bEnableMemoryDiagnostics"))
	bool bEnableMemoryDiagnostics;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Performance", meta = (DisplayName = "bParallelDynamicMeshPreparation", ToolTip = "Prepare planet and moon dynamic mesh build data in parallel batches during runtime system generation. The final component and surface grid apply step still runs on the game thread."))
	bool bParallelDynamicMeshPreparation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Performance", meta = (DisplayName = "DynamicMeshPreparationMaxConcurrency", ClampMin = "1", EditCondition = "bParallelDynamicMeshPreparation", ToolTip = "Maximum number of celestial body dynamic mesh build-data tasks to run at once. Values above the number of prepared bodies have no extra effect."))
	int32 DynamicMeshPreparationMaxConcurrency;

private:
	struct FSRAsyncGenerationStageTiming
	{
		FString Name;
		double Milliseconds = 0.0;
	};

	void StartRuntimeSystemGenerationWithLoadingScreen();
	void GenerateRuntimeSystemDeferred();
	void BeginRuntimeSystemGenerationDeferred();
	void ContinueRuntimeSystemGenerationAfterClear();
	void ContinueRuntimeDynamicMeshPreparation();
	void ContinueRuntimeNaturalStructureGeneration();
	void FinishRuntimeSystemGeneration();
	void ShowLoadingScreen();
	void HideLoadingScreen();
	void UpdateLoadingProgress(float Progress, const FText& StatusText);
	void ScheduleLoadingGenerationStep(void (ASRSolarSystemGenerator::*StepFunction)(), float DelaySeconds = 0.0f);
	void LogAsyncGenerationStageTiming(const TCHAR* StageName, double Milliseconds, const FString& Suffix = FString());
	void EnsureMemoryDiagnosticTrackedClasses() const;
	void LogMemoryDiagnosticsSnapshot(const FString& Label) const;
	void NormalizeOrbitPeriodSettings();
	float ResolvePlanetOrbitPeriod(int32 PlanetIndex) const;
	float ResolveMoonOrbitPeriod(int32 MoonIndex) const;
	ASRCelestialBody* SpawnPrimaryStar(FRandomStream& RandomStream, const USRStarDataAsset*& OutSelectedStarDataAsset);
	ASRCelestialBody* SpawnOrbitingBody(const TSubclassOf<ASRCelestialBody>& BodyClass, const FSRCelestialBodyGenerateRequest& CelestialBodyRequest, ASRCelestialBody* ParentBody);
	void BuildOrbitingBodyRequests(
		ASRCelestialBody* ParentBody,
		int32 RequestedBodyCount,
		const TArray<FSRCelestialBodyGenerateRequest>& CandidateCelestialBodyRequests,
		FRandomStream& RandomStream,
		TArray<FSRCelestialBodyGenerateRequest>& OutResolvedCelestialBodyRequests) const;
	bool TrySolvePackedOrbitRadii(ASRCelestialBody* ParentBody, const TArray<FSRCelestialBodyGenerateRequest>& CelestialBodyRequests, TArray<float>& OutOrbitRadii) const;
	void EnsureParentGravityContainsOrbitingBody(ASRCelestialBody* ParentBody, const ASRCelestialBody* OrbitingBody) const;
	FVector ComputeOrbitWorldLocation(const AActor* ParentBody, float OrbitRadius, float InitialAngleDegrees) const;
	void SpawnPlanets(ASRCelestialBody* ParentStar, const USRStarDataAsset* SourceStarDataAsset, FRandomStream& RandomStream, TArray<TObjectPtr<ASRCelestialBody>>& OutGeneratedPlanets);
	void SpawnMoons(ASRCelestialBody* ParentPlanet, FRandomStream& RandomStream, TArray<TObjectPtr<ASRCelestialBody>>& OutGeneratedMoons);
	void PrepareRuntimeGeneratedDynamicMeshes();
	void GenerateRuntimeNaturalStructures(int32 RuntimeGenerationSeed);
	void GenerateNaturalStructuresForBody(ASRCelestialBody* Body, FRandomStream& RandomStream);
	void DestroyRuntimeNaturalStructures();
	void DestroyTrackedActor(TObjectPtr<ASRCelestialBody>& ActorToDestroy);
	void DestroyTrackedActors(TArray<TObjectPtr<ASRCelestialBody>>& ActorsToDestroy);

	UPROPERTY(Transient)
	TObjectPtr<ASRCelestialBody> RuntimeStarBody;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASRCelestialBody>> RuntimePlanetBodies;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASRCelestialBody>> RuntimeMoonBodies;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> RuntimeNaturalStructureActors;

	UPROPERTY(Transient)
	TObjectPtr<USRLoadingScreenWidget> LoadingScreenWidget;

	FTimerHandle DeferredGenerateRuntimeSystemTimerHandle;
	bool bRuntimeGenerationInProgress = false;
	double AsyncGenerationTotalStart = 0.0;
	double AsyncCurrentStageStart = 0.0;
	double AsyncDynamicMeshTotalStart = 0.0;
	double AsyncNaturalStructuresTotalStart = 0.0;
	TArray<FSRAsyncGenerationStageTiming> AsyncGenerationStageTimings;
	FRandomStream AsyncGenerationRandomStream;
	FRandomStream AsyncNaturalStructureRandomStream;
	int32 AsyncRuntimeGenerationSeed = 0;

	UPROPERTY(Transient)
	int32 LastRuntimeGenerationSeed = 0;

	const USRStarDataAsset* AsyncSelectedStarDataAsset = nullptr;
	int32 AsyncPrepareBodyIndex = 0;
	int32 AsyncPreparePlanetCount = 0;
	int32 AsyncPrepareMoonCount = 0;
	double AsyncPreparePlanetTotalMs = 0.0;
	double AsyncPrepareMoonTotalMs = 0.0;
	double AsyncPrepareSlowestBodyMs = 0.0;
	FString AsyncPrepareSlowestBodyName;
	TArray<FString> AsyncPrepareSlowestBodyDetailLines;
	TArray<FSRPreparedBodyTimingDetail> AsyncPrepareBodyTimingDetails;
	int32 AsyncNaturalPlanetIndex = 0;
	int32 AsyncNaturalPlanetCount = 0;
	double AsyncNaturalPlanetTotalMs = 0.0;
	double AsyncNaturalSlowestBodyMs = 0.0;
	FString AsyncNaturalSlowestBodyName;
};
