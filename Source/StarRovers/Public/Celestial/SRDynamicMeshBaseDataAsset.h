#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SRDynamicMeshBaseDataAsset.generated.h"

UENUM(BlueprintType)
enum class ESRDynamicMeshBaseShape : uint8
{
	CubeSphere UMETA(DisplayName = "Cube Sphere"),
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

	int32 GetClampedFaceResolution() const;
	float GetSafeBaseRadius(float FallbackRadius = 1.0f) const;
};
