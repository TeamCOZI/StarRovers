#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Structure/SRBuildableStructureInterface.h"
#include "Structure/SRStructureDataAsset.h"
#include "SRStructure.generated.h"

class USceneComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class STARROVERS_API ASRStructure : public AActor, public ISRBuildableStructureInterface
{
	GENERATED_BODY()

public:
	ASRStructure();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Structure")
	void SetInitialStructureDataAsset(USRStructureDataAsset* NewStructureDataAsset);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Structure")
	USRStructureDataAsset* GetStructureDataAsset() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Structure")
	FSRStructureData GetStructureData() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Structure")
	bool IsStructureGhostMode() const;

	virtual void ApplyStructureDataAsset_Implementation(USRStructureDataAsset* StructureDataAsset) override;
	virtual void SetStructureGhostMode_Implementation(bool bNewGhostMode) override;
	virtual bool CanPlaceOnSurfaceCell_Implementation(const FSRPlanetSurfaceGridCellInfo& SurfaceCellInfo) const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "SceneRoot"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "StructureStaticMesh"))
	TObjectPtr<UStaticMeshComponent> StructureStaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "InitialStructureDataAsset"))
	TObjectPtr<USRStructureDataAsset> InitialStructureDataAsset;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "AppliedStructureData"))
	FSRStructureData AppliedStructureData;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "bStructureGhostMode"))
	bool bStructureGhostMode = false;

private:
	void ApplyStructureVisuals();
	UMaterialInterface* ResolveActiveMaterial() const;
	FVector ResolveSurfaceSnappedMeshRelativeLocation() const;
	bool HasValidStructureData() const;

	UPROPERTY(Transient)
	TObjectPtr<USRStructureDataAsset> CurrentStructureDataAsset;
};
