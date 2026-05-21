#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Celestial/SRCelestialBody.h"
#include "SRSolarSystemGenerator.generated.h"

class UMaterialInterface;
class USRMoonDataAsset;
class USRPlanetDataAsset;
class USRStarDataAsset;
class USceneComponent;
class UStaticMesh;

USTRUCT()
struct FSRCelestialBodyGenerateRequest
{
	GENERATED_BODY()

	UPROPERTY()
	FSRCelestialBodySpec BodySpec;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BodyMesh = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BodyMaterial = nullptr;

	UPROPERTY()
	TSubclassOf<ASRCelestialBody> BodyClass;
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

private:
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
	FVector ComputeOrbitWorldLocation(const AActor* ParentBody, float OrbitRadius, float StartingPhaseDegrees) const;
	void SpawnPlanets(ASRCelestialBody* ParentStar, const USRStarDataAsset* SourceStarDataAsset, FRandomStream& RandomStream, TArray<TObjectPtr<ASRCelestialBody>>& OutGeneratedPlanets);
	void SpawnMoons(ASRCelestialBody* ParentPlanet, FRandomStream& RandomStream, TArray<TObjectPtr<ASRCelestialBody>>& OutGeneratedMoons);
	void DestroyTrackedActor(TObjectPtr<ASRCelestialBody>& ActorToDestroy);
	void DestroyTrackedActors(TArray<TObjectPtr<ASRCelestialBody>>& ActorsToDestroy);

	UPROPERTY(Transient)
	TObjectPtr<ASRCelestialBody> RuntimeStarBody;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASRCelestialBody>> RuntimePlanetBodies;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASRCelestialBody>> RuntimeMoonBodies;
};
