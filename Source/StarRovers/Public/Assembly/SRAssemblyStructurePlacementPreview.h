#pragma once

#include "Automation/SROperationalCapacityTypes.h"
#include "Conveyor/SRConveyorTypes.h"
#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRAssemblyStructurePlacementPreview.generated.h"

class USRPlanetSurfaceGrid;
class USRStructureDataAsset;

UENUM(BlueprintType)
enum class ESRStructurePlacementPreviewStatus : uint8
{
	Inactive UMETA(DisplayName = "Inactive"),
	AwaitingSurface UMETA(DisplayName = "Awaiting Surface"),
	ConveyorPath UMETA(DisplayName = "Conveyor Path"),
	Ready UMETA(DisplayName = "Ready"),
	Replacement UMETA(DisplayName = "Replacement"),
	InvalidDefinition UMETA(DisplayName = "Invalid Definition"),
	OutsideSurface UMETA(DisplayName = "Outside Surface"),
	BlockedTerrain UMETA(DisplayName = "Blocked Terrain"),
	BlockedOccupancy UMETA(DisplayName = "Blocked Occupancy"),
};

/** Player-facing summary for the structure currently under the cursor. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRStructurePlacementPreview
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Placement Preview")
	ESRStructurePlacementPreviewStatus Status = ESRStructurePlacementPreviewStatus::Inactive;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Placement Preview")
	bool bHasTarget = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Placement Preview")
	bool bCanPlace = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Placement Preview")
	bool bWillReplace = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Placement Preview")
	bool bHasCapacityData = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Placement Preview")
	bool bCapacityWarning = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Placement Preview")
	int32 FootprintCellsX = 1;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Placement Preview")
	int32 FootprintCellsY = 1;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Placement Preview")
	int32 FootprintCellCount = 1;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Placement Preview")
	int32 ReplacementStructureCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Placement Preview")
	int32 ReplacementConveyorCellCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Placement Preview")
	int32 OperationalLoad = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Placement Preview")
	int32 CurrentDemand = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Placement Preview")
	int32 TotalCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Placement Preview")
	int32 ProjectedDemand = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Placement Preview")
	float RemainingCapacity = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Placement Preview")
	FText StatusText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Placement Preview")
	FText DetailText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Placement Preview")
	FText CapacityText;
};

/** Internal payload shared by the read-only UI query and the world ghost updater. */
struct STARROVERS_API FSRStructurePlacementEvaluation
{
	FSRStructurePlacementPreview Preview;
	TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;
	TSet<FName> ReplacementStructureIds;
	TArray<FSRPlanetSurfaceGridCellId> ReplacementConveyorCellIds;
	TArray<FSRConveyorBeltPath> ReplacementConveyorBeltPaths;
};

class STARROVERS_API FSRAssemblyStructurePlacementPreviewEvaluator final
{
public:
	static FSRStructurePlacementEvaluation Evaluate(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRStructureDataAsset* StructureDataAsset,
		int32 PlacementRotationSteps);
};
