#include "Camera/SRCameraPawn.h"

#include "GameFramework/SpringArmComponent.h"

ASRCameraPawn::ASRCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	InitializeCameraComponents();
	InitializeCameraDefaults();
	ApplyZoomDrivenViewRotation(SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	RefreshScreenSpaceThicknessReferenceView();
}

void ASRCameraPawn::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ConfigureSpringArmCollision();
	ApplyZoomDrivenViewRotation(SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	RefreshScreenSpaceThicknessReferenceView();
}

void ASRCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	ConfigureSpringArmCollision();
	DragTargetLocation = ClampPivotLocationInsideSpace(GetActorLocation());
	ZoomDistanceTarget = ClampZoomDistance(SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	if (SpringArm)
	{
		SpringArm->TargetArmLength = ClampZoomDistance(SpringArm->TargetArmLength);
	}
	ApplyZoomDrivenViewRotation(SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	RefreshScreenSpaceThicknessReferenceView();
	FocusTrackingDelta = FVector::ZeroVector;
	FocusTrackingDeltaVelocity = FVector::ZeroVector;

	ApplyMappingContext();
	ApplyZoomDrivenViewRotation(SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	UpdateDynamicMeshVisibility();
}

void ASRCameraPawn::ConfigureSpringArmCollision()
{
	if (!SpringArm)
	{
		return;
	}

	// Space-scale zoom already uses explicit boundary/body avoidance. A SpringArm sweep across
	// the whole space sphere can stall badly at maximum zoom distance.
	SpringArm->bDoCollisionTest = false;
	SpringArm->ProbeChannel = ECC_Camera;
	SpringArm->ProbeSize = 25.0f;
}
