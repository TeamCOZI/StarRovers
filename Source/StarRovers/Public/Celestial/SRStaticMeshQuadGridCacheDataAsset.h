#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRStaticMeshQuadGridCacheDataAsset.generated.h"

class UStaticMesh;

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStaticMeshQuadGridCacheSourceQuad
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Static Mesh Quad Grid Cache")
	int32 Vertex0 = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Static Mesh Quad Grid Cache")
	int32 Vertex1 = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Static Mesh Quad Grid Cache")
	int32 Vertex2 = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Static Mesh Quad Grid Cache")
	int32 Vertex3 = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStaticMeshQuadGridCacheAddress
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Static Mesh Quad Grid Cache")
	FSRPlanetSurfaceGridCellId CellId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Static Mesh Quad Grid Cache")
	FVector2D FaceCoordinates = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Static Mesh Quad Grid Cache")
	int32 FaceResolution = 1;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStaticMeshQuadGridCacheEdgeLink
{
	GENERATED_BODY()

	UPROPERTY()
	int32 QuadIndexA = INDEX_NONE;

	UPROPERTY()
	int32 EdgeIndexA = INDEX_NONE;

	UPROPERTY()
	int32 QuadIndexB = INDEX_NONE;

	UPROPERTY()
	int32 EdgeIndexB = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStaticMeshQuadGridCacheBoundaryEdge
{
	GENERATED_BODY()

	UPROPERTY()
	int32 QuadIndex = INDEX_NONE;

	UPROPERTY()
	int32 EdgeIndex = INDEX_NONE;
};

UCLASS(BlueprintType)
class STARROVERS_API USRStaticMeshQuadGridCacheDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "StarRovers|Static Mesh Quad Grid Cache")
	bool RebuildCacheFromSourceStaticMesh();

	bool IsValidForStaticMesh(const UStaticMesh* StaticMesh, int32 InVertexCount, int32 InIndexCount, int32 InMeshSignature) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Static Mesh Quad Grid Cache", meta = (DisplayName = "SourceStaticMesh"))
	TObjectPtr<UStaticMesh> SourceStaticMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Static Mesh Quad Grid Cache", meta = (DisplayName = "bRebuildCacheNow"))
	bool bRebuildCacheNow = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Static Mesh Quad Grid Cache", meta = (DisplayName = "VertexCount"))
	int32 VertexCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Static Mesh Quad Grid Cache", meta = (DisplayName = "IndexCount"))
	int32 IndexCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Static Mesh Quad Grid Cache", meta = (DisplayName = "MeshSignature"))
	int32 MeshSignature = 0;

	UPROPERTY()
	TArray<FVector> SourceVertexPositions;

	UPROPERTY()
	TArray<FSRStaticMeshQuadGridCacheSourceQuad> SourceQuads;

	UPROPERTY()
	TArray<FSRStaticMeshQuadGridCacheAddress> QuadGridAddresses;

	UPROPERTY()
	TArray<FSRStaticMeshQuadGridCacheEdgeLink> EdgeLinks;

	UPROPERTY()
	TArray<FSRStaticMeshQuadGridCacheBoundaryEdge> BoundaryEdges;
};
