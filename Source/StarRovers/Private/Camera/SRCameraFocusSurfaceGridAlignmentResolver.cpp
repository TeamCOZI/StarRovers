#include "SRCameraFocusSurfaceGridAlignmentResolver.h"

#include "Utility/SRLog.h"
#include "Camera/CameraComponent.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "GameFramework/Actor.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	constexpr float TwoPiRadians = PI * 2.0f;

	float NormalizeRadians(float AngleRadians)
	{
		float NormalizedAngle = FMath::Fmod(AngleRadians + PI, TwoPiRadians);
		if (NormalizedAngle < 0.0f)
		{
			NormalizedAngle += TwoPiRadians;
		}
		return NormalizedAngle - PI;
	}
}

bool FSRCameraFocusSurfaceGridAlignmentResolver::Resolve(
	AActor* FocusedActor,
	const UCameraComponent* Camera,
	const FVector& PawnLocation,
	const FVector& FocusLocation,
	const FQuat& ViewQuat,
	float ZoomDistance,
	FVector& OutAxis,
	float& OutAngleRadians)
{
	OutAxis = FVector::ZeroVector;
	OutAngleRadians = 0.0f;
	if (!IsValid(FocusedActor))
	{
		return false;
	}

	USRPlanetSurfaceGrid* SurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(FocusedActor);
	if (!IsValid(SurfaceGrid) || SurfaceGrid->GetCellCount() <= 0)
	{
		return false;
	}

	if (AActor* SurfaceOwner = SurfaceGrid->GetOwner())
	{
		SurfaceOwner->UpdateComponentTransforms();
	}
	SurfaceGrid->UpdateComponentToWorld();

	const FVector ViewForward = ViewQuat.RotateVector(FVector::ForwardVector).GetSafeNormal();
	const FVector ViewRight = ViewQuat.RotateVector(FVector::RightVector).GetSafeNormal();
	const FVector ViewUp = ViewQuat.RotateVector(FVector::UpVector).GetSafeNormal();
	if (ViewForward.IsNearlyZero() || ViewRight.IsNearlyZero() || ViewUp.IsNearlyZero())
	{
		return false;
	}

	const float SafeZoomDistance = FMath::Max(1.0f, ZoomDistance);
	FVector RayOrigin = Camera ? Camera->GetComponentLocation() : PawnLocation - (ViewForward * SafeZoomDistance);
	FVector RayDirection = Camera ? Camera->GetForwardVector().GetSafeNormal() : ViewForward;
	if (RayDirection.IsNearlyZero())
	{
		RayDirection = ViewForward;
	}

	FSRPlanetSurfaceGridCell HitCell;
	FVector HitLocation = FVector::ZeroVector;
	if (!SurfaceGrid->RaycastCell(RayOrigin, RayDirection, HitCell, HitLocation))
	{
		return false;
	}

	FTransform CellWorldTransform = FTransform::Identity;
	const bool bHasCellWorldTransform = SurfaceGrid->GetCellWorldTransform(HitCell.CellId, 0.0f, CellWorldTransform);
	FVector AlignmentAxis = (HitLocation - FocusLocation).GetSafeNormal();
	if (AlignmentAxis.IsNearlyZero() && bHasCellWorldTransform)
	{
		AlignmentAxis = (CellWorldTransform.GetLocation() - FocusLocation).GetSafeNormal();
	}
	if (AlignmentAxis.IsNearlyZero() && bHasCellWorldTransform)
	{
		AlignmentAxis = CellWorldTransform.GetRotation().GetUpVector().GetSafeNormal();
	}
	if (AlignmentAxis.IsNearlyZero())
	{
		return false;
	}

	FVector Corner00 = FVector::ZeroVector;
	FVector Corner10 = FVector::ZeroVector;
	FVector Corner11 = FVector::ZeroVector;
	FVector Corner01 = FVector::ZeroVector;
	if (!SurfaceGrid->GetCellWorldCorners(HitCell.CellId, Corner00, Corner10, Corner11, Corner01))
	{
		return false;
	}

	const FVector GridUWorld = (((Corner10 + Corner11) * 0.5f) - ((Corner00 + Corner01) * 0.5f)).GetSafeNormal();
	const FVector GridVWorld = (((Corner01 + Corner11) * 0.5f) - ((Corner00 + Corner10) * 0.5f)).GetSafeNormal();
	if (GridUWorld.IsNearlyZero() || GridVWorld.IsNearlyZero())
	{
		return false;
	}

	bool bFoundCandidate = false;
	float BestCandidateScore = BIG_NUMBER;
	float BestAngleDistanceRadians = BIG_NUMBER;
	float BestAngleRadians = 0.0f;
	auto EvaluateAlignmentAngle = [
		&bFoundCandidate,
		&BestCandidateScore,
		&BestAngleDistanceRadians,
		&BestAngleRadians,
		&AlignmentAxis,
		&ViewForward,
		&ViewRight,
		&ViewUp](const FVector& WorldAxis, float CandidateAngleRadians)
	{
		const float NormalizedAngleRadians = NormalizeRadians(CandidateAngleRadians);
		const FQuat CandidateDelta(AlignmentAxis, NormalizedAngleRadians);
		const FVector CandidateForward = CandidateDelta.RotateVector(ViewForward).GetSafeNormal();
		const FVector CandidateRight = CandidateDelta.RotateVector(ViewRight).GetSafeNormal();
		const FVector CandidateUp = CandidateDelta.RotateVector(ViewUp).GetSafeNormal();
		if (CandidateForward.IsNearlyZero() || CandidateRight.IsNearlyZero() || CandidateUp.IsNearlyZero())
		{
			return;
		}

		const FVector ProjectedAxis = FVector::VectorPlaneProject(WorldAxis, CandidateForward).GetSafeNormal();
		if (ProjectedAxis.IsNearlyZero())
		{
			return;
		}

		const float VerticalError = FMath::Abs(FVector::DotProduct(ProjectedAxis, CandidateRight));
		const float DirectionError = 1.0f - FMath::Abs(FVector::DotProduct(ProjectedAxis, CandidateUp));
		const float CandidateScore = (VerticalError + DirectionError) * 1000.0f + FMath::Abs(NormalizedAngleRadians);
		const float AngleDistanceRadians = FMath::Abs(NormalizedAngleRadians);
		if (!bFoundCandidate
			|| CandidateScore < BestCandidateScore - UE_SMALL_NUMBER
			|| (FMath::IsNearlyEqual(CandidateScore, BestCandidateScore, UE_SMALL_NUMBER)
				&& AngleDistanceRadians < BestAngleDistanceRadians))
		{
			bFoundCandidate = true;
			BestCandidateScore = CandidateScore;
			BestAngleDistanceRadians = AngleDistanceRadians;
			BestAngleRadians = NormalizedAngleRadians;
		}
	};

	auto ConsiderProjectedGridAxis = [
		&AlignmentAxis,
		&ViewRight,
		&EvaluateAlignmentAngle](const FVector& WorldAxis)
	{
		const FVector NormalizedWorldAxis = WorldAxis.GetSafeNormal();
		if (NormalizedWorldAxis.IsNearlyZero())
		{
			return;
		}

		const float AxisDotGrid = FVector::DotProduct(AlignmentAxis, NormalizedWorldAxis);
		const float AxisDotRight = FVector::DotProduct(AlignmentAxis, ViewRight);
		const float CosCoefficient = FVector::DotProduct(NormalizedWorldAxis, ViewRight) - (AxisDotGrid * AxisDotRight);
		const float SinCoefficient = FVector::DotProduct(NormalizedWorldAxis, FVector::CrossProduct(AlignmentAxis, ViewRight));
		const float ConstantCoefficient = AxisDotGrid * AxisDotRight;
		const float WaveAmplitude = FMath::Sqrt(
			FMath::Square(CosCoefficient)
			+ FMath::Square(SinCoefficient));
		if (WaveAmplitude <= UE_SMALL_NUMBER)
		{
			EvaluateAlignmentAngle(NormalizedWorldAxis, 0.0f);
			return;
		}

		const float WavePhase = FMath::Atan2(SinCoefficient, CosCoefficient);
		const float RawCosValue = -ConstantCoefficient / WaveAmplitude;
		if (RawCosValue < -1.0f - KINDA_SMALL_NUMBER || RawCosValue > 1.0f + KINDA_SMALL_NUMBER)
		{
			EvaluateAlignmentAngle(NormalizedWorldAxis, WavePhase);
			EvaluateAlignmentAngle(NormalizedWorldAxis, WavePhase + PI);
			return;
		}

		const float AngleOffset = FMath::Acos(FMath::Clamp(RawCosValue, -1.0f, 1.0f));
		EvaluateAlignmentAngle(NormalizedWorldAxis, WavePhase + AngleOffset);
		EvaluateAlignmentAngle(NormalizedWorldAxis, WavePhase - AngleOffset);
	};

	ConsiderProjectedGridAxis(GridUWorld);
	ConsiderProjectedGridAxis(-GridUWorld);
	ConsiderProjectedGridAxis(GridVWorld);
	ConsiderProjectedGridAxis(-GridVWorld);
	if (!bFoundCandidate)
	{
		return false;
	}

	SR_LOG(Camera, LogTemp,
		Display,
		TEXT("[Camera] Focus surface grid alignment center cell: Face=%d X=%d Y=%d Ray=%s Hit=%s Axis=%s AngleDegrees=%.3f FocusedActor=%s"),
		HitCell.CellId.Face,
		HitCell.CellId.CellX,
		HitCell.CellId.CellY,
		Camera ? TEXT("CameraComponentCenter") : TEXT("ViewForwardFallback"),
		*HitLocation.ToCompactString(),
		*AlignmentAxis.ToCompactString(),
		FMath::RadiansToDegrees(BestAngleRadians),
		*GetNameSafe(FocusedActor));

	OutAxis = AlignmentAxis;
	OutAngleRadians = BestAngleRadians;
	return true;
}
