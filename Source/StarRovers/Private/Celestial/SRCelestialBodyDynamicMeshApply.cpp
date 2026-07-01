#include "Celestial/SRCelestialBody.h"

#include "SRCelestialBodyLog.h"
#include "Celestial/SRCelestialBodyDynamicMeshInternal.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Engine/StaticMesh.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "Utility/SRTimingLog.h"

using namespace StarRovers::Celestial::DynamicMesh;

bool ASRCelestialBody::ApplyPreparedCelestialBodyDynamicMesh(FSRCelestialBodyPreparedDynamicMesh&& PreparedMesh, double TotalStart)
{
	if (!PreparedMesh.bValid)
	{
		return false;
	}

	ResetDynamicMeshCellColorData();
	DynamicMeshState.ColorDataByFlatId = MoveTemp(PreparedMesh.ColorDataByFlatId);
	DynamicMeshState.SurfaceGridCells = MoveTemp(PreparedMesh.SurfaceGridCells);

	double StageStart = SRCelestialNowSeconds();
	for (int32 FaceIndex = 0; FaceIndex < PreparedMesh.FaceDynamicMeshes.Num(); ++FaceIndex)
	{
		if (UDynamicMeshComponent* FaceDynamicMeshComponent = GetDynamicMeshFaceComponent(FaceIndex))
		{
			FaceDynamicMeshComponent->SetMesh(MoveTemp(PreparedMesh.FaceDynamicMeshes[FaceIndex]));
		}
	}
	const double SetMeshMs = SRCelestialElapsedMilliseconds(StageStart);

	double SurfaceGridApplyMs = 0.0;
	if (USRPlanetSurfaceGrid* SurfaceGrid = GetSurfaceGrid())
	{
		StageStart = SRCelestialNowSeconds();
		TArray<FSRPlanetSurfaceGridCell> GeneratedGridCells = DynamicMeshState.SurfaceGridCells;
		UE::Geometry::FDynamicMesh3 GeneratedGridMesh;
		GeneratedGridMesh.EnableAttributes();
		GeneratedGridMesh.Attributes()->EnablePrimaryColors();
		SurfaceGrid->ApplyGeneratedGridBuild(MoveTemp(GeneratedGridCells), MoveTemp(GeneratedGridMesh), MoveTemp(PreparedMesh.CellIndexByFlatId));
		SurfaceGridApplyMs = SRCelestialElapsedMilliseconds(StageStart);
	}

	DynamicMeshState.MarkBuilt(PreparedMesh.BuildHash);
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.Total %.2f ms Build=%.2f ms RuntimeCache=0.00 ms SetMesh=%.2f ms SurfaceGrid=%.2f ms"),
		*GetName(),
		SRCelestialElapsedMilliseconds(TotalStart),
		PreparedMesh.BuildMilliseconds,
		SetMeshMs,
		SurfaceGridApplyMs));
	return true;
}

bool ASRCelestialBody::BuildDynamicMeshFromBaseMetadata(uint32 DynamicMeshBuildHash, double TotalStart)
{
	(void)DynamicMeshBuildHash;
	FSRCelestialBodyPreparedDynamicMesh PreparedMesh;
	if (!BuildPreparedCelestialBodyDynamicMesh(PreparedMesh))
	{
		return false;
	}
	return ApplyPreparedCelestialBodyDynamicMesh(MoveTemp(PreparedMesh), TotalStart);
}

bool ASRCelestialBody::BuildCelestialBodyDynamicMesh()
{
	FSRTimingLogSession TimingLogSession(FString::Printf(TEXT("DynamicMesh '%s'"), *GetName()));
	const double TotalStart = SRCelestialNowSeconds();
	const bool bHasMeshSource = IsValid(StaticMesh.Get()) || IsValid(DynamicMeshBaseDataAsset.Get());
	if (!IsValid(CelestialBodyDynamicMesh.Get()) || !bHasMeshSource)
	{
		return false;
	}

	const uint32 DynamicMeshBuildHash = ComputeDynamicMeshBuildHash();
	if (DynamicMeshState.HasBuildHash(DynamicMeshBuildHash))
	{
		FSRTimingLog::AddLine(FString::Printf(TEXT("DynamicMesh '%s' AlreadyBuilt %.2f ms"), *GetName(), SRCelestialElapsedMilliseconds(TotalStart)));
		return true;
	}

	auto ApplyRuntimeCacheEntry = [this, DynamicMeshBuildHash](const FSRCelestialBodyDynamicMeshRuntimeCacheEntry& CacheEntry)
	{
		const double ApplyStart = SRCelestialNowSeconds();
		ResetDynamicMeshCellColorData();

		DynamicMeshState.ColorDataByFlatId = CacheEntry.ColorDataByFlatId;
		DynamicMeshState.SurfaceGridCells = CacheEntry.SurfaceGridCells;

		for (int32 FaceIndex = 0; FaceIndex < CubeSphereFaceComponentCount; ++FaceIndex)
		{
			if (UDynamicMeshComponent* FaceDynamicMeshComponent = GetDynamicMeshFaceComponent(FaceIndex))
			{
				UE::Geometry::FDynamicMesh3 MeshCopy;
				if (CacheEntry.FaceDynamicMeshes.IsValidIndex(FaceIndex))
				{
					MeshCopy = CacheEntry.FaceDynamicMeshes[FaceIndex];
				}
				else
				{
					MeshCopy.EnableAttributes();
					MeshCopy.Attributes()->EnablePrimaryColors();
					MeshCopy.Attributes()->SetNumUVLayers(2);
				}
				FaceDynamicMeshComponent->SetMesh(MoveTemp(MeshCopy));
			}
		}

		double SurfaceGridApplyMs = 0.0;
		if (USRPlanetSurfaceGrid* SurfaceGrid = GetSurfaceGrid())
		{
			if (!DynamicMeshState.SurfaceGridCells.IsEmpty())
			{
				const double SurfaceGridApplyStart = SRCelestialNowSeconds();
				TArray<FSRPlanetSurfaceGridCell> GeneratedGridCells = DynamicMeshState.SurfaceGridCells;
				UE::Geometry::FDynamicMesh3 EmptyGridMesh;
				EmptyGridMesh.EnableAttributes();
				EmptyGridMesh.Attributes()->EnablePrimaryColors();
				SurfaceGrid->ApplyGeneratedGridBuild(MoveTemp(GeneratedGridCells), MoveTemp(EmptyGridMesh), TMap<FSRPlanetSurfaceGridCellId, int32>());
				SurfaceGridApplyMs = SRCelestialElapsedMilliseconds(SurfaceGridApplyStart);
			}
		}

		DynamicMeshState.MarkBuilt(DynamicMeshBuildHash);
		FSRTimingLog::AddLine(FString::Printf(TEXT("DynamicMesh.ApplyCache '%s' %.2f ms SurfaceGrid=%.2f ms Meshes=%d Cells=%d"), *GetName(), SRCelestialElapsedMilliseconds(ApplyStart), SurfaceGridApplyMs, CacheEntry.FaceDynamicMeshes.Num(), CacheEntry.SurfaceGridCells.Num()));
		return true;
	};

	if (bEnableGlobalDynamicMeshRuntimeCache)
	{
		if (const FSRCelestialBodyDynamicMeshRuntimeCacheEntry* CacheEntry = FindCelestialBodyDynamicMeshRuntimeCache(DynamicMeshBuildHash))
		{
			const bool bApplied = ApplyRuntimeCacheEntry(*CacheEntry);
			FSRTimingLog::AddLine(FString::Printf(TEXT("DynamicMesh '%s' RuntimeCacheHit Total=%.2f ms"), *GetName(), SRCelestialElapsedMilliseconds(TotalStart)));
			return bApplied;
		}
	}

	ResetDynamicMeshCellColorData();

	const bool bShouldGenerateMetadataTerrain =
		(BodyCategory == ESRCelestialBodyCategory::Planet || BodyCategory == ESRCelestialBodyCategory::Moon)
		&& DynamicMeshGeneration.bDynamicMeshGeneration
		&& DynamicMeshGeneration.DynamicMeshHeight > KINDA_SMALL_NUMBER;
	if (bShouldGenerateMetadataTerrain)
	{
		if (IsValid(DynamicMeshBaseDataAsset.Get()))
		{
			return BuildDynamicMeshFromBaseMetadata(DynamicMeshBuildHash, TotalStart);
		}

		UE_LOG(LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires DynamicMeshBaseDataAsset for metadata terrain generation."), *GetName());
		return false;
	}

	if (!IsValid(StaticMesh.Get()))
	{
		return false;
	}

	const double RenderDataStart = SRCelestialNowSeconds();
	const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
	if (!RenderData || RenderData->LODResources.IsEmpty())
	{
		UE_LOG(LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires render data on StaticMesh."), *GetName());
		return false;
	}

	const FStaticMeshLODResources& LODResource = RenderData->LODResources[0];
	const FPositionVertexBuffer& PositionVertexBuffer = LODResource.VertexBuffers.PositionVertexBuffer;
	const FStaticMeshVertexBuffer& StaticMeshVertexBuffer = LODResource.VertexBuffers.StaticMeshVertexBuffer;
	const FRawStaticIndexBuffer& IndexBuffer = LODResource.IndexBuffer;
	const int32 VertexCount = static_cast<int32>(PositionVertexBuffer.GetNumVertices());
	const int32 IndexCount = static_cast<int32>(IndexBuffer.GetNumIndices());
	if (VertexCount <= 0 || IndexCount < 3)
	{
		UE_LOG(LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires valid vertices and triangles on StaticMesh."), *GetName());
		return false;
	}
	FSRTimingLog::AddLine(FString::Printf(TEXT("DynamicMesh '%s' RenderData %.2f ms Vertices=%d Indices=%d"), *GetName(), SRCelestialElapsedMilliseconds(RenderDataStart), VertexCount, IndexCount));

	UE::Geometry::FDynamicMesh3 DynamicMesh;
	DynamicMesh.EnableAttributes();
	DynamicMesh.Attributes()->EnablePrimaryColors();
	DynamicMesh.Attributes()->SetNumUVLayers(2);
	DynamicMesh.Attributes()->EnableMaterialID();
	UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = DynamicMesh.Attributes()->PrimaryNormals();
	UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = DynamicMesh.Attributes()->PrimaryUV();
	UE::Geometry::FDynamicMeshUVOverlay* FeatureMaskUVOverlay = DynamicMesh.Attributes()->GetUVLayer(1);
	auto* ColorOverlay = DynamicMesh.Attributes()->PrimaryColors();
	auto* MaterialIdAttribute = DynamicMesh.Attributes()->GetMaterialID();

	TArray<int32> DynamicVertexIds;
	DynamicVertexIds.Reserve(VertexCount);
	TArray<int32> DynamicNormalIds;
	DynamicNormalIds.Reserve(VertexCount);
	TArray<int32> DynamicUVIds;
	DynamicUVIds.Reserve(VertexCount);
	TArray<int32> DynamicFeatureMaskUVIds;
	DynamicFeatureMaskUVIds.Reserve(VertexCount);
	TArray<int32> DynamicColorIds;
	DynamicColorIds.Reserve(VertexCount);

	double StageStart = SRCelestialNowSeconds();
	for (int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
	{
		const FVector SourcePosition(PositionVertexBuffer.VertexPosition(VertexIndex));
		FVector TargetPosition = SourcePosition;
		FLinearColor TargetColor = FLinearColor::White;

		DynamicVertexIds.Add(DynamicMesh.AppendVertex(FVector3d(TargetPosition)));

		FVector TargetNormal = TargetPosition.GetSafeNormal();
		if (TargetNormal.IsNearlyZero())
		{
			TargetNormal = FVector(StaticMeshVertexBuffer.VertexTangentZ(VertexIndex)).GetSafeNormal();
		}
		if (TargetNormal.IsNearlyZero())
		{
			TargetNormal = FVector::UpVector;
		}

		DynamicNormalIds.Add(NormalOverlay->AppendElement(FVector3f(TargetNormal)));
		DynamicUVIds.Add(UVOverlay ? UVOverlay->AppendElement(FVector2f(0.0f, 0.0f)) : INDEX_NONE);
		DynamicFeatureMaskUVIds.Add(FeatureMaskUVOverlay ? FeatureMaskUVOverlay->AppendElement(FVector2f(0.0f, 0.0f)) : INDEX_NONE);
		DynamicColorIds.Add(ColorOverlay->AppendElement(FVector4f(TargetColor.R, TargetColor.G, TargetColor.B, TargetColor.A)));
	}
	const double FallbackVertexMs = SRCelestialElapsedMilliseconds(StageStart);

	StageStart = SRCelestialNowSeconds();
	for (int32 Index = 0; Index + 2 < IndexCount; Index += 3)
	{
		const int32 SourceVertexIndex0 = static_cast<int32>(IndexBuffer.GetIndex(Index));
		const int32 SourceVertexIndex1 = static_cast<int32>(IndexBuffer.GetIndex(Index + 1));
		const int32 SourceVertexIndex2 = static_cast<int32>(IndexBuffer.GetIndex(Index + 2));
		if (!DynamicVertexIds.IsValidIndex(SourceVertexIndex0)
			|| !DynamicVertexIds.IsValidIndex(SourceVertexIndex1)
			|| !DynamicVertexIds.IsValidIndex(SourceVertexIndex2))
		{
			continue;
		}

		const int32 TriangleId = DynamicMesh.AppendTriangle(
			DynamicVertexIds[SourceVertexIndex0],
			DynamicVertexIds[SourceVertexIndex1],
			DynamicVertexIds[SourceVertexIndex2]);
		if (TriangleId >= 0)
		{
			NormalOverlay->SetTriangle(
				TriangleId,
				UE::Geometry::FIndex3i(
					DynamicNormalIds[SourceVertexIndex0],
					DynamicNormalIds[SourceVertexIndex1],
					DynamicNormalIds[SourceVertexIndex2]));
			if (UVOverlay)
			{
				UVOverlay->SetTriangle(
					TriangleId,
					UE::Geometry::FIndex3i(
						DynamicUVIds[SourceVertexIndex0],
						DynamicUVIds[SourceVertexIndex1],
						DynamicUVIds[SourceVertexIndex2]));
			}
			if (FeatureMaskUVOverlay)
			{
				FeatureMaskUVOverlay->SetTriangle(
					TriangleId,
					UE::Geometry::FIndex3i(
						DynamicFeatureMaskUVIds[SourceVertexIndex0],
						DynamicFeatureMaskUVIds[SourceVertexIndex1],
						DynamicFeatureMaskUVIds[SourceVertexIndex2]));
			}
			ColorOverlay->SetTriangle(
				TriangleId,
				UE::Geometry::FIndex3i(
					DynamicColorIds[SourceVertexIndex0],
					DynamicColorIds[SourceVertexIndex1],
					DynamicColorIds[SourceVertexIndex2]));
			if (MaterialIdAttribute)
			{
				MaterialIdAttribute->SetValue(TriangleId, 0);
			}
		}
	}
	const double FallbackTriangleMs = SRCelestialElapsedMilliseconds(StageStart);

	double RuntimeCacheStoreMs = 0.0;
	if (bEnableGlobalDynamicMeshRuntimeCache)
	{
		TArray<UE::Geometry::FDynamicMesh3> CachedFaceDynamicMeshes;
		CachedFaceDynamicMeshes.SetNum(CubeSphereFaceComponentCount);
		CachedFaceDynamicMeshes[0] = DynamicMesh;
		for (int32 FaceIndex = 1; FaceIndex < CubeSphereFaceComponentCount; ++FaceIndex)
		{
			CachedFaceDynamicMeshes[FaceIndex].EnableAttributes();
			CachedFaceDynamicMeshes[FaceIndex].Attributes()->EnablePrimaryColors();
			CachedFaceDynamicMeshes[FaceIndex].Attributes()->SetNumUVLayers(2);
		}
		FSRCelestialBodyDynamicMeshRuntimeCacheEntry GeneratedCacheEntry;
		GeneratedCacheEntry.FaceDynamicMeshes = CachedFaceDynamicMeshes;
		GeneratedCacheEntry.SurfaceGridCells = DynamicMeshState.SurfaceGridCells;
		GeneratedCacheEntry.ColorDataByFlatId = DynamicMeshState.ColorDataByFlatId;
		StageStart = SRCelestialNowSeconds();
		StoreCelestialBodyDynamicMeshRuntimeCache(DynamicMeshBuildHash, MoveTemp(GeneratedCacheEntry));
		RuntimeCacheStoreMs = SRCelestialElapsedMilliseconds(StageStart);
	}

	StageStart = SRCelestialNowSeconds();
	CelestialBodyDynamicMesh->SetMesh(MoveTemp(DynamicMesh));
	for (int32 FaceIndex = 1; FaceIndex < CubeSphereFaceComponentCount; ++FaceIndex)
	{
		if (UDynamicMeshComponent* FaceDynamicMeshComponent = GetDynamicMeshFaceComponent(FaceIndex))
		{
			UE::Geometry::FDynamicMesh3 EmptyMesh;
			EmptyMesh.EnableAttributes();
			EmptyMesh.Attributes()->EnablePrimaryColors();
			EmptyMesh.Attributes()->SetNumUVLayers(2);
			FaceDynamicMeshComponent->SetMesh(MoveTemp(EmptyMesh));
		}
	}
	const double SetMeshMs = SRCelestialElapsedMilliseconds(StageStart);
	DynamicMeshState.MarkBuilt(DynamicMeshBuildHash);
	FSRTimingLog::AddLine(FString::Printf(TEXT("DynamicMesh '%s' FallbackTriangle Total=%.2f ms Vertices=%.2f ms Triangles=%.2f ms RuntimeCache=%.2f ms SetMesh=%.2f ms"), *GetName(), SRCelestialElapsedMilliseconds(TotalStart), FallbackVertexMs, FallbackTriangleMs, RuntimeCacheStoreMs, SetMeshMs));
	return true;
}
