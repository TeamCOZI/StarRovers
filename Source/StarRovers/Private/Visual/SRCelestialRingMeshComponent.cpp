#include "Visual/SRCelestialRingMeshComponent.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Visual/SRLineThicknessUtils.h"

namespace
{
	constexpr double CameraRefreshIntervalSeconds = 0.25;
	constexpr float CenterMoveRebuildDistance = 500.0f;
	constexpr float CameraMoveRebuildDistance = 500.0f;
	constexpr float CameraForwardDotThreshold = 0.9999f;
	constexpr float RadiusRebuildTolerance = 1.0f;
	constexpr float ThicknessRebuildTolerance = 0.1f;
	constexpr float CameraTanHalfFovTolerance = 0.005f;
	constexpr float ReferenceViewDepthTolerance = 1.0f;
	constexpr float ReferenceFovTolerance = 0.1f;
	constexpr int32 MinRingSegments = 12;
	constexpr int32 MaxRingSegments = 128;

	bool AreColorsNearlyEqual(const FLinearColor& A, const FLinearColor& B)
	{
		return FMath::IsNearlyEqual(A.R, B.R, 0.001f)
			&& FMath::IsNearlyEqual(A.G, B.G, 0.001f)
			&& FMath::IsNearlyEqual(A.B, B.B, 0.001f)
			&& FMath::IsNearlyEqual(A.A, B.A, 0.001f);
	}

	void AppendRingSegmentQuad(
		UE::Geometry::FDynamicMesh3& RingMesh,
		const FVector& DirectionA,
		const FVector& DirectionB,
		float Radius,
		float HalfThickness,
		const FLinearColor& Color)
	{
		const float InnerRadius = FMath::Max(0.0f, Radius - HalfThickness);
		const float OuterRadius = FMath::Max(InnerRadius + 0.01f, Radius + HalfThickness);

		const FVector LocalOuterA = DirectionA * OuterRadius;
		const FVector LocalOuterB = DirectionB * OuterRadius;
		const FVector LocalInnerB = DirectionB * InnerRadius;
		const FVector LocalInnerA = DirectionA * InnerRadius;

		const int32 Vertex0 = RingMesh.AppendVertex(FVector3d(LocalOuterA));
		const int32 Vertex1 = RingMesh.AppendVertex(FVector3d(LocalOuterB));
		const int32 Vertex2 = RingMesh.AppendVertex(FVector3d(LocalInnerB));
		const int32 Vertex3 = RingMesh.AppendVertex(FVector3d(LocalInnerA));

		const int32 FrontTriangle0 = RingMesh.AppendTriangle(Vertex0, Vertex1, Vertex2);
		const int32 FrontTriangle1 = RingMesh.AppendTriangle(Vertex0, Vertex2, Vertex3);
		const int32 BackTriangle0 = RingMesh.AppendTriangle(Vertex0, Vertex2, Vertex1);
		const int32 BackTriangle1 = RingMesh.AppendTriangle(Vertex0, Vertex3, Vertex2);

		UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = RingMesh.Attributes()->PrimaryNormals();
		auto* ColorOverlay = RingMesh.Attributes()->PrimaryColors();
		if (!NormalOverlay || !ColorOverlay)
		{
			return;
		}

		const int32 FrontNormal0 = NormalOverlay->AppendElement(FVector3f(FVector::XAxisVector));
		const int32 FrontNormal1 = NormalOverlay->AppendElement(FVector3f(FVector::XAxisVector));
		const int32 FrontNormal2 = NormalOverlay->AppendElement(FVector3f(FVector::XAxisVector));
		const int32 FrontNormal3 = NormalOverlay->AppendElement(FVector3f(FVector::XAxisVector));
		const int32 BackNormal0 = NormalOverlay->AppendElement(FVector3f(-FVector::XAxisVector));
		const int32 BackNormal1 = NormalOverlay->AppendElement(FVector3f(-FVector::XAxisVector));
		const int32 BackNormal2 = NormalOverlay->AppendElement(FVector3f(-FVector::XAxisVector));
		const int32 BackNormal3 = NormalOverlay->AppendElement(FVector3f(-FVector::XAxisVector));

		const FVector4f VertexColor(Color.R, Color.G, Color.B, Color.A);
		const int32 Color0 = ColorOverlay->AppendElement(VertexColor);
		const int32 Color1 = ColorOverlay->AppendElement(VertexColor);
		const int32 Color2 = ColorOverlay->AppendElement(VertexColor);
		const int32 Color3 = ColorOverlay->AppendElement(VertexColor);

		if (FrontTriangle0 >= 0)
		{
			NormalOverlay->SetTriangle(FrontTriangle0, UE::Geometry::FIndex3i(FrontNormal0, FrontNormal1, FrontNormal2));
			ColorOverlay->SetTriangle(FrontTriangle0, UE::Geometry::FIndex3i(Color0, Color1, Color2));
		}
		if (FrontTriangle1 >= 0)
		{
			NormalOverlay->SetTriangle(FrontTriangle1, UE::Geometry::FIndex3i(FrontNormal0, FrontNormal2, FrontNormal3));
			ColorOverlay->SetTriangle(FrontTriangle1, UE::Geometry::FIndex3i(Color0, Color2, Color3));
		}
		if (BackTriangle0 >= 0)
		{
			NormalOverlay->SetTriangle(BackTriangle0, UE::Geometry::FIndex3i(BackNormal0, BackNormal2, BackNormal1));
			ColorOverlay->SetTriangle(BackTriangle0, UE::Geometry::FIndex3i(Color0, Color2, Color1));
		}
		if (BackTriangle1 >= 0)
		{
			NormalOverlay->SetTriangle(BackTriangle1, UE::Geometry::FIndex3i(BackNormal0, BackNormal3, BackNormal2));
			ColorOverlay->SetTriangle(BackTriangle1, UE::Geometry::FIndex3i(Color0, Color3, Color2));
		}
	}
}

USRCelestialRingMeshComponent::USRCelestialRingMeshComponent()
{
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCastShadow(false);
	bReceivesDecals = false;
	SetVisibility(false);
	SetHiddenInGame(true);
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VertexColorMaterialFinder(
		TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	if (VertexColorMaterialFinder.Succeeded())
	{
		SetMaterial(0, VertexColorMaterialFinder.Object);
	}
}

void USRCelestialRingMeshComponent::UpdateRingVisual(
	const FVector& WorldCenter,
	float Radius,
	const FLinearColor& Color,
	float ReferenceWorldThickness,
	int32 SegmentCount,
	bool bForceRefresh)
{
	const float SafeRadius = FMath::Max(0.0f, Radius);
	const float SafeThickness = FMath::Max(0.0f, ReferenceWorldThickness);
	const int32 SafeSegmentCount = FMath::Clamp(SegmentCount, MinRingSegments, MaxRingSegments);
	const FLinearColor SafeColor(Color.R, Color.G, Color.B, FMath::Clamp(Color.A, 0.0f, 1.0f));
	if (SafeRadius <= KINDA_SMALL_NUMBER || SafeThickness <= KINDA_SMALL_NUMBER || SafeColor.A <= KINDA_SMALL_NUMBER)
	{
		ClearRingVisual();
		return;
	}

	SetWorldLocation(WorldCenter);
	SetWorldRotation(FRotator::ZeroRotator);
	SetWorldScale3D(FVector::OneVector);

	const UWorld* World = GetWorld();
	const double CurrentTime = World ? World->GetTimeSeconds() : 0.0;
	if (ShouldRebuildMesh(
			WorldCenter,
			SafeRadius,
			SafeColor,
			SafeThickness,
			SafeSegmentCount,
			CurrentTime,
			bForceRefresh))
	{
		RebuildRingMesh(WorldCenter, SafeRadius, SafeColor, SafeThickness, SafeSegmentCount, CurrentTime);
	}

	if (bHasRingMesh)
	{
		SetVisibility(true);
		SetHiddenInGame(false);
	}
}

void USRCelestialRingMeshComponent::ClearRingVisual()
{
	SetVisibility(false);
	SetHiddenInGame(true);
	if (!bHasRingMesh)
	{
		return;
	}

	UE::Geometry::FDynamicMesh3 EmptyMesh;
	EmptyMesh.EnableAttributes();
	EmptyMesh.Attributes()->EnablePrimaryColors();
	SetMesh(MoveTemp(EmptyMesh));

	bHasRingMesh = false;
	LastRadius = 0.0f;
	LastReferenceWorldThickness = 0.0f;
	LastSegmentCount = 0;
	LastColor = FLinearColor::Transparent;
	LastMeshUpdateTime = -BIG_NUMBER;
}

bool USRCelestialRingMeshComponent::ShouldRebuildMesh(
	const FVector& WorldCenter,
	float Radius,
	const FLinearColor& Color,
	float ReferenceWorldThickness,
	int32 SegmentCount,
	double CurrentTime,
	bool bForceRefresh) const
{
	if (bForceRefresh || !bHasRingMesh)
	{
		return true;
	}

	if (FMath::Abs(Radius - LastRadius) > RadiusRebuildTolerance
		|| FMath::Abs(ReferenceWorldThickness - LastReferenceWorldThickness) > ThicknessRebuildTolerance
		|| SegmentCount != LastSegmentCount
		|| !AreColorsNearlyEqual(Color, LastColor))
	{
		return true;
	}

	const bool bRefreshIntervalElapsed = (CurrentTime - LastMeshUpdateTime) >= CameraRefreshIntervalSeconds;
	if (!bRefreshIntervalElapsed)
	{
		return false;
	}

	FSRCameraInfo CameraInfo;
	FSRLineThicknessUtils::TryBuildPrimaryCameraInfo(GetWorld(), CameraInfo);
	if (!CameraInfo.bIsValid)
	{
		return false;
	}

	float ReferenceViewDepth = FSRLineThicknessUtils::DefaultReferenceViewDepth;
	float ReferenceFieldOfViewDegrees = FSRLineThicknessUtils::DefaultReferenceFieldOfViewDegrees;
	FSRLineThicknessUtils::ResolveReferenceView(GetWorld(), ReferenceViewDepth, ReferenceFieldOfViewDegrees);

	const bool bCenterMovedEnough = FVector::DistSquared(WorldCenter, LastWorldCenter) >= FMath::Square(CenterMoveRebuildDistance);
	const bool bCameraMovedEnough = FVector::DistSquared(CameraInfo.ViewLocation, LastCameraLocation) >= FMath::Square(CameraMoveRebuildDistance);
	const bool bCameraRotatedEnough = FVector::DotProduct(CameraInfo.ViewForward, LastCameraForward) <= CameraForwardDotThreshold;
	const bool bCameraFovChangedEnough = FMath::Abs(CameraInfo.TanHalfVerticalFieldOfView - LastCameraTanHalfVerticalFieldOfView) >= CameraTanHalfFovTolerance;
	const bool bReferenceDepthChangedEnough = FMath::Abs(ReferenceViewDepth - LastReferenceViewDepth) >= ReferenceViewDepthTolerance;
	const bool bReferenceFovChangedEnough = FMath::Abs(ReferenceFieldOfViewDegrees - LastReferenceFieldOfViewDegrees) >= ReferenceFovTolerance;

	return bCenterMovedEnough
		|| bCameraMovedEnough
		|| bCameraRotatedEnough
		|| bCameraFovChangedEnough
		|| bReferenceDepthChangedEnough
		|| bReferenceFovChangedEnough;
}

void USRCelestialRingMeshComponent::RebuildRingMesh(
	const FVector& WorldCenter,
	float Radius,
	const FLinearColor& Color,
	float ReferenceWorldThickness,
	int32 SegmentCount,
	double CurrentTime)
{
	FSRCameraInfo CameraInfo;
	FSRLineThicknessUtils::TryBuildPrimaryCameraInfo(GetWorld(), CameraInfo);

	float ReferenceViewDepth = FSRLineThicknessUtils::DefaultReferenceViewDepth;
	float ReferenceFieldOfViewDegrees = FSRLineThicknessUtils::DefaultReferenceFieldOfViewDegrees;
	FSRLineThicknessUtils::ResolveReferenceView(GetWorld(), ReferenceViewDepth, ReferenceFieldOfViewDegrees);

	UE::Geometry::FDynamicMesh3 RingMesh;
	RingMesh.EnableAttributes();
	RingMesh.Attributes()->EnablePrimaryColors();

	const float SafeRadius = FMath::Max(0.0f, Radius);
	const int32 SafeSegmentCount = FMath::Clamp(SegmentCount, MinRingSegments, MaxRingSegments);
	for (int32 SegmentIndex = 0; SegmentIndex < SafeSegmentCount; ++SegmentIndex)
	{
		const float AlphaA = static_cast<float>(SegmentIndex) / static_cast<float>(SafeSegmentCount);
		const float AlphaB = static_cast<float>(SegmentIndex + 1) / static_cast<float>(SafeSegmentCount);
		const float AngleA = AlphaA * UE_TWO_PI;
		const float AngleB = AlphaB * UE_TWO_PI;
		const FVector DirectionA(0.0f, FMath::Cos(AngleA), FMath::Sin(AngleA));
		const FVector DirectionB(0.0f, FMath::Cos(AngleB), FMath::Sin(AngleB));
		const FVector WorldMidpoint = WorldCenter + ((DirectionA + DirectionB).GetSafeNormal() * SafeRadius);
		const float SegmentWorldThickness = FSRLineThicknessUtils::ComputeWorldThicknessAtLocation(
			CameraInfo,
			WorldMidpoint,
			ReferenceWorldThickness,
			ReferenceViewDepth,
			ReferenceFieldOfViewDegrees);
		const float HalfThickness = FMath::Max(0.01f, SegmentWorldThickness) * 0.5f;
		AppendRingSegmentQuad(RingMesh, DirectionA, DirectionB, SafeRadius, HalfThickness, Color);
	}

	SetMesh(MoveTemp(RingMesh));

	LastWorldCenter = WorldCenter;
	LastRadius = Radius;
	LastColor = Color;
	LastReferenceWorldThickness = ReferenceWorldThickness;
	LastSegmentCount = SafeSegmentCount;
	LastMeshUpdateTime = CurrentTime;
	if (CameraInfo.bIsValid)
	{
		LastCameraLocation = CameraInfo.ViewLocation;
		LastCameraForward = CameraInfo.ViewForward;
		LastCameraTanHalfVerticalFieldOfView = CameraInfo.TanHalfVerticalFieldOfView;
	}
	LastReferenceViewDepth = ReferenceViewDepth;
	LastReferenceFieldOfViewDegrees = ReferenceFieldOfViewDegrees;
	bHasRingMesh = true;
}
