#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRDynamicMeshBaseDataAsset.generated.h"

UENUM(BlueprintType)
enum class ESRDynamicMeshBaseShape : uint8
{
	CubeSphere UMETA(DisplayName = "Cube Sphere"),
};

USTRUCT()
struct STARROVERS_API FSRDynamicMeshBaseSourceMetadataCell
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 CornerHash00 = 0;

	UPROPERTY()
	uint32 CornerHash10 = 0;

	UPROPERTY()
	uint32 CornerHash11 = 0;

	UPROPERTY()
	uint32 CornerHash01 = 0;

	uint32 GetCornerHash(int32 CornerIndex) const;
};

USTRUCT()
struct STARROVERS_API FSRDynamicMeshBasePrecomputedCell
{
	GENERATED_BODY()

	UPROPERTY()
	FSRPlanetSurfaceGridCellId CellId;

	UPROPERTY()
	FVector LocalCenter = FVector::ZeroVector;

	UPROPERTY()
	FVector LocalNormal = FVector::UpVector;

	UPROPERTY()
	FVector Corner00 = FVector::ZeroVector;

	UPROPERTY()
	FVector Corner10 = FVector::ZeroVector;

	UPROPERTY()
	FVector Corner11 = FVector::ZeroVector;

	UPROPERTY()
	FVector Corner01 = FVector::ZeroVector;

	UPROPERTY()
	FVector2D FaceUVMin = FVector2D::ZeroVector;

	UPROPERTY()
	FVector2D FaceUVMax = FVector2D::ZeroVector;

	UPROPERTY()
	float ApproxSurfaceArea = 0.0f;

	UPROPERTY()
	FSRPlanetSurfaceGridCellNeighbors Neighbors;

	void SetFromSurfaceGridCell(const FSRPlanetSurfaceGridCell& SourceCell);
	FSRPlanetSurfaceGridCell ToSurfaceGridCell(float Scale) const;
};

UCLASS(BlueprintType)
class STARROVERS_API USRDynamicMeshBaseDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Dynamic Mesh Base", meta = (DisplayName = "BaseShape"))
	ESRDynamicMeshBaseShape BaseShape = ESRDynamicMeshBaseShape::CubeSphere;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Dynamic Mesh Base", meta = (DisplayName = "FaceResolution", ClampMin = "1", ClampMax = "512"))
	int32 FaceResolution = 256;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Dynamic Mesh Base", meta = (DisplayName = "BaseRadius", ClampMin = "1.0"))
	float BaseRadius = 1.0f;

	UPROPERTY()
	int32 PrecomputedFaceResolution = 0;

	UPROPERTY()
	float PrecomputedBaseRadius = 1.0f;

	UPROPERTY()
	TArray<FSRDynamicMeshBasePrecomputedCell> PrecomputedCells;

	UPROPERTY()
	TArray<FSRDynamicMeshBaseSourceMetadataCell> PrecomputedSourceMetadata;

	int32 GetClampedFaceResolution() const;
	float GetSafeBaseRadius(float FallbackRadius = 1.0f) const;
	int32 GetExpectedCellCount() const;
	bool HasValidPrecomputedCells() const;
	const TArray<FSRDynamicMeshBasePrecomputedCell>* GetValidPrecomputedCells() const;
	float GetPrecomputedCellScale(float TargetRadius) const;
	bool TryGetPrecomputedBaseCells(float TargetRadius, TArray<FSRPlanetSurfaceGridCell>& OutCells) const;
	const TArray<FSRDynamicMeshBaseSourceMetadataCell>* GetValidPrecomputedSourceMetadata() const;

	UFUNCTION(CallInEditor, Category = "StarRovers|Dynamic Mesh Base", meta = (DisplayName = "Bake Precomputed Base Data"))
	void BakePrecomputedBaseData();

	UFUNCTION(CallInEditor, Category = "StarRovers|Dynamic Mesh Base", meta = (DisplayName = "Clear Precomputed Base Data"))
	void ClearPrecomputedBaseData();
};
