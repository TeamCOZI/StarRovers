#pragma once

#include "CoreMinimal.h"
#include "Components/DynamicMeshComponent.h"
#include "SRCelestialRingMeshComponent.generated.h"

UCLASS(ClassGroup = (StarRovers), meta = (BlueprintSpawnableComponent))
class STARROVERS_API USRCelestialRingMeshComponent : public UDynamicMeshComponent
{
	GENERATED_BODY()

public:
	USRCelestialRingMeshComponent();

	void UpdateRingVisual(
		const FVector& WorldCenter,
		float Radius,
		const FLinearColor& Color,
		float ReferenceWorldThickness,
		int32 SegmentCount,
		bool bForceRefresh = false);
	void ClearRingVisual();

private:
	bool ShouldRebuildMesh(
		const FVector& WorldCenter,
		float Radius,
		const FLinearColor& Color,
		float ReferenceWorldThickness,
		int32 SegmentCount,
		double CurrentTime,
		bool bForceRefresh) const;
	void RebuildRingMesh(
		const FVector& WorldCenter,
		float Radius,
		const FLinearColor& Color,
		float ReferenceWorldThickness,
		int32 SegmentCount,
		double CurrentTime);

	UPROPERTY(Transient)
	bool bHasRingMesh = false;

	UPROPERTY(Transient)
	FVector LastWorldCenter = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector LastCameraLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector LastCameraForward = FVector::ForwardVector;

	UPROPERTY(Transient)
	float LastRadius = 0.0f;

	UPROPERTY(Transient)
	float LastReferenceWorldThickness = 0.0f;

	UPROPERTY(Transient)
	float LastCameraTanHalfVerticalFieldOfView = 0.0f;

	UPROPERTY(Transient)
	float LastReferenceViewDepth = 0.0f;

	UPROPERTY(Transient)
	float LastReferenceFieldOfViewDegrees = 0.0f;

	UPROPERTY(Transient)
	int32 LastSegmentCount = 0;

	UPROPERTY(Transient)
	FLinearColor LastColor = FLinearColor::Transparent;

	UPROPERTY(Transient)
	double LastMeshUpdateTime = -BIG_NUMBER;
};
