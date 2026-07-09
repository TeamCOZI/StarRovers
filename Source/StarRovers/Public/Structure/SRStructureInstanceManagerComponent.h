#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRStructureInstanceManagerComponent.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UTextRenderComponent;
class USRPlanetSurfaceGrid;
class USRStructureDataAsset;

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPlacedStructureInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "OccupantId"))
	FName OccupantId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "StructureId"))
	FName StructureId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "OriginCellId"))
	FSRPlanetSurfaceGridCellId OriginCellId;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "FootprintCellIds"))
	TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "PlacementRotationSteps"))
	int32 PlacementRotationSteps = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "StructureDataAsset"))
	TObjectPtr<USRStructureDataAsset> StructureDataAsset = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "VisualKey"))
	FName VisualKey = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "InstanceIndex"))
	int32 InstanceIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "bNaturalStructure"))
	bool bNaturalStructure = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "bUseStaticMeshMaterials"))
	bool bUseStaticMeshMaterials = false;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceDepositInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Deposit", meta = (DisplayName = "OccupantId"))
	FName OccupantId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Deposit", meta = (DisplayName = "StructureId"))
	FName StructureId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Deposit", meta = (DisplayName = "ResourceDataAsset"))
	TObjectPtr<USRResourceDataAsset> ResourceDataAsset = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Deposit", meta = (DisplayName = "ResourceId"))
	FName ResourceId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Deposit", meta = (DisplayName = "TotalAmount"))
	int32 TotalAmount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Deposit", meta = (DisplayName = "RemainingAmount"))
	int32 RemainingAmount = 0;
};

UCLASS(ClassGroup = (StarRovers), Blueprintable, meta = (BlueprintSpawnableComponent))
class STARROVERS_API USRStructureInstanceManagerComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USRStructureInstanceManagerComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Structure")
	bool TryPlaceStructureOnSurfaceGrid(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& TargetCellId,
		USRStructureDataAsset* StructureDataAsset,
		FName& OutOccupantId,
		bool bNaturalStructure = false,
		bool bUseStaticMeshMaterials = false,
		int32 PlacementRotationSteps = 0);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Structure")
	bool TryRemoveStructureAtCell(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& TargetCellId);

	bool TryRemoveStructureByOccupantId(
		USRPlanetSurfaceGrid* SurfaceGrid,
		FName OccupantId,
		FSRPlacedStructureInstance& OutRemovedStructure);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Structure")
	void ClearNaturalStructures(USRPlanetSurfaceGrid* SurfaceGrid);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Structure")
	void ClearAllStructures(USRPlanetSurfaceGrid* SurfaceGrid);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Structure")
	bool GetPlacedStructure(FName OccupantId, FSRPlacedStructureInstance& OutPlacedStructure) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Structure")
	void GetPlacedStructures(TArray<FSRPlacedStructureInstance>& OutPlacedStructures) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Structure")
	bool CanDestroyNaturalStructureForConstruction(FName OccupantId) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Structure")
	bool CanDestroyStructureForConstruction(FName OccupantId) const;

	bool CanBuildOverCellsForConstruction(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
		TSet<FName>& OutDestructibleOccupantIds) const;
	void SetGhostedStructures(const TSet<FName>& OccupantIds);
	void ClearGhostedStructures();
	void SetDeletePreviewedStructures(const TSet<FName>& OccupantIds);
	void SetConstructionReplacementPreviewedStructures(const TSet<FName>& OccupantIds);
	void ClearDeletePreviewedStructures();
	bool RemoveNonResourceStructuresByOccupantIds(USRPlanetSurfaceGrid* SurfaceGrid, const TSet<FName>& OccupantIds);
	bool RemoveConstructionDestructibleStructuresByOccupantIds(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const TSet<FName>& OccupantIds,
		TArray<FSRPlacedStructureInstance>* OutRemovedStructures = nullptr);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Structure")
	bool TryRemoveConstructionDestructibleNaturalStructuresAtCells(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const TArray<FSRPlanetSurfaceGridCellId>& CellIds);

	bool TryRemoveConstructionDestructibleStructuresAtCells(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
		TArray<FSRPlacedStructureInstance>* OutRemovedStructures = nullptr);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Resource Deposit")
	bool GetResourceDepositInstance(FName OccupantId, FSRResourceDepositInstance& OutResourceDeposit) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Resource Deposit")
	bool FindAdjacentResourceDeposit(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const TArray<FSRPlanetSurfaceGridCellId>& SourceFootprintCellIds,
		FSRResourceDepositInstance& OutResourceDeposit) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Resource Deposit")
	bool TryHarvestResourceDeposit(
		USRPlanetSurfaceGrid* SurfaceGrid,
		FName DepositOccupantId,
		FSRResourceInstance& OutResourceInstance,
		FSRResourceDepositInstance& OutUpdatedResourceDeposit);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Name Label", meta = (DisplayName = "bShowStructureNameLabels"))
	bool bShowStructureNameLabels;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Name Label", meta = (DisplayName = "bShowNaturalStructureNameLabels"))
	bool bShowNaturalStructureNameLabels;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Name Label", meta = (DisplayName = "StructureNameLabelHeightOffset", ClampMin = "0.0"))
	float StructureNameLabelHeightOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Name Label", meta = (DisplayName = "StructureNameLabelWorldSize", ClampMin = "1.0"))
	float StructureNameLabelWorldSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Name Label", meta = (DisplayName = "StructureNameLabelColor"))
	FLinearColor StructureNameLabelColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Name Label", meta = (DisplayName = "StructureNameLabelMaxDrawDistance", ClampMin = "0.0"))
	float StructureNameLabelMaxDrawDistance;

private:
	struct FSRStructureVisualGroup
	{
		UHierarchicalInstancedStaticMeshComponent* Component = nullptr;
		TArray<FName> OccupantIds;
	};

	enum class ESRStructureVisualOverride : uint8
	{
		None,
		Ghost,
		Delete,
	};

	static FName MakeVisualKey(USRStructureDataAsset* StructureDataAsset, bool bUseStaticMeshMaterials);
	static FName MakeVisualKey(USRStructureDataAsset* StructureDataAsset, bool bUseStaticMeshMaterials, ESRStructureVisualOverride VisualOverride);
	static FName MakeOccupantId(const FSRPlanetSurfaceGridCellId& CellId, FName StructureId, int32 SequenceNumber);
	static FTransform BuildInstanceWorldTransform(const FTransform& PlacementTransform, const FSRStructureData& StructureData);

	FSRStructureVisualGroup& FindOrCreateVisualGroup(USRStructureDataAsset* StructureDataAsset, FName VisualKey, bool bUseStaticMeshMaterials, ESRStructureVisualOverride VisualOverride = ESRStructureVisualOverride::None);
	bool IsDeletePreviewTarget(FName OccupantId) const;
	void RemoveStructureByOccupantId(USRPlanetSurfaceGrid* SurfaceGrid, FName OccupantId);
	void RemoveStructuresByOccupantIds(USRPlanetSurfaceGrid* SurfaceGrid, const TArray<FName>& OccupantIds);
	void RemoveVisualInstances(FName VisualKey, const TArray<FName>& RemovedOccupantIds);
	void RebuildVisualGroup(FName VisualKey);
	void RefreshVisualGroupsForPreviewState();
	void RegisterResourceDeposit(const FSRPlacedStructureInstance& PlacedStructure, const FSRStructureData& StructureData);
	void RefreshStructureNameLabel(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlacedStructureInstance& PlacedStructure);
	void RefreshAllStructureNameLabels(USRPlanetSurfaceGrid* SurfaceGrid);
	void DestroyStructureNameLabel(FName OccupantId);
	void DestroyAllStructureNameLabels();
	void UpdateStructureNameLabelTransforms(USRPlanetSurfaceGrid* SurfaceGrid);
	bool ResolveStructureNameLabelLocation(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlacedStructureInstance& PlacedStructure,
		FVector& OutWorldLocation) const;
	FText BuildStructureNameLabelText(const FSRPlacedStructureInstance& PlacedStructure) const;
	void UpdateNameLabelTickEnabled();
	void LogStructureMemoryDiagnostics(const TCHAR* Label, bool bRequestGarbageCollection, int32 AffectedStructures, int32 AffectedCells) const;

	UPROPERTY(Transient)
	TMap<FName, FSRPlacedStructureInstance> PlacedStructuresByOccupantId;

	UPROPERTY(Transient)
	TMap<FName, FSRResourceDepositInstance> ResourceDepositsByOccupantId;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTextRenderComponent>> StructureNameLabelsByOccupantId;

	TMap<FName, FSRStructureVisualGroup> VisualGroupsByKey;
	TSet<FName> GhostedStructureOccupantIds;
	TSet<FName> DeletePreviewedStructureOccupantIds;
	int32 NextStructureInstanceSequence;
};
