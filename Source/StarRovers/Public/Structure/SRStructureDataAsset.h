#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "SRStructureDataAsset.generated.h"

class UMaterialInterface;
class UStaticMesh;

UENUM(BlueprintType)
enum class ESRStructureBuildKind : uint8
{
	Structure UMETA(DisplayName = "Structure"),
	Conveyor UMETA(DisplayName = "Conveyor"),
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

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Build", meta = (DisplayName = "BuildKind"))
	ESRStructureBuildKind BuildKind = ESRStructureBuildKind::Structure;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Conveyor", meta = (DisplayName = "ConveyorLayer"))
	int32 ConveyorLayer = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Conveyor", meta = (DisplayName = "ConveyorLayerHeight"))
	float ConveyorLayerHeight = 160.0f;
};

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Build", meta = (DisplayName = "BuildKind"))
	ESRStructureBuildKind BuildKind = ESRStructureBuildKind::Structure;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Conveyor", meta = (DisplayName = "ConveyorLayer", ClampMin = "0"))
	int32 ConveyorLayer = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Conveyor", meta = (DisplayName = "ConveyorLayerHeight", ClampMin = "0.0"))
	float ConveyorLayerHeight = 160.0f;
};
