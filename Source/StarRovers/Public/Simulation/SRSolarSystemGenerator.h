#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Celestial/SRCelestialBodyDataTypes.h"
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

	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Generation")
	ASRCelestialBody* GenerateRuntimeSystem();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Generation")
	void ClearRuntimeGeneratedBodies();

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
