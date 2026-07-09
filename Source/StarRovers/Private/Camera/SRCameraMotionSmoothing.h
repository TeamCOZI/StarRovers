#pragma once

#include "CoreMinimal.h"

namespace StarRovers::Camera
{
	inline FVector SmoothDampCameraVector(
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
}
