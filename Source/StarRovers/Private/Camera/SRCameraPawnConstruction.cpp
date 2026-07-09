#include "Camera/SRCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

namespace StarRoversInputPaths
{
	static constexpr TCHAR DefaultMappingContext[] = TEXT("/Game/StarRovers/Input/IMC_SR.IMC_SR");
	static constexpr TCHAR DragHoldAction[] = TEXT("/Game/StarRovers/Input/IA_LeftClick.IA_LeftClick");
	static constexpr TCHAR FocusSurfaceDragHoldAction[] = TEXT("/Game/StarRovers/Input/IA_DragHold.IA_DragHold");
	static constexpr TCHAR DragDeltaAction[] = TEXT("/Game/StarRovers/Input/IA_DragDelta.IA_DragDelta");
	static constexpr TCHAR ZoomAction[] = TEXT("/Game/StarRovers/Input/IA_Zoom.IA_Zoom");
	static constexpr TCHAR FocusSurfaceAction[] = TEXT("/Game/StarRovers/Input/IA_FocusSurface.IA_FocusSurface");
	static constexpr TCHAR ResetFocusAction[] = TEXT("/Game/StarRovers/Input/IA_ResetFocus.IA_ResetFocus");
}

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
}

void ASRCameraPawn::LoadDefaultInputAssets()
{
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
}
