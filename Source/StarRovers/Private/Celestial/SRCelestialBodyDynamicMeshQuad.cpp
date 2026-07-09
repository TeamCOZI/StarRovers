#include "Celestial/SRCelestialBodyDynamicMeshPipeline.h"

#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"

namespace StarRovers::Celestial::DynamicMesh
{
FSRCelestialBodyDynamicMeshQuadRenderData AppendFlatColoredDynamicMeshQuad(
	TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes,
	TMap<FSRCelestialBodyDynamicMeshTerrainVertexKey, int32>& WeldedVertexIds,
	int32 MeshComponentIndex,
	const FVector& Point0,
	const FVector& Point1,
	const FVector& Point2,
	const FVector& Point3,
	const FLinearColor& SurfaceColor,
	int32 MaterialId,
	bool bDoubleSided,
	const FSRCelestialBodyDynamicMeshTerrainVertexKey* VertexKeys,
	const FVector* NormalReferenceDirectionOverride,
	bool bAllowUnweldedFallbackForFailedTriangles)
{
	FSRCelestialBodyDynamicMeshQuadRenderData RenderData;
	MeshComponentIndex = 0;
	if (!FaceDynamicMeshes.IsValidIndex(MeshComponentIndex))
	{
		return RenderData;
	}

	UE::Geometry::FDynamicMesh3& TargetDynamicMesh = FaceDynamicMeshes[MeshComponentIndex];
	UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = TargetDynamicMesh.Attributes()->PrimaryNormals();
	UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = TargetDynamicMesh.Attributes()->PrimaryUV();
	UE::Geometry::FDynamicMeshUVOverlay* FeatureMaskUVOverlay = TargetDynamicMesh.Attributes()->NumUVLayers() > 1
		? TargetDynamicMesh.Attributes()->GetUVLayer(1)
		: nullptr;
	auto* ColorOverlay = TargetDynamicMesh.Attributes()->PrimaryColors();
	auto* MaterialIdAttribute = TargetDynamicMesh.Attributes()->GetMaterialID();
	if (!NormalOverlay || !ColorOverlay)
	{
		return RenderData;
	}

	FVector QuadPoints[4] = { Point0, Point1, Point2, Point3 };
	int32 InputEdgeToFeatureMaskEdgeIndex[4] = { 0, 1, 2, 3 };
	FSRCelestialBodyDynamicMeshTerrainVertexKey ResolvedVertexKeys[4];
	if (VertexKeys)
	{
		for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
		{
			ResolvedVertexKeys[CornerIndex] = VertexKeys[CornerIndex];
		}
	}

	const FVector QuadCenter = (Point0 + Point1 + Point2 + Point3) * 0.25f;
	const FVector OutwardDirection = NormalReferenceDirectionOverride
		? NormalReferenceDirectionOverride->GetSafeNormal()
		: QuadCenter.GetSafeNormal();
	FVector QuadNormal = FVector::CrossProduct(QuadPoints[1] - QuadPoints[0], QuadPoints[2] - QuadPoints[0]).GetSafeNormal();
	if (!OutwardDirection.IsNearlyZero() && FVector::DotProduct(QuadNormal, OutwardDirection) < 0.0f)
	{
		Swap(QuadPoints[1], QuadPoints[3]);
		if (VertexKeys)
		{
			Swap(ResolvedVertexKeys[1], ResolvedVertexKeys[3]);
		}
		InputEdgeToFeatureMaskEdgeIndex[0] = 3;
		InputEdgeToFeatureMaskEdgeIndex[1] = 2;
		InputEdgeToFeatureMaskEdgeIndex[2] = 1;
		InputEdgeToFeatureMaskEdgeIndex[3] = 0;
		QuadNormal *= -1.0f;
	}
	if (QuadNormal.IsNearlyZero())
	{
		QuadNormal = OutwardDirection.IsNearlyZero() ? FVector::UpVector : OutwardDirection;
	}

	auto FindOrAppendVertex = [&TargetDynamicMesh, &WeldedVertexIds](const FVector& Position, const FSRCelestialBodyDynamicMeshTerrainVertexKey* VertexKey)
	{
		const FSRCelestialBodyDynamicMeshTerrainVertexKey ResolvedVertexKey = VertexKey ? *VertexKey : MakeCelestialBodyDynamicMeshTerrainVertexKey(Position);
		if (const int32* ExistingVertexId = WeldedVertexIds.Find(ResolvedVertexKey))
		{
			return *ExistingVertexId;
		}

		const int32 NewVertexId = TargetDynamicMesh.AppendVertex(FVector3d(Position));
		WeldedVertexIds.Add(ResolvedVertexKey, NewVertexId);
		return NewVertexId;
	};

	const int32 Vertex0 = FindOrAppendVertex(QuadPoints[0], VertexKeys ? &ResolvedVertexKeys[0] : nullptr);
	const int32 Vertex1 = FindOrAppendVertex(QuadPoints[1], VertexKeys ? &ResolvedVertexKeys[1] : nullptr);
	const int32 Vertex2 = FindOrAppendVertex(QuadPoints[2], VertexKeys ? &ResolvedVertexKeys[2] : nullptr);
	const int32 Vertex3 = FindOrAppendVertex(QuadPoints[3], VertexKeys ? &ResolvedVertexKeys[3] : nullptr);

	const int32 Normal0 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
	const int32 Normal1 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
	const int32 Normal2 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
	const int32 Normal3 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
	const int32 Color0 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
	const int32 Color1 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
	const int32 Color2 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
	const int32 Color3 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
	const int32 UV0 = UVOverlay ? UVOverlay->AppendElement(FVector2f(0.0f, 0.0f)) : INDEX_NONE;
	const int32 UV1 = UVOverlay ? UVOverlay->AppendElement(FVector2f(1.0f, 0.0f)) : INDEX_NONE;
	const int32 UV2 = UVOverlay ? UVOverlay->AppendElement(FVector2f(1.0f, 1.0f)) : INDEX_NONE;
	const int32 UV3 = UVOverlay ? UVOverlay->AppendElement(FVector2f(0.0f, 1.0f)) : INDEX_NONE;
	const int32 FeatureMaskUV0 = FeatureMaskUVOverlay ? FeatureMaskUVOverlay->AppendElement(FVector2f(0.0f, 0.0f)) : INDEX_NONE;
	const int32 FeatureMaskUV1 = FeatureMaskUVOverlay ? FeatureMaskUVOverlay->AppendElement(FVector2f(0.0f, 0.0f)) : INDEX_NONE;
	const int32 FeatureMaskUV2 = FeatureMaskUVOverlay ? FeatureMaskUVOverlay->AppendElement(FVector2f(0.0f, 0.0f)) : INDEX_NONE;
	const int32 FeatureMaskUV3 = FeatureMaskUVOverlay ? FeatureMaskUVOverlay->AppendElement(FVector2f(0.0f, 0.0f)) : INDEX_NONE;
	RenderData.FeatureMaskRef.MeshComponentIndex = MeshComponentIndex;
	RenderData.FeatureMaskRef.FeatureMaskUVElementIds[0] = FeatureMaskUV0;
	RenderData.FeatureMaskRef.FeatureMaskUVElementIds[1] = FeatureMaskUV1;
	RenderData.FeatureMaskRef.FeatureMaskUVElementIds[2] = FeatureMaskUV2;
	RenderData.FeatureMaskRef.FeatureMaskUVElementIds[3] = FeatureMaskUV3;
	RenderData.FeatureMaskRef.InputEdgeToFeatureMaskEdgeIndex[0] = InputEdgeToFeatureMaskEdgeIndex[0];
	RenderData.FeatureMaskRef.InputEdgeToFeatureMaskEdgeIndex[1] = InputEdgeToFeatureMaskEdgeIndex[1];
	RenderData.FeatureMaskRef.InputEdgeToFeatureMaskEdgeIndex[2] = InputEdgeToFeatureMaskEdgeIndex[2];
	RenderData.FeatureMaskRef.InputEdgeToFeatureMaskEdgeIndex[3] = InputEdgeToFeatureMaskEdgeIndex[3];

	auto TrackColorElement = [&RenderData, &SurfaceColor, MeshComponentIndex](int32 ColorElementId)
	{
		if (ColorElementId == INDEX_NONE)
		{
			return;
		}

		FSRCelestialBodyDynamicMeshColorElement ColorElement;
		ColorElement.MeshComponentIndex = MeshComponentIndex;
		ColorElement.ElementId = ColorElementId;
		ColorElement.BaseColor = SurfaceColor;
		RenderData.ColorElements.Add(ColorElement);
	};
	TrackColorElement(Color0);
	TrackColorElement(Color1);
	TrackColorElement(Color2);
	TrackColorElement(Color3);

	auto AppendUnweldedTriangleWithAttributes = [
		&TargetDynamicMesh,
		NormalOverlay,
		UVOverlay,
		FeatureMaskUVOverlay,
		ColorOverlay,
		MaterialIdAttribute,
		&RenderData,
		&TrackColorElement,
		&SurfaceColor,
		MaterialId,
		&QuadNormal](
			const FVector& Position0,
			const FVector& Position1,
			const FVector& Position2,
			const FVector2f& UVPosition0,
			const FVector2f& UVPosition1,
			const FVector2f& UVPosition2)
	{
		const int32 FallbackVertex0 = TargetDynamicMesh.AppendVertex(FVector3d(Position0));
		const int32 FallbackVertex1 = TargetDynamicMesh.AppendVertex(FVector3d(Position1));
		const int32 FallbackVertex2 = TargetDynamicMesh.AppendVertex(FVector3d(Position2));
		const int32 FallbackTriangle = TargetDynamicMesh.AppendTriangle(FallbackVertex0, FallbackVertex1, FallbackVertex2);
		if (FallbackTriangle < 0)
		{
			return false;
		}

		const int32 FallbackNormal0 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
		const int32 FallbackNormal1 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
		const int32 FallbackNormal2 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
		const int32 FallbackColor0 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
		const int32 FallbackColor1 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
		const int32 FallbackColor2 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
		const int32 FallbackUV0 = UVOverlay ? UVOverlay->AppendElement(UVPosition0) : INDEX_NONE;
		const int32 FallbackUV1 = UVOverlay ? UVOverlay->AppendElement(UVPosition1) : INDEX_NONE;
		const int32 FallbackUV2 = UVOverlay ? UVOverlay->AppendElement(UVPosition2) : INDEX_NONE;
		const int32 FallbackFeatureMaskUV0 = FeatureMaskUVOverlay ? FeatureMaskUVOverlay->AppendElement(FVector2f(0.0f, 0.0f)) : INDEX_NONE;
		const int32 FallbackFeatureMaskUV1 = FeatureMaskUVOverlay ? FeatureMaskUVOverlay->AppendElement(FVector2f(0.0f, 0.0f)) : INDEX_NONE;
		const int32 FallbackFeatureMaskUV2 = FeatureMaskUVOverlay ? FeatureMaskUVOverlay->AppendElement(FVector2f(0.0f, 0.0f)) : INDEX_NONE;
		TrackColorElement(FallbackColor0);
		TrackColorElement(FallbackColor1);
		TrackColorElement(FallbackColor2);

		NormalOverlay->SetTriangle(FallbackTriangle, UE::Geometry::FIndex3i(FallbackNormal0, FallbackNormal1, FallbackNormal2));
		if (UVOverlay)
		{
			UVOverlay->SetTriangle(FallbackTriangle, UE::Geometry::FIndex3i(FallbackUV0, FallbackUV1, FallbackUV2));
		}
		if (FeatureMaskUVOverlay)
		{
			FeatureMaskUVOverlay->SetTriangle(
				FallbackTriangle,
				UE::Geometry::FIndex3i(FallbackFeatureMaskUV0, FallbackFeatureMaskUV1, FallbackFeatureMaskUV2));
		}
		ColorOverlay->SetTriangle(FallbackTriangle, UE::Geometry::FIndex3i(FallbackColor0, FallbackColor1, FallbackColor2));
		if (MaterialIdAttribute)
		{
			MaterialIdAttribute->SetValue(FallbackTriangle, MaterialId);
		}
		++RenderData.FallbackTriangleCount;
		return true;
	};

	const int32 Triangle0 = TargetDynamicMesh.AppendTriangle(Vertex0, Vertex2, Vertex1);
	const int32 Triangle1 = TargetDynamicMesh.AppendTriangle(Vertex0, Vertex3, Vertex2);
	if (Triangle0 >= 0)
	{
		NormalOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Normal0, Normal2, Normal1));
		if (UVOverlay)
		{
			UVOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(UV0, UV2, UV1));
		}
		if (FeatureMaskUVOverlay)
		{
			FeatureMaskUVOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(FeatureMaskUV0, FeatureMaskUV2, FeatureMaskUV1));
		}
		ColorOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Color0, Color2, Color1));
		if (MaterialIdAttribute)
		{
			MaterialIdAttribute->SetValue(Triangle0, MaterialId);
		}
	}
	else
	{
		++RenderData.FailedTriangleCount;
		if (bAllowUnweldedFallbackForFailedTriangles)
		{
			AppendUnweldedTriangleWithAttributes(
				QuadPoints[0],
				QuadPoints[2],
				QuadPoints[1],
				FVector2f(0.0f, 0.0f),
				FVector2f(1.0f, 1.0f),
				FVector2f(1.0f, 0.0f));
		}
	}
	if (Triangle1 >= 0)
	{
		NormalOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Normal0, Normal3, Normal2));
		if (UVOverlay)
		{
			UVOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(UV0, UV3, UV2));
		}
		if (FeatureMaskUVOverlay)
		{
			FeatureMaskUVOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(FeatureMaskUV0, FeatureMaskUV3, FeatureMaskUV2));
		}
		ColorOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Color0, Color3, Color2));
		if (MaterialIdAttribute)
		{
			MaterialIdAttribute->SetValue(Triangle1, MaterialId);
		}
	}
	else
	{
		++RenderData.FailedTriangleCount;
		if (bAllowUnweldedFallbackForFailedTriangles)
		{
			AppendUnweldedTriangleWithAttributes(
				QuadPoints[0],
				QuadPoints[3],
				QuadPoints[2],
				FVector2f(0.0f, 0.0f),
				FVector2f(0.0f, 1.0f),
				FVector2f(1.0f, 1.0f));
		}
	}

	if (bDoubleSided)
	{
		const FVector BackNormal = -QuadNormal;
		const int32 BackNormal0 = NormalOverlay->AppendElement(FVector3f(BackNormal));
		const int32 BackNormal1 = NormalOverlay->AppendElement(FVector3f(BackNormal));
		const int32 BackNormal2 = NormalOverlay->AppendElement(FVector3f(BackNormal));
		const int32 BackNormal3 = NormalOverlay->AppendElement(FVector3f(BackNormal));
		const int32 BackColor0 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
		const int32 BackColor1 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
		const int32 BackColor2 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
		const int32 BackColor3 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
		TrackColorElement(BackColor0);
		TrackColorElement(BackColor1);
		TrackColorElement(BackColor2);
		TrackColorElement(BackColor3);

		const int32 BackTriangle0 = TargetDynamicMesh.AppendTriangle(Vertex0, Vertex1, Vertex2);
		const int32 BackTriangle1 = TargetDynamicMesh.AppendTriangle(Vertex0, Vertex2, Vertex3);
		if (BackTriangle0 >= 0)
		{
			NormalOverlay->SetTriangle(BackTriangle0, UE::Geometry::FIndex3i(BackNormal0, BackNormal1, BackNormal2));
			if (UVOverlay)
			{
				UVOverlay->SetTriangle(BackTriangle0, UE::Geometry::FIndex3i(UV0, UV1, UV2));
			}
			if (FeatureMaskUVOverlay)
			{
				FeatureMaskUVOverlay->SetTriangle(BackTriangle0, UE::Geometry::FIndex3i(FeatureMaskUV0, FeatureMaskUV1, FeatureMaskUV2));
			}
			ColorOverlay->SetTriangle(BackTriangle0, UE::Geometry::FIndex3i(BackColor0, BackColor1, BackColor2));
			if (MaterialIdAttribute)
			{
				MaterialIdAttribute->SetValue(BackTriangle0, MaterialId);
			}
		}
		if (BackTriangle1 >= 0)
		{
			NormalOverlay->SetTriangle(BackTriangle1, UE::Geometry::FIndex3i(BackNormal0, BackNormal2, BackNormal3));
			if (UVOverlay)
			{
				UVOverlay->SetTriangle(BackTriangle1, UE::Geometry::FIndex3i(UV0, UV2, UV3));
			}
			if (FeatureMaskUVOverlay)
			{
				FeatureMaskUVOverlay->SetTriangle(BackTriangle1, UE::Geometry::FIndex3i(FeatureMaskUV0, FeatureMaskUV2, FeatureMaskUV3));
			}
			ColorOverlay->SetTriangle(BackTriangle1, UE::Geometry::FIndex3i(BackColor0, BackColor2, BackColor3));
			if (MaterialIdAttribute)
			{
				MaterialIdAttribute->SetValue(BackTriangle1, MaterialId);
			}
		}
	}

	return RenderData;
}
}
