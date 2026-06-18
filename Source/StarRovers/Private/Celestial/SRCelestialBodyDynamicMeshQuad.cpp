#include "Celestial/SRCelestialBodyDynamicMeshInternal.h"

#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"

namespace StarRovers::Celestial::DynamicMesh
{
FSRCelestialBodyDynamicMeshQuadRenderData AppendFlatColoredDynamicMeshQuad(
	TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes,
	TMap<FSRTerrainVertexKey, int32>& WeldedVertexIds,
	int32 MeshComponentIndex,
	const FVector& Point0,
	const FVector& Point1,
	const FVector& Point2,
	const FVector& Point3,
	const FLinearColor& SurfaceColor,
	int32 MaterialId,
	bool bDoubleSided,
	const FSRTerrainVertexKey* VertexKeys,
	const FVector* NormalReferenceDirectionOverride)
{
	FSRCelestialBodyDynamicMeshQuadRenderData RenderData;
	MeshComponentIndex = 0;
	if (!FaceDynamicMeshes.IsValidIndex(MeshComponentIndex))
	{
		return RenderData;
	}

	UE::Geometry::FDynamicMesh3& TargetDynamicMesh = FaceDynamicMeshes[MeshComponentIndex];
	UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = TargetDynamicMesh.Attributes()->PrimaryNormals();
	auto* ColorOverlay = TargetDynamicMesh.Attributes()->PrimaryColors();
	auto* MaterialIdAttribute = TargetDynamicMesh.Attributes()->GetMaterialID();
	if (!NormalOverlay || !ColorOverlay)
	{
		return RenderData;
	}

	FVector QuadPoints[4] = { Point0, Point1, Point2, Point3 };
	FSRTerrainVertexKey ResolvedVertexKeys[4];
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
		QuadNormal *= -1.0f;
	}
	if (QuadNormal.IsNearlyZero())
	{
		QuadNormal = OutwardDirection.IsNearlyZero() ? FVector::UpVector : OutwardDirection;
	}

	auto FindOrAppendVertex = [&TargetDynamicMesh, &WeldedVertexIds](const FVector& Position, const FSRTerrainVertexKey* VertexKey)
	{
		const FSRTerrainVertexKey ResolvedVertexKey = VertexKey ? *VertexKey : MakeTerrainVertexKey(Position);
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

	const int32 Triangle0 = TargetDynamicMesh.AppendTriangle(Vertex0, Vertex2, Vertex1);
	const int32 Triangle1 = TargetDynamicMesh.AppendTriangle(Vertex0, Vertex3, Vertex2);
	if (Triangle0 >= 0)
	{
		NormalOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Normal0, Normal2, Normal1));
		ColorOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Color0, Color2, Color1));
		if (MaterialIdAttribute)
		{
			MaterialIdAttribute->SetValue(Triangle0, MaterialId);
		}
	}
	if (Triangle1 >= 0)
	{
		NormalOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Normal0, Normal3, Normal2));
		ColorOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Color0, Color3, Color2));
		if (MaterialIdAttribute)
		{
			MaterialIdAttribute->SetValue(Triangle1, MaterialId);
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
			ColorOverlay->SetTriangle(BackTriangle0, UE::Geometry::FIndex3i(BackColor0, BackColor1, BackColor2));
			if (MaterialIdAttribute)
			{
				MaterialIdAttribute->SetValue(BackTriangle0, MaterialId);
			}
		}
		if (BackTriangle1 >= 0)
		{
			NormalOverlay->SetTriangle(BackTriangle1, UE::Geometry::FIndex3i(BackNormal0, BackNormal2, BackNormal3));
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
