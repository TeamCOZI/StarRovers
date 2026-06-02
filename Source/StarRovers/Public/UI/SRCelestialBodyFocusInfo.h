#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRCelestialBodyFocusInfo.generated.h"

class AActor;

USTRUCT(BlueprintType)
struct STARROVERS_API FSRCelestialBodyFocusInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "Actor"))
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "VariableName"))
	FText VariableName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bCanConstruct"))
	bool bCanConstruct = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bHasSurfaceGrid"))
	bool bHasSurfaceGrid = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bHasHoveredSurfaceCell"))
	bool bHasHoveredSurfaceCell = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "HoveredSurfaceCellInfo"))
	FSRPlanetSurfaceGridCellInfo HoveredSurfaceCellInfo;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "HoveredSurfaceGridPatchCellIds"))
	TArray<FSRPlanetSurfaceGridCellId> HoveredSurfaceGridPatchCellIds;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bIsValid"))
	bool bIsValid = false;
};
