#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorTypes.h"
#include "GameFramework/Actor.h"
#include "SRConveyorBeltActor.generated.h"

class UPCGComponent;
class UBoxComponent;
class UMaterialInterface;
class USceneComponent;
class USplineComponent;
class USplineMeshComponent;
class USRPlanetSurfaceGrid;

UCLASS(Blueprintable)
class STARROVERS_API ASRConveyorBeltActor : public AActor
{
	GENERATED_BODY()

public:
	ASRConveyorBeltActor();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Conveyor")
	bool InitializeConveyorPath(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRConveyorBeltPath& BeltPath,
		FName SplineComponentTag,
		float SurfaceOffset);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Conveyor")
	bool InitializeConveyorPaths(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const TArray<FSRConveyorBeltPath>& BeltPaths,
		FName SplineComponentTag,
		float SurfaceOffset);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Conveyor|Rendering")
	void SetConveyorGhostMode(bool bNewGhostMode, UMaterialInterface* InGhostMaterial);

	bool IsConveyorGhostGenerationPending() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "SceneRoot"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "PCGComponent"))
	TObjectPtr<UPCGComponent> PCGComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "PCGBoundsComponent"))
	TObjectPtr<UBoxComponent> PCGBoundsComponent;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Conveyor", meta = (DisplayName = "ConveyorBeltPath"))
	FSRConveyorBeltPath ConveyorBeltPath;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Conveyor", meta = (DisplayName = "ConveyorBeltPaths"))
	TArray<FSRConveyorBeltPath> ConveyorBeltPaths;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|PCG", meta = (DisplayName = "bAutoGeneratePCG"))
	bool bAutoGeneratePCG;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|PCG", meta = (DisplayName = "bRebaseGeneratedSplineMeshes"))
	bool bRebaseGeneratedSplineMeshes;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Conveyor|PCG", meta = (DisplayName = "ConveyorSplineComponentTag"))
	FName ConveyorSplineComponentTag;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Conveyor|PCG", meta = (DisplayName = "ConveyorSurfaceOffset"))
	float ConveyorSurfaceOffset;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Conveyor|Rendering", meta = (DisplayName = "bConveyorGhostMode"))
	bool bConveyorGhostMode;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ConveyorGhostMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineComponent>> ConveyorSplineComponents;

	bool bConveyorGhostGenerationPending;

private:
	USplineComponent* EnsureConveyorSplineComponent(int32 SplineIndex);
	void ClearUnusedConveyorSplineComponents(int32 FirstUnusedSplineIndex);
	bool BuildConveyorPathPoints(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRConveyorBeltPath& BeltPath,
		TArray<FVector>& OutWorldPoints,
		TArray<FVector>& OutWorldNormals) const;
	void UpdatePCGBoundsFromWorldBounds(const FBox& WorldBounds);
	void BindPCGGenerationDelegate();
	void RequestPCGGeneration();
	void HandlePCGGraphGenerated(UPCGComponent* InPCGComponent);
	void CollectAllGeneratedSplineMeshes(TArray<USplineMeshComponent*>& OutGeneratedSplineMeshes) const;
	bool HasReusableGeneratedSplineMeshes(int32 RequiredSplineMeshCount) const;
	void HideGeneratedSplineMeshes() const;
	void RebaseGeneratedSplineMeshes();
	void ApplyConveyorGhostModeToSplineMesh(USplineMeshComponent* SplineMeshComponent) const;
	void ApplyConveyorGhostModeToGeneratedMeshes() const;
};
