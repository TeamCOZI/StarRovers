#include "Camera/SRCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"

namespace
{
	constexpr float DefaultCameraFieldOfView = 30.0f;
}

void ASRCameraPawn::InitializeCameraComponents()
{
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	SetRootComponent(SceneRoot);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(SceneRoot);
	ConfigureSpringArmCollision();

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	Camera->PostProcessBlendWeight = 1.0f;
	Camera->PostProcessSettings.bOverride_MotionBlurAmount = true;
	Camera->PostProcessSettings.MotionBlurAmount = 0.0f;
	Camera->PostProcessSettings.bOverride_MotionBlurMax = true;
	Camera->PostProcessSettings.MotionBlurMax = 0.0f;
}

void ASRCameraPawn::InitializeCameraDefaults()
{
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	ZoomSpeed = 5000.0f;
	ZoomInputScaleMultiplier = 1.0f;
	LeftDragInputScaleMultiplier = 1.0f;
	RightDragInputScaleMultiplier = 1.0f;
	RightDragInputScaleMax = 0.0f;
	CameraSurfacePadding = 100.0f;
	if (Camera)
	{
		Camera->SetFieldOfView(DefaultCameraFieldOfView);
	}
	UseObliqueView = true;
	NearViewRotation = FRotator::ZeroRotator;
	FarViewRotation = FRotator(60.0f, 0.0f, 0.0f);
	ObliqueViewStart = 0.3f;
	ObliqueViewEnd = 1.0f;
	UseFocusedObliqueViewAltitudeRange = true;
	FocusedObliqueViewNearAltitudeMultiplier = 0.0f;
	FocusedObliqueViewFarAltitudeMultiplier = 2.5f;
	FocusedObliqueViewBaseRotation = FRotator::ZeroRotator;
	FocusedObliqueViewMaxRotation = FRotator(60.0f, 0.0f, 0.0f);
	FocusFollowSmoothTime = 0.35f;
	SmallActorFocusZoomDistance = 2500.0f;
	FocusArcTransitionDuration = 1.55f;
	FocusArcHeightMultiplier = 2.75f;
	FocusArcMinHeight = 30000.0f;
	FocusArcZoomOutDistanceMultiplier = 1.65f;
	SurfaceRotateSensitivity = 0.2f;
	FocusSurfaceSpeed = 60.0f;
	FocusSurfaceInputAcceleration = 6.0f;
	FocusSurfaceInputDeceleration = 10.0f;
	FocusSurfaceInertiaDamping = 2.5f;
	FocusSurfaceMinInertiaSpeed = 1.0f;
	DragTargetLocation = FVector::ZeroVector;
	ZoomDistanceTarget = SpringArm->TargetArmLength;
	bIsDragging = false;
	bMappingContextApplied = false;
	bIsFocusSurfaceActive = false;
	bIsDraggingFocusSurface = false;
	bHasDragStartMousePosition = false;
	FocusDragOffset = FVector::ZeroVector;
	FocusTrackingDelta = FVector::ZeroVector;
	FocusTrackingDeltaVelocity = FVector::ZeroVector;
	ScreenSpaceThicknessReferenceZoomDistance = 0.0f;
	ScreenSpaceThicknessReferenceFieldOfView = 0.0f;
	DragStartMouseScreenPosition = FVector2D::ZeroVector;
	DragStartFocusDragOffset = FVector::ZeroVector;
	DragStartTargetLocation = FVector::ZeroVector;
	bHasFocusSurfaceCenterTarget = false;
	FocusSurfaceCenterTargetActorLocalDirection = FVector::ZeroVector;
	FocusSurfaceCenterTargetRotationOffset = FQuat::Identity;
	FocusSurfaceCenterTargetRadius = 0.0f;
}
