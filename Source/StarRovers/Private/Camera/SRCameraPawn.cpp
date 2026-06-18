#include "Camera/SRCameraPawn.h"

#include "SRCameraPawnInternal.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

namespace StarRoversInputPaths
{
	static constexpr TCHAR DefaultMappingContext[] = TEXT("/Game/BlueprintClasses/Core/IMC_SR.IMC_SR");
	static constexpr TCHAR DragHoldAction[] = TEXT("/Game/BlueprintClasses/Core/IA_LeftClick.IA_LeftClick");
	static constexpr TCHAR FocusSurfaceDragHoldAction[] = TEXT("/Game/BlueprintClasses/Core/IA_DragHold.IA_DragHold");
	static constexpr TCHAR DragDeltaAction[] = TEXT("/Game/BlueprintClasses/Core/IA_DragDelta.IA_DragDelta");
	static constexpr TCHAR ZoomAction[] = TEXT("/Game/BlueprintClasses/Core/IA_Zoom.IA_Zoom");
	static constexpr TCHAR FocusSurfaceAction[] = TEXT("/Game/BlueprintClasses/Core/IA_FocusSurface.IA_FocusSurface");
	static constexpr TCHAR ResetFocusAction[] = TEXT("/Game/BlueprintClasses/Core/IA_ResetFocus.IA_ResetFocus");
	static constexpr TCHAR AlignFocusSurfaceGridAction[] = TEXT("/Game/BlueprintClasses/Core/IA_AlignFocusSurfaceGrid.IA_AlignFocusSurfaceGrid");
}

namespace
{
	constexpr float DefaultCameraFieldOfView = 30.0f;
	constexpr float DefaultDragInterpSpeed = 10.0f;
	constexpr float DefaultZoomInterpSpeed = 8.0f;
}

ASRCameraPawn::ASRCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

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
	FocusFollowSmoothTime = 0.35f;
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
	FocusArcTransitionStartLocation = FVector::ZeroVector;
	FocusArcTransitionElapsed = 0.0f;
	FocusArcTransitionStartZoomDistance = 0.0f;
	FocusArcTransitionFinalZoomDistance = 0.0f;
	FocusArcTransitionPeakZoomDistance = 0.0f;
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
	FocusSurfaceInput = FVector2D::ZeroVector;
	FocusSurfaceAcceleratedInput = FVector2D::ZeroVector;
	FocusSurfaceAngularVelocity = FVector2D::ZeroVector;
	FocusSurfaceRotation = FQuat::Identity;
	FocusSurfaceTargetRotation = FQuat::Identity;
	FocusSurfaceRotationSmoothVelocity = FVector::ZeroVector;
	LastDynamicMeshVisibilityCameraLocation = FVector::ZeroVector;
	LastDynamicMeshVisibilityCameraRotation = FRotator::ZeroRotator;
	LastDynamicMeshVisibilityFocusedActor = nullptr;
	LastDynamicMeshVisibilityZoomDistance = 0.0f;
	LastDynamicMeshVisibilityUpdateTime = -BIG_NUMBER;
	bHasDynamicMeshVisibilityState = false;
	bIsResettingFocusSurfaceRotation = false;
	bIsFocusArcTransitionActive = false;

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultMappingContextFinder(StarRoversInputPaths::DefaultMappingContext);
	if (DefaultMappingContextFinder.Succeeded())
	{
		DefaultMappingContext = DefaultMappingContextFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires DefaultMappingContext at '%s'."), StarRoversInputPaths::DefaultMappingContext);
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> DragHoldFinder(StarRoversInputPaths::DragHoldAction);
	if (DragHoldFinder.Succeeded())
	{
		DragHoldAction = DragHoldFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires DragHoldAction at '%s'."), StarRoversInputPaths::DragHoldAction);
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> FocusSurfaceDragHoldFinder(StarRoversInputPaths::FocusSurfaceDragHoldAction);
	if (FocusSurfaceDragHoldFinder.Succeeded())
	{
		FocusSurfaceDragHoldAction = FocusSurfaceDragHoldFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRCameraPawn requires FocusSurfaceDragHoldAction at '%s' for left-click focus surface drag."), StarRoversInputPaths::FocusSurfaceDragHoldAction);
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> DragDeltaFinder(StarRoversInputPaths::DragDeltaAction);
	if (DragDeltaFinder.Succeeded())
	{
		DragDeltaAction = DragDeltaFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires DragDeltaAction at '%s'."), StarRoversInputPaths::DragDeltaAction);
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> ZoomFinder(StarRoversInputPaths::ZoomAction);
	if (ZoomFinder.Succeeded())
	{
		ZoomAction = ZoomFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires ZoomAction at '%s'."), StarRoversInputPaths::ZoomAction);
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> FocusSurfaceFinder(StarRoversInputPaths::FocusSurfaceAction);
	if (FocusSurfaceFinder.Succeeded())
	{
		FocusSurfaceAction = FocusSurfaceFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRCameraPawn requires FocusSurfaceAction at '%s' for focus surface camera input."), StarRoversInputPaths::FocusSurfaceAction);
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> ResetFocusFinder(StarRoversInputPaths::ResetFocusAction);
	if (ResetFocusFinder.Succeeded())
	{
		ResetFocusAction = ResetFocusFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRCameraPawn requires ResetFocusAction at '%s' for focus reset."), StarRoversInputPaths::ResetFocusAction);
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> AlignFocusSurfaceGridFinder(StarRoversInputPaths::AlignFocusSurfaceGridAction);
	if (AlignFocusSurfaceGridFinder.Succeeded())
	{
		AlignFocusSurfaceGridAction = AlignFocusSurfaceGridFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRCameraPawn requires AlignFocusSurfaceGridAction at '%s' for manual surface grid alignment."), StarRoversInputPaths::AlignFocusSurfaceGridAction);
	}

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

void ASRCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ApplyMappingContext();
	UpdateFocusSurface(DeltaSeconds);
	UpdateFocusSurfaceRotation(DeltaSeconds);

	if (FocusedActor)
	{
		if (IsValid(FocusedActor))
		{
			if (HasExitedFocusedActorGravityField())
			{
				ClearFocusActor();
			}

			if (FocusedActor)
			{
				DragTargetLocation = GetFocusLocation() + FocusDragOffset;
			}
		}
		else
		{
			ClearFocusActor();
		}
	}

	const FVector ClampedDragTargetLocation = ClampPivotLocationInsideSpace(DragTargetLocation);
	if (!ClampedDragTargetLocation.Equals(DragTargetLocation, KINDA_SMALL_NUMBER))
	{
		DragTargetLocation = ClampedDragTargetLocation;
		if (FocusedActor)
		{
			FocusDragOffset = DragTargetLocation - GetFocusLocation();
		}
	}

	const FVector DesiredLocation = DragTargetLocation;
	FVector NewLocation = DesiredLocation;
	bool bUpdatedFocusArcTransition = false;
	if (bIsDragging)
	{
		StopFocusArcTransition();
		FocusTrackingDelta = FVector::ZeroVector;
		FocusTrackingDeltaVelocity = FVector::ZeroVector;
		NewLocation = DesiredLocation;
	}
	else if (UpdateFocusArcTransition(DeltaSeconds, NewLocation))
	{
		bUpdatedFocusArcTransition = true;
		FocusTrackingDelta = FVector::ZeroVector;
		FocusTrackingDeltaVelocity = FVector::ZeroVector;
	}
	else if (FocusedActor)
	{
		FocusTrackingDelta = StarRovers::Camera::SmoothDampVector(
			FocusTrackingDelta,
			FVector::ZeroVector,
			FocusTrackingDeltaVelocity,
			FocusFollowSmoothTime,
			DeltaSeconds);

		if (FocusTrackingDelta.SizeSquared() <= KINDA_SMALL_NUMBER && FocusTrackingDeltaVelocity.SizeSquared() <= KINDA_SMALL_NUMBER)
		{
			FocusTrackingDelta = FVector::ZeroVector;
			FocusTrackingDeltaVelocity = FVector::ZeroVector;
		}

		NewLocation = DesiredLocation - FocusTrackingDelta;
	}
	else
	{
		FocusTrackingDelta = FVector::ZeroVector;
		FocusTrackingDeltaVelocity = FVector::ZeroVector;
		NewLocation = FMath::VInterpTo(GetActorLocation(), DesiredLocation, DeltaSeconds, DefaultDragInterpSpeed);
	}

	NewLocation = ClampPivotLocationInsideSpace(NewLocation);
	if (SpringArm)
	{
		ZoomDistanceTarget = ClampZoomDistance(ZoomDistanceTarget);
		SpringArm->TargetArmLength = ClampZoomDistance(SpringArm->TargetArmLength);
		ApplyZoomDrivenViewRotation(ZoomDistanceTarget);
		ZoomDistanceTarget = ClampZoomDistanceAgainstSpace(ZoomDistanceTarget, NewLocation);
		ZoomDistanceTarget = ClampZoomDistanceAgainstCelestialBodies(ZoomDistanceTarget, NewLocation);
		ZoomDistanceTarget = ClampZoomDistanceAgainstSpace(ZoomDistanceTarget, NewLocation);

		if (bUpdatedFocusArcTransition)
		{
			SpringArm->TargetArmLength = ZoomDistanceTarget;
			ApplyZoomDrivenViewRotation(SpringArm->TargetArmLength);
		}
		else
		{
			const float InterpolatedZoom = FMath::FInterpTo(SpringArm->TargetArmLength, ZoomDistanceTarget, DeltaSeconds, DefaultZoomInterpSpeed);
			float ClampedInterpolatedZoom = ClampZoomDistance(InterpolatedZoom);
			ClampedInterpolatedZoom = ClampZoomDistanceAgainstSpace(ClampedInterpolatedZoom, NewLocation);
			ClampedInterpolatedZoom = ClampZoomDistanceAgainstCelestialBodies(ClampedInterpolatedZoom, NewLocation);
			ClampedInterpolatedZoom = ClampZoomDistanceAgainstSpace(ClampedInterpolatedZoom, NewLocation);
			SpringArm->TargetArmLength = ClampedInterpolatedZoom;
			ApplyZoomDrivenViewRotation(SpringArm->TargetArmLength);
		}
	}

	SetActorLocation(NewLocation);
	UpdateDynamicMeshVisibility();
}

void ASRCameraPawn::ConfigureSpringArmCollision()
{
	if (!SpringArm)
	{
		return;
	}

	SpringArm->bDoCollisionTest = true;
	SpringArm->ProbeChannel = ECC_Camera;
	SpringArm->ProbeSize = 25.0f;
}
