#pragma once

#include "CoreMinimal.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRConveyorTypes.generated.h"

UENUM(BlueprintType)
enum class ESRConveyorGridDirection : uint8
{
	None UMETA(DisplayName = "None"),
	NegativeU UMETA(DisplayName = "NegativeU"),
	PositiveU UMETA(DisplayName = "PositiveU"),
	NegativeV UMETA(DisplayName = "NegativeV"),
	PositiveV UMETA(DisplayName = "PositiveV"),
};

UENUM(BlueprintType)
enum class ESRConveyorSegmentShape : uint8
{
	End UMETA(DisplayName = "End"),
	Straight UMETA(DisplayName = "Straight"),
	Corner UMETA(DisplayName = "Corner"),
	LiftUp UMETA(DisplayName = "LiftUp"),
	LiftDown UMETA(DisplayName = "LiftDown"),
	Bridge UMETA(DisplayName = "Bridge"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRConveyorLaneKey
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor", meta = (DisplayName = "CellId"))
	FSRPlanetSurfaceGridCellId CellId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor", meta = (DisplayName = "Layer", ClampMin = "0"))
	int32 Layer = 0;

	bool operator==(const FSRConveyorLaneKey& Other) const
	{
		return CellId == Other.CellId && Layer == Other.Layer;
	}
};

FORCEINLINE uint32 GetTypeHash(const FSRConveyorLaneKey& LaneKey)
{
	return HashCombine(::GetTypeHash(LaneKey.CellId), ::GetTypeHash(LaneKey.Layer));
}

USTRUCT(BlueprintType)
struct STARROVERS_API FSRConveyorSegment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor", meta = (DisplayName = "Lane"))
	FSRConveyorLaneKey Lane;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor", meta = (DisplayName = "InputDirection"))
	ESRConveyorGridDirection InputDirection = ESRConveyorGridDirection::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor", meta = (DisplayName = "OutputDirection"))
	ESRConveyorGridDirection OutputDirection = ESRConveyorGridDirection::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor", meta = (DisplayName = "Shape"))
	ESRConveyorSegmentShape Shape = ESRConveyorSegmentShape::End;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor", meta = (DisplayName = "NetworkId"))
	FName NetworkId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor", meta = (DisplayName = "StructureDataAsset"))
	TObjectPtr<USRStructureDataAsset> StructureDataAsset = nullptr;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRConveyorVisualPath
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor", meta = (DisplayName = "CellIds"))
	TArray<FSRPlanetSurfaceGridCellId> CellIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor", meta = (DisplayName = "Layer", ClampMin = "0"))
	int32 Layer = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor", meta = (DisplayName = "LayerHeight", ClampMin = "0.0"))
	float LayerHeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor", meta = (DisplayName = "NetworkId"))
	FName NetworkId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor", meta = (DisplayName = "StructureDataAsset"))
	TObjectPtr<USRStructureDataAsset> StructureDataAsset = nullptr;
};
