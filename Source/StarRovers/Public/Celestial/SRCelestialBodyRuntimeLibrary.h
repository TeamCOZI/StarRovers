#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UI/SRCelestialBodyFocusInfo.h"
#include "SRCelestialBodyRuntimeLibrary.generated.h"

class AActor;
class USRPlanetSurfaceGrid;

UCLASS()
class STARROVERS_API USRCelestialBodyRuntimeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	static bool IsCelestialBodyActor(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	static bool IsCelestialStarActor(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	static bool GetCelestialParentBody(const AActor* Actor, AActor*& OutParentBody);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	static bool GetCelestialRootBody(const AActor* Actor, AActor*& OutRootBody);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	static bool GetCelestialPrimaryStar(const AActor* Actor, AActor*& OutPrimaryStar);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	static bool GetCelestialOrbitRadius(const AActor* Actor, float& OutOrbitRadius);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	static bool GetCelestialOrbitPeriod(const AActor* Actor, float& OutOrbitPeriod);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	static bool GetCelestialStartingPhase(const AActor* Actor, float& OutStartingPhaseDegrees);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	static float GetCelestialFocusZoomDistance(const AActor* Actor, float CameraFieldOfViewDegrees = 90.0f, float FramingPadding = 3.0f);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	static FText GetCelestialDisplayName(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	static bool GetCelestialCanConstruct(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	static float GetCelestialApproximateRadius(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	static USRPlanetSurfaceGrid* FindPlanetSurfaceGrid(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	static FSRCelestialBodyFocusInfo BuildCelestialBodyFocusInfo(const AActor* Actor);
};
