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

	UFUNCTION(BlueprintCallable, Category = "Star Rovers|Generation")
	ASRCelestialBody* GenerateRuntimeSystem();

	UFUNCTION(BlueprintCallable, Category = "Star Rovers|Generation")
	void ClearRuntimeGeneratedBodies();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star Rovers|Generation Seed", meta = (ClampMin = "0"))
	int32 GenerationSeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star Rovers|Celestial Body Class")
	TSubclassOf<ASRCelestialBody> StarClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star Rovers|Celestial Body Class")
	TSubclassOf<ASRCelestialBody> PlanetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star Rovers|Celestial Body Data Assets")
	TArray<TObjectPtr<USRStarDataAsset>> StarDataAssets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star Rovers|Celestial Body Data Assets")
	TArray<TObjectPtr<USRPlanetDataAsset>> PlanetDataAssets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star Rovers|Celestial Body Data Assets")
	TArray<TObjectPtr<USRMoonDataAsset>> MoonDataAssets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star Rovers|Celestial Body Count", meta = (ClampMin = "0"))
	int32 MinPlanet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star Rovers|Celestial Body Count", meta = (ClampMin = "0"))
	int32 MaxPlanet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star Rovers|Celestial Body Count", meta = (ClampMin = "0"))
	int32 MinMoon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star Rovers|Celestial Body Count", meta = (ClampMin = "0"))
	int32 MaxMoon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star Rovers|Orbit", meta = (ClampMin = "0.0"))
	float PlanetInitialOrbit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star Rovers|Orbit", meta = (ClampMin = "0.0"))
	float PlanetOrbitIncrease;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star Rovers|Orbit", meta = (ClampMin = "0.0"))
	float MoonInitialOrbit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star Rovers|Orbit", meta = (ClampMin = "0.0"))
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
