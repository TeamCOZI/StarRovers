#include "Celestial/SRCelestialBody.h"

#include "Utility/SRLog.h"
#include "SRCelestialBodyLog.h"
#include "Celestial/SRCelestialBodyDynamicMeshPipeline.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Engine/StaticMesh.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "Utility/SRTimingLog.h"

using namespace StarRovers::Celestial::DynamicMesh;

namespace
{
	struct FSRCelestialBodyStaticMeshFallbackRenderData
	{
		const FPositionVertexBuffer* PositionVertexBuffer = nullptr;
		const FStaticMeshVertexBuffer* StaticMeshVertexBuffer = nullptr;
		const FRawStaticIndexBuffer* IndexBuffer = nullptr;
		int32 VertexCount = 0;
		int32 IndexCount = 0;
	};

	struct FSRCelestialBodyStaticMeshFallbackAttributeIds
	{
		TArray<int32> VertexIds;
		TArray<int32> NormalIds;
		TArray<int32> UVIds;
		TArray<int32> FeatureMaskUVIds;
		TArray<int32> ColorIds;

		void Reserve(int32 VertexCount)
		{
			VertexIds.Reserve(VertexCount);
			NormalIds.Reserve(VertexCount);
			UVIds.Reserve(VertexCount);
			FeatureMaskUVIds.Reserve(VertexCount);
			ColorIds.Reserve(VertexCount);
		}
	};

	bool TryGetStaticMeshFallbackRenderData(
		UStaticMesh* StaticMesh,
		const FString& BodyName,
		FSRCelestialBodyStaticMeshFallbackRenderData& OutRenderData,
		double& OutRenderDataMs)
	{
		const double RenderDataStart = GetDynamicMeshTimingSeconds();
		if (!IsValid(StaticMesh))
		{
			return false;
		}

		const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
		if (!RenderData || RenderData->LODResources.IsEmpty())
		{
			SR_LOG(DynamicMesh, LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires render data on StaticMesh."), *BodyName);
			return false;
		}

		const FStaticMeshLODResources& LODResource = RenderData->LODResources[0];
		OutRenderData.PositionVertexBuffer = &LODResource.VertexBuffers.PositionVertexBuffer;
		OutRenderData.StaticMeshVertexBuffer = &LODResource.VertexBuffers.StaticMeshVertexBuffer;
		OutRenderData.IndexBuffer = &LODResource.IndexBuffer;
		OutRenderData.VertexCount = static_cast<int32>(OutRenderData.PositionVertexBuffer->GetNumVertices());
		OutRenderData.IndexCount = static_cast<int32>(OutRenderData.IndexBuffer->GetNumIndices());
		if (OutRenderData.VertexCount <= 0 || OutRenderData.IndexCount < 3)
		{
			SR_LOG(DynamicMesh, LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires valid vertices and triangles on StaticMesh."), *BodyName);
			return false;
		}

		OutRenderDataMs = GetDynamicMeshTimingElapsedMilliseconds(RenderDataStart);
		return true;
	}

	void InitializeStaticMeshFallbackDynamicMesh(UE::Geometry::FDynamicMesh3& DynamicMesh)
	{
		DynamicMesh.EnableAttributes();
		DynamicMesh.Attributes()->EnablePrimaryColors();
		DynamicMesh.Attributes()->SetNumUVLayers(2);
		DynamicMesh.Attributes()->EnableMaterialID();
	}

	void AppendStaticMeshFallbackVertices(
		const FSRCelestialBodyStaticMeshFallbackRenderData& RenderData,
		UE::Geometry::FDynamicMesh3& DynamicMesh,
		FSRCelestialBodyStaticMeshFallbackAttributeIds& AttributeIds)
	{
		UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = DynamicMesh.Attributes()->PrimaryNormals();
		UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = DynamicMesh.Attributes()->PrimaryUV();
		UE::Geometry::FDynamicMeshUVOverlay* FeatureMaskUVOverlay = DynamicMesh.Attributes()->GetUVLayer(1);
		auto* ColorOverlay = DynamicMesh.Attributes()->PrimaryColors();

		AttributeIds.Reserve(RenderData.VertexCount);
		for (int32 VertexIndex = 0; VertexIndex < RenderData.VertexCount; ++VertexIndex)
		{
			const FVector SourcePosition(RenderData.PositionVertexBuffer->VertexPosition(VertexIndex));
			const FVector TargetPosition = SourcePosition;
			const FLinearColor TargetColor = FLinearColor::White;

			AttributeIds.VertexIds.Add(DynamicMesh.AppendVertex(FVector3d(TargetPosition)));

			FVector TargetNormal = TargetPosition.GetSafeNormal();
			if (TargetNormal.IsNearlyZero())
			{
				TargetNormal = FVector(RenderData.StaticMeshVertexBuffer->VertexTangentZ(VertexIndex)).GetSafeNormal();
			}
			if (TargetNormal.IsNearlyZero())
			{
				TargetNormal = FVector::UpVector;
			}

			AttributeIds.NormalIds.Add(NormalOverlay->AppendElement(FVector3f(TargetNormal)));
			AttributeIds.UVIds.Add(UVOverlay ? UVOverlay->AppendElement(FVector2f(0.0f, 0.0f)) : INDEX_NONE);
			AttributeIds.FeatureMaskUVIds.Add(FeatureMaskUVOverlay ? FeatureMaskUVOverlay->AppendElement(FVector2f(0.0f, 0.0f)) : INDEX_NONE);
			AttributeIds.ColorIds.Add(ColorOverlay->AppendElement(FVector4f(TargetColor.R, TargetColor.G, TargetColor.B, TargetColor.A)));
		}
	}

	void AppendStaticMeshFallbackTriangles(
		const FSRCelestialBodyStaticMeshFallbackRenderData& RenderData,
		const FSRCelestialBodyStaticMeshFallbackAttributeIds& AttributeIds,
		UE::Geometry::FDynamicMesh3& DynamicMesh)
	{
		UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = DynamicMesh.Attributes()->PrimaryNormals();
		UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = DynamicMesh.Attributes()->PrimaryUV();
		UE::Geometry::FDynamicMeshUVOverlay* FeatureMaskUVOverlay = DynamicMesh.Attributes()->GetUVLayer(1);
		auto* ColorOverlay = DynamicMesh.Attributes()->PrimaryColors();
		auto* MaterialIdAttribute = DynamicMesh.Attributes()->GetMaterialID();

		for (int32 Index = 0; Index + 2 < RenderData.IndexCount; Index += 3)
		{
			const int32 SourceVertexIndex0 = static_cast<int32>(RenderData.IndexBuffer->GetIndex(Index));
			const int32 SourceVertexIndex1 = static_cast<int32>(RenderData.IndexBuffer->GetIndex(Index + 1));
			const int32 SourceVertexIndex2 = static_cast<int32>(RenderData.IndexBuffer->GetIndex(Index + 2));
			if (!AttributeIds.VertexIds.IsValidIndex(SourceVertexIndex0)
				|| !AttributeIds.VertexIds.IsValidIndex(SourceVertexIndex1)
				|| !AttributeIds.VertexIds.IsValidIndex(SourceVertexIndex2))
			{
				continue;
			}

			const int32 TriangleId = DynamicMesh.AppendTriangle(
				AttributeIds.VertexIds[SourceVertexIndex0],
				AttributeIds.VertexIds[SourceVertexIndex1],
				AttributeIds.VertexIds[SourceVertexIndex2]);
			if (TriangleId < 0)
			{
				continue;
			}

			NormalOverlay->SetTriangle(
				TriangleId,
				UE::Geometry::FIndex3i(
					AttributeIds.NormalIds[SourceVertexIndex0],
					AttributeIds.NormalIds[SourceVertexIndex1],
					AttributeIds.NormalIds[SourceVertexIndex2]));
			if (UVOverlay)
			{
				UVOverlay->SetTriangle(
					TriangleId,
					UE::Geometry::FIndex3i(
						AttributeIds.UVIds[SourceVertexIndex0],
						AttributeIds.UVIds[SourceVertexIndex1],
						AttributeIds.UVIds[SourceVertexIndex2]));
			}
			if (FeatureMaskUVOverlay)
			{
				FeatureMaskUVOverlay->SetTriangle(
					TriangleId,
					UE::Geometry::FIndex3i(
						AttributeIds.FeatureMaskUVIds[SourceVertexIndex0],
						AttributeIds.FeatureMaskUVIds[SourceVertexIndex1],
						AttributeIds.FeatureMaskUVIds[SourceVertexIndex2]));
			}
			ColorOverlay->SetTriangle(
				TriangleId,
				UE::Geometry::FIndex3i(
					AttributeIds.ColorIds[SourceVertexIndex0],
					AttributeIds.ColorIds[SourceVertexIndex1],
					AttributeIds.ColorIds[SourceVertexIndex2]));
			if (MaterialIdAttribute)
			{
				MaterialIdAttribute->SetValue(TriangleId, 0);
			}
		}
	}

	double StoreStaticMeshFallbackRuntimeCache(
		uint32 DynamicMeshBuildHash,
		const UE::Geometry::FDynamicMesh3& DynamicMesh,
		const FSRCelestialBodyDynamicMeshRuntimeState& DynamicMeshState)
	{
		if (!bEnableGlobalDynamicMeshRuntimeCache)
		{
			return 0.0;
		}

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
		const double StageStart = GetDynamicMeshTimingSeconds();
		StoreCelestialBodyDynamicMeshRuntimeCache(DynamicMeshBuildHash, MoveTemp(GeneratedCacheEntry));
		return GetDynamicMeshTimingElapsedMilliseconds(StageStart);
	}

	void InitializeEmptyStaticMeshFallbackFaceMesh(UE::Geometry::FDynamicMesh3& EmptyMesh)
	{
		EmptyMesh.EnableAttributes();
		EmptyMesh.Attributes()->EnablePrimaryColors();
		EmptyMesh.Attributes()->SetNumUVLayers(2);
	}

	double ApplyStaticMeshFallbackDynamicMeshes(
		UDynamicMeshComponent* PrimaryDynamicMeshComponent,
		const TArray<TObjectPtr<UDynamicMeshComponent>>& DynamicMeshFaces,
		UE::Geometry::FDynamicMesh3&& DynamicMesh)
	{
		const double StageStart = GetDynamicMeshTimingSeconds();
		if (IsValid(PrimaryDynamicMeshComponent))
		{
			PrimaryDynamicMeshComponent->SetMesh(MoveTemp(DynamicMesh));
		}

		for (int32 FaceIndex = 1; FaceIndex < CubeSphereFaceComponentCount; ++FaceIndex)
		{
			UDynamicMeshComponent* FaceDynamicMeshComponent = DynamicMeshFaces.IsValidIndex(FaceIndex)
				? DynamicMeshFaces[FaceIndex].Get()
				: nullptr;
			if (!IsValid(FaceDynamicMeshComponent))
			{
				continue;
			}

			UE::Geometry::FDynamicMesh3 EmptyMesh;
			InitializeEmptyStaticMeshFallbackFaceMesh(EmptyMesh);
			FaceDynamicMeshComponent->SetMesh(MoveTemp(EmptyMesh));
		}

		return GetDynamicMeshTimingElapsedMilliseconds(StageStart);
	}
}

bool ASRCelestialBody::BuildDynamicMeshFromStaticMeshFallback(uint32 DynamicMeshBuildHash, double TotalStart)
{
	FSRCelestialBodyStaticMeshFallbackRenderData RenderData;
	double RenderDataMs = 0.0;
	if (!TryGetStaticMeshFallbackRenderData(StaticMesh.Get(), GetName(), RenderData, RenderDataMs))
	{
		return false;
	}
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' RenderData %.2f ms Vertices=%d Indices=%d"),
		*GetName(),
		RenderDataMs,
		RenderData.VertexCount,
		RenderData.IndexCount));

	UE::Geometry::FDynamicMesh3 DynamicMesh;
	InitializeStaticMeshFallbackDynamicMesh(DynamicMesh);

	FSRCelestialBodyStaticMeshFallbackAttributeIds AttributeIds;
	double StageStart = GetDynamicMeshTimingSeconds();
	AppendStaticMeshFallbackVertices(RenderData, DynamicMesh, AttributeIds);
	const double FallbackVertexMs = GetDynamicMeshTimingElapsedMilliseconds(StageStart);

	StageStart = GetDynamicMeshTimingSeconds();
	AppendStaticMeshFallbackTriangles(RenderData, AttributeIds, DynamicMesh);
	const double FallbackTriangleMs = GetDynamicMeshTimingElapsedMilliseconds(StageStart);

	const double RuntimeCacheStoreMs = StoreStaticMeshFallbackRuntimeCache(DynamicMeshBuildHash, DynamicMesh, DynamicMeshState);
	const double SetMeshMs = ApplyStaticMeshFallbackDynamicMeshes(
		CelestialBodyDynamicMesh.Get(),
		CelestialBodyDynamicMeshFaces,
		MoveTemp(DynamicMesh));

	DynamicMeshState.MarkBuilt(DynamicMeshBuildHash);
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' FallbackTriangle Total=%.2f ms Vertices=%.2f ms Triangles=%.2f ms RuntimeCache=%.2f ms SetMesh=%.2f ms"),
		*GetName(),
		GetDynamicMeshTimingElapsedMilliseconds(TotalStart),
		FallbackVertexMs,
		FallbackTriangleMs,
		RuntimeCacheStoreMs,
		SetMeshMs));
	return true;
}
