#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorTypes.h"
#include "GameFramework/Actor.h"
#include "SRConveyorBeltActor.generated.h"

class UPCGComponent;
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
		const FSRConveyorVisualPath& VisualPath,
		FName SplineComponentTag,
		float SurfaceOffset);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Conveyor")
	bool InitializeConveyorPaths(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const TArray<FSRConveyorVisualPath>& VisualPaths,
		FName SplineComponentTag,
		float SurfaceOffset);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "SceneRoot"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "PCGComponent"))
	TObjectPtr<UPCGComponent> PCGComponent;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Conveyor", meta = (DisplayName = "ConveyorVisualPath"))
	FSRConveyorVisualPath ConveyorVisualPath;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Conveyor", meta = (DisplayName = "ConveyorVisualPaths"))
	TArray<FSRConveyorVisualPath> ConveyorVisualPaths;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|PCG", meta = (DisplayName = "bAutoGeneratePCG"))
	bool bAutoGeneratePCG;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|PCG", meta = (DisplayName = "bRebaseGeneratedSplineMeshes"))
	bool bRebaseGeneratedSplineMeshes;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Conveyor|PCG", meta = (DisplayName = "ConveyorSplineComponentTag"))
	FName ConveyorSplineComponentTag;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Conveyor|PCG", meta = (DisplayName = "ConveyorSurfaceOffset"))
	float ConveyorSurfaceOffset;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineComponent>> ConveyorSplineComponents;

private:
	USplineComponent* EnsureConveyorSplineComponent(int32 SplineIndex);
	void ClearUnusedConveyorSplineComponents(int32 FirstUnusedSplineIndex);
	bool BuildConveyorPathPoints(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRConveyorVisualPath& VisualPath,
		TArray<FVector>& OutWorldPoints,
		TArray<FVector>& OutWorldNormals) const;
	void BindPCGGenerationDelegate();
	void RequestPCGGeneration();
	void HandlePCGGraphGenerated(UPCGComponent* InPCGComponent);
	void RebaseGeneratedSplineMeshes();
};
