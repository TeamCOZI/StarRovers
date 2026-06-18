#pragma once

#include "CoreMinimal.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Engine/StaticMesh.h"

namespace StarRovers::Camera
{
	inline FVector SmoothDampVector(
		const FVector& Current,
		const FVector& Target,
		FVector& CurrentVelocity,
		const float SmoothTime,
		const float DeltaTime)
	{
		if (DeltaTime <= UE_SMALL_NUMBER)
		{
			return Current;
		}

		const float SafeSmoothTime = FMath::Max(0.01f, SmoothTime);
		const float Omega = 2.0f / SafeSmoothTime;
		const float X = Omega * DeltaTime;
		const float ExponentialDecay = 1.0f / (1.0f + X + (0.48f * X * X) + (0.235f * X * X * X));
		const FVector DeltaFromTarget = Current - Target;
		const FVector Temp = (CurrentVelocity + (DeltaFromTarget * Omega)) * DeltaTime;

		CurrentVelocity = (CurrentVelocity - (Temp * Omega)) * ExponentialDecay;
		FVector Output = Target + ((DeltaFromTarget + Temp) * ExponentialDecay);

		if (FVector::DotProduct(Current - Target, Output - Target) <= 0.0f)
		{
			Output = Target;
			CurrentVelocity = FVector::ZeroVector;
		}

		return Output;
	}

	inline float ComputeScaledBodyRadius(const AActor* Actor)
	{
		if (const ASRCelestialBody* CelestialBody = Cast<ASRCelestialBody>(Actor))
		{
			const FSRCelestialBodyData BodyData = CelestialBody->GetData();
			if (IsValid(BodyData.StaticMesh.Get()))
			{
				return BodyData.StaticMesh->GetBounds().SphereRadius * FMath::Max(0.0f, BodyData.Scale);
			}
			return IsValid(BodyData.DynamicMeshBaseDataAsset.Get())
				? BodyData.DynamicMeshBaseDataAsset->GetSafeBaseRadius() * FMath::Max(0.0f, BodyData.Scale)
				: 0.0f;
		}

		return 0.0f;
	}
}
