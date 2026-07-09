#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRAssemblySurfaceState.generated.h"

class USRPlanetSurfaceGrid;

USTRUCT()
struct STARROVERS_API FSRAssemblySurfaceState
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> ActiveAssemblySurfaceGrid = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> LastHoveredSampleSurfaceGrid = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> LastPublishedHoveredSurfaceGrid = nullptr;

	UPROPERTY(Transient)
	FVector2D LastHoveredSampleMousePosition = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	bool bHasLastHoveredSampleMousePosition = false;

	UPROPERTY(Transient)
	bool bHasLastPublishedHoveredCellInfo = false;

	UPROPERTY(Transient)
	FSRPlanetSurfaceGridCellId LastPublishedHoveredCellId;

	void ResetHoverSampleCache()
	{
		LastHoveredSampleSurfaceGrid = nullptr;
		LastHoveredSampleMousePosition = FVector2D::ZeroVector;
		bHasLastHoveredSampleMousePosition = false;
	}

	void ResetPublishedHoveredCellInfo()
	{
		bHasLastPublishedHoveredCellInfo = false;
		LastPublishedHoveredSurfaceGrid = nullptr;
		LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
	}
};
