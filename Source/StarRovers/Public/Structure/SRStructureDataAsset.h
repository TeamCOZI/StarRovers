#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityDataAsset.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "Pattern/SRPatternRoutingFilter.h"
#include "SRStructureDataAsset.generated.h"

class UMaterialInterface;
class UStaticMesh;

UENUM(BlueprintType)
enum class ESRStructureBuildKind : uint8
{
	Structure UMETA(DisplayName = "Structure"),
	Conveyor UMETA(DisplayName = "Conveyor"),
};

UENUM(BlueprintType)
enum class ESRStructurePortKind : uint8
{
	Input UMETA(DisplayName = "Input"),
	Output UMETA(DisplayName = "Output"),
};

UENUM(BlueprintType)
enum class ESRStructurePortDirection : uint8
{
	Left UMETA(DisplayName = "Left"),
	Right UMETA(DisplayName = "Right"),
	Top UMETA(DisplayName = "Top"),
	Bottom UMETA(DisplayName = "Bottom"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStructurePortSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Port", meta = (DisplayName = "PortId"))
	FName PortId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Port", meta = (DisplayName = "CellOffsetX", ClampMin = "0"))
	int32 CellOffsetX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Port", meta = (DisplayName = "CellOffsetY", ClampMin = "0"))
	int32 CellOffsetY = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Port", meta = (DisplayName = "Direction"))
	ESRStructurePortDirection Direction = ESRStructurePortDirection::Left;

	// Pattern coordinates remain canonical when the physical structure port is rotated.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Port", meta = (DisplayName = "RoutingFilter", ShowOnlyInnerProperties))
	FSRPatternRoutingFilter RoutingFilter;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStructureData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "StructureId"))
	FName StructureId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "Description"))
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "StructureActorClass"))
	TSubclassOf<AActor> StructureActorClass;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "StaticMesh"))
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "Material"))
	TObjectPtr<UMaterialInterface> Material = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "GhostMaterial"))
	TObjectPtr<UMaterialInterface> GhostMaterial = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "DeleteMaterial"))
	TObjectPtr<UMaterialInterface> DeleteMaterial = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "CopyPlaceableMaterial"))
	TObjectPtr<UMaterialInterface> CopyPlaceableMaterial = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "ReplaceableMaterial"))
	TObjectPtr<UMaterialInterface> ReplaceableMaterial = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "CopyBlockedMaterial"))
	TObjectPtr<UMaterialInterface> CopyBlockedMaterial = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "MeshRelativeLocation"))
	FVector MeshRelativeLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "MeshRelativeRotation"))
	FRotator MeshRelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "MeshRelativeScale"))
	FVector MeshRelativeScale = FVector::OneVector;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Placement", meta = (DisplayName = "FootprintCellsX"))
	int32 FootprintCellsX = 1;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Placement", meta = (DisplayName = "FootprintCellsY"))
	int32 FootprintCellsY = 1;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Placement", meta = (DisplayName = "ConstructionHeightOffset"))
	float ConstructionHeightOffset = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Placement", meta = (DisplayName = "PlacementYawDegrees"))
	float PlacementYawDegrees = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Placement", meta = (DisplayName = "bAlignToSurfaceNormal"))
	bool bAlignToSurfaceNormal = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Placement", meta = (DisplayName = "bAvailableForConstruction"))
	bool bAvailableForConstruction = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Placement", meta = (DisplayName = "bDestroyableByConstruction"))
	bool bDestroyableByConstruction = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Build", meta = (DisplayName = "BuildKind"))
	ESRStructureBuildKind BuildKind = ESRStructureBuildKind::Structure;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Automation", meta = (DisplayName = "FacilityDataAsset"))
	TObjectPtr<USRFacilityDataAsset> FacilityDataAsset = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Automation", meta = (DisplayName = "bProcessReady"))
	bool bProcessReady = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Automation", meta = (DisplayName = "bDeliveryReady"))
	bool bDeliveryReady = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Deposit", meta = (DisplayName = "bIsResourceDeposit"))
	bool bIsResourceDeposit = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Deposit", meta = (DisplayName = "DepositResourceDataAsset"))
	TObjectPtr<USRResourceDataAsset> DepositResourceDataAsset = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Deposit", meta = (DisplayName = "DepositTotalAmount"))
	int32 DepositTotalAmount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Automation|Ports", meta = (DisplayName = "InputPorts"))
	TArray<FSRStructurePortSpec> InputPorts;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Automation|Ports", meta = (DisplayName = "OutputPorts"))
	TArray<FSRStructurePortSpec> OutputPorts;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Conveyor", meta = (DisplayName = "ConveyorLayer"))
	int32 ConveyorLayer = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Conveyor", meta = (DisplayName = "ConveyorLayerHeight"))
	float ConveyorLayerHeight = 160.0f;
};

namespace StarRovers::Structure
{
	FORCEINLINE int32 NormalizePlacementRotationSteps(int32 PlacementRotationSteps)
	{
		return ((PlacementRotationSteps % 4) + 4) % 4;
	}

	FORCEINLINE float PlacementRotationStepsToYawDegrees(int32 PlacementRotationSteps)
	{
		return static_cast<float>(NormalizePlacementRotationSteps(PlacementRotationSteps)) * 90.0f;
	}

	FORCEINLINE bool DoesPlacementRotationSwapFootprintAxes(int32 PlacementRotationSteps)
	{
		const int32 NormalizedSteps = NormalizePlacementRotationSteps(PlacementRotationSteps);
		return NormalizedSteps == 1 || NormalizedSteps == 3;
	}

	FORCEINLINE int32 GetRotatedFootprintCellsX(const FSRStructureData& StructureData, int32 PlacementRotationSteps)
	{
		return FMath::Max(1, DoesPlacementRotationSwapFootprintAxes(PlacementRotationSteps)
			? StructureData.FootprintCellsY
			: StructureData.FootprintCellsX);
	}

	FORCEINLINE int32 GetRotatedFootprintCellsY(const FSRStructureData& StructureData, int32 PlacementRotationSteps)
	{
		return FMath::Max(1, DoesPlacementRotationSwapFootprintAxes(PlacementRotationSteps)
			? StructureData.FootprintCellsX
			: StructureData.FootprintCellsY);
	}

	FORCEINLINE ESRStructurePortDirection RotateStructurePortDirection(ESRStructurePortDirection Direction, int32 PlacementRotationSteps)
	{
		const int32 NormalizedSteps = NormalizePlacementRotationSteps(PlacementRotationSteps);
		ESRStructurePortDirection RotatedDirection = Direction;
		for (int32 StepIndex = 0; StepIndex < NormalizedSteps; ++StepIndex)
		{
			switch (RotatedDirection)
			{
			case ESRStructurePortDirection::Left:
				RotatedDirection = ESRStructurePortDirection::Top;
				break;
			case ESRStructurePortDirection::Top:
				RotatedDirection = ESRStructurePortDirection::Right;
				break;
			case ESRStructurePortDirection::Right:
				RotatedDirection = ESRStructurePortDirection::Bottom;
				break;
			case ESRStructurePortDirection::Bottom:
				RotatedDirection = ESRStructurePortDirection::Left;
				break;
			default:
				break;
			}
		}
		return RotatedDirection;
	}

	FORCEINLINE FSRStructurePortSpec RotateStructurePortSpec(
		const FSRStructurePortSpec& PortSpec,
		const FSRStructureData& StructureData,
		int32 PlacementRotationSteps)
	{
		FSRStructurePortSpec RotatedPortSpec = PortSpec;
		const int32 NormalizedSteps = NormalizePlacementRotationSteps(PlacementRotationSteps);
		if (NormalizedSteps == 0)
		{
			return RotatedPortSpec;
		}

		const int32 FootprintCellsX = FMath::Max(1, StructureData.FootprintCellsX);
		const int32 FootprintCellsY = FMath::Max(1, StructureData.FootprintCellsY);
		switch (NormalizedSteps)
		{
		case 1:
			RotatedPortSpec.CellOffsetX = FootprintCellsY - 1 - PortSpec.CellOffsetY;
			RotatedPortSpec.CellOffsetY = PortSpec.CellOffsetX;
			break;
		case 2:
			RotatedPortSpec.CellOffsetX = FootprintCellsX - 1 - PortSpec.CellOffsetX;
			RotatedPortSpec.CellOffsetY = FootprintCellsY - 1 - PortSpec.CellOffsetY;
			break;
		case 3:
			RotatedPortSpec.CellOffsetX = PortSpec.CellOffsetY;
			RotatedPortSpec.CellOffsetY = FootprintCellsX - 1 - PortSpec.CellOffsetX;
			break;
		default:
			break;
		}
		RotatedPortSpec.Direction = RotateStructurePortDirection(PortSpec.Direction, NormalizedSteps);
		return RotatedPortSpec;
	}
}

UCLASS(BlueprintType)
class STARROVERS_API USRStructureDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	USRStructureDataAsset();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Structure")
	FSRStructureData BuildData() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "StructureId"))
	FName StructureId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "Description"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Class", meta = (DisplayName = "StructureActorClass"))
	TSubclassOf<AActor> StructureActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "StaticMesh"))
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "Material"))
	TObjectPtr<UMaterialInterface> Material = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "GhostMaterial"))
	TObjectPtr<UMaterialInterface> GhostMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "DeleteMaterial"))
	TObjectPtr<UMaterialInterface> DeleteMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "CopyPlaceableMaterial"))
	TObjectPtr<UMaterialInterface> CopyPlaceableMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "ReplaceableMaterial"))
	TObjectPtr<UMaterialInterface> ReplaceableMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "CopyBlockedMaterial"))
	TObjectPtr<UMaterialInterface> CopyBlockedMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "MeshRelativeLocation"))
	FVector MeshRelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "MeshRelativeRotation"))
	FRotator MeshRelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "MeshRelativeScale"))
	FVector MeshRelativeScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Placement", meta = (DisplayName = "FootprintCellsX", ClampMin = "1"))
	int32 FootprintCellsX = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Placement", meta = (DisplayName = "FootprintCellsY", ClampMin = "1"))
	int32 FootprintCellsY = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Placement", meta = (DisplayName = "ConstructionHeightOffset", ClampMin = "0.0"))
	float ConstructionHeightOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Placement", meta = (DisplayName = "PlacementYawDegrees"))
	float PlacementYawDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Placement", meta = (DisplayName = "bAlignToSurfaceNormal"))
	bool bAlignToSurfaceNormal = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Placement", meta = (DisplayName = "bAvailableForConstruction"))
	bool bAvailableForConstruction = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Placement", meta = (DisplayName = "bDestroyableByConstruction"))
	bool bDestroyableByConstruction = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Build", meta = (DisplayName = "BuildKind"))
	ESRStructureBuildKind BuildKind = ESRStructureBuildKind::Structure;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Automation", meta = (DisplayName = "FacilityDataAsset"))
	TObjectPtr<USRFacilityDataAsset> FacilityDataAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Automation", meta = (DisplayName = "bProcessReady"))
	bool bProcessReady = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Automation", meta = (DisplayName = "bDeliveryReady"))
	bool bDeliveryReady = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource Deposit", meta = (DisplayName = "bIsResourceDeposit"))
	bool bIsResourceDeposit = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource Deposit", meta = (DisplayName = "DepositResourceDataAsset"))
	TObjectPtr<USRResourceDataAsset> DepositResourceDataAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource Deposit", meta = (DisplayName = "DepositTotalAmount", ClampMin = "0"))
	int32 DepositTotalAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Automation|Ports", meta = (DisplayName = "InputPorts"))
	TArray<FSRStructurePortSpec> InputPorts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Automation|Ports", meta = (DisplayName = "OutputPorts"))
	TArray<FSRStructurePortSpec> OutputPorts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Conveyor", meta = (DisplayName = "ConveyorLayer", ClampMin = "0"))
	int32 ConveyorLayer = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Conveyor", meta = (DisplayName = "ConveyorLayerHeight", ClampMin = "0.0"))
	float ConveyorLayerHeight = 160.0f;
};
