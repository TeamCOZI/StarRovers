#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRStructureInstanceManagerComponent.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
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

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "StructureDataAsset"))
	TObjectPtr<USRStructureDataAsset> StructureDataAsset = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "VisualKey"))
	FName VisualKey = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "InstanceIndex"))
	int32 InstanceIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure", meta = (DisplayName = "bNaturalStructure"))
	bool bNaturalStructure = false;
};

UCLASS(ClassGroup = (StarRovers), Blueprintable, meta = (BlueprintSpawnableComponent))
class STARROVERS_API USRStructureInstanceManagerComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USRStructureInstanceManagerComponent();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Structure")
	bool TryPlaceStructureOnSurfaceGrid(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& TargetCellId,
		USRStructureDataAsset* StructureDataAsset,
		FName& OutOccupantId,
		bool bNaturalStructure = false,
		bool bUseStaticMeshMaterials = false);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Structure")
	bool TryRemoveStructureAtCell(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& TargetCellId);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Structure")
	void ClearNaturalStructures(USRPlanetSurfaceGrid* SurfaceGrid);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Structure")
	void ClearAllStructures(USRPlanetSurfaceGrid* SurfaceGrid);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Structure")
	bool GetPlacedStructure(FName OccupantId, FSRPlacedStructureInstance& OutPlacedStructure) const;

private:
	struct FSRStructureVisualGroup
	{
		UHierarchicalInstancedStaticMeshComponent* Component = nullptr;
		TArray<FName> OccupantIds;
	};

	static FName MakeVisualKey(USRStructureDataAsset* StructureDataAsset, bool bUseStaticMeshMaterials);
	static FName MakeOccupantId(const FSRPlanetSurfaceGridCellId& CellId, FName StructureId, int32 SequenceNumber);
	static FTransform BuildInstanceWorldTransform(const FTransform& PlacementTransform, const FSRStructureData& StructureData);

	FSRStructureVisualGroup& FindOrCreateVisualGroup(USRStructureDataAsset* StructureDataAsset, FName VisualKey, bool bUseStaticMeshMaterials);
	void RemoveStructureByOccupantId(USRPlanetSurfaceGrid* SurfaceGrid, FName OccupantId);
	void RemoveStructuresByOccupantIds(USRPlanetSurfaceGrid* SurfaceGrid, const TArray<FName>& OccupantIds);
	void RemoveVisualInstances(FName VisualKey, const TArray<FName>& RemovedOccupantIds);
	void RebuildVisualGroup(FName VisualKey);
	void LogStructureMemoryDiagnostics(const TCHAR* Label, bool bRequestGarbageCollection, int32 AffectedStructures, int32 AffectedCells) const;

	UPROPERTY(Transient)
	TMap<FName, FSRPlacedStructureInstance> PlacedStructuresByOccupantId;

	TMap<FName, FSRStructureVisualGroup> VisualGroupsByKey;
	int32 NextStructureInstanceSequence;
};
