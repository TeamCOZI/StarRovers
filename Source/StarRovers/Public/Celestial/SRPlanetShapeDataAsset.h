#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "SRPlanetShapeDataAsset.generated.h"

#if WITH_EDITOR
struct FPropertyChangedEvent;
#endif

UCLASS(BlueprintType)
class STARROVERS_API USRPlanetShapeDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Shape", meta = (DisplayName = "Shape"))
	ESRDynamicMeshBaseShape Shape = ESRDynamicMeshBaseShape::CubeSphere;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Shape", meta = (DisplayName = "DynamicMeshBaseDataAsset", ToolTip = "Baked terrain base cells for this shape. Runtime generation reads these cells instead of converting the ocean or atmosphere meshes."))
	TObjectPtr<USRDynamicMeshBaseDataAsset> DynamicMeshBaseDataAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Shape", meta = (DisplayName = "OceanDynamicMeshBaseDataAsset", ToolTip = "Optional lower-resolution baked base cells for the ocean shell. If unset, DynamicMeshBaseDataAsset is used."))
	TObjectPtr<USRDynamicMeshBaseDataAsset> OceanDynamicMeshBaseDataAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Shape", meta = (DisplayName = "AtmosphereDynamicMeshBaseDataAsset", ToolTip = "Optional lower-resolution baked base cells for the atmosphere shell. If unset, DynamicMeshBaseDataAsset is used."))
	TObjectPtr<USRDynamicMeshBaseDataAsset> AtmosphereDynamicMeshBaseDataAsset = nullptr;

	USRDynamicMeshBaseDataAsset* GetDynamicMeshBaseDataAsset() const;
	USRDynamicMeshBaseDataAsset* GetOceanDynamicMeshBaseDataAsset() const;
	USRDynamicMeshBaseDataAsset* GetAtmosphereDynamicMeshBaseDataAsset() const;
	bool IsDynamicMeshBaseShapeCompatible() const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
