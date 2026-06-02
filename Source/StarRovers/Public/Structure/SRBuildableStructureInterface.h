#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRBuildableStructureInterface.generated.h"

class USRStructureDataAsset;

UINTERFACE(BlueprintType)
class STARROVERS_API USRBuildableStructureInterface : public UInterface
{
	GENERATED_BODY()
};

class STARROVERS_API ISRBuildableStructureInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "StarRovers|Structure")
	void ApplyStructureDataAsset(USRStructureDataAsset* StructureDataAsset);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "StarRovers|Structure")
	void SetStructureGhostMode(bool bNewGhostMode);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "StarRovers|Structure")
	bool CanPlaceOnSurfaceCell(const FSRPlanetSurfaceGridCellInfo& SurfaceCellInfo) const;
};
