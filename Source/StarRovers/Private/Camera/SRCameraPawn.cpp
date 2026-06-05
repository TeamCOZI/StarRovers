#include "Camera/SRCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Camera/SRPlayerController.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Gravity/SRGravityParent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Surface/SRPlanetSurfaceGrid.h"
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
	constexpr float DefaultFocusZoomMultiplier = 3.0f;

	bool IsSpaceBoundaryActor(const AActor* Candidate)
	{
		if (!IsValid(Candidate))
		{
			return false;
		}

		const FString CandidateName = Candidate->GetName();
		const FString CandidateClassName = Candidate->GetClass() ? Candidate->GetClass()->GetName() : FString();
		return CandidateName.Contains(TEXT("SpaceSphere"), ESearchCase::IgnoreCase)
			|| CandidateName.Contains(TEXT("SpaceSkySphere"), ESearchCase::IgnoreCase)
			|| CandidateName.Contains(TEXT("Space Sky Sphere"), ESearchCase::IgnoreCase)
			|| CandidateName.Equals(TEXT("BP_Space"), ESearchCase::IgnoreCase)
			|| CandidateName.StartsWith(TEXT("BP_Space_"), ESearchCase::IgnoreCase)
			|| CandidateClassName.Contains(TEXT("SpaceSphere"), ESearchCase::IgnoreCase)
			|| CandidateClassName.Contains(TEXT("SpaceSkySphere"), ESearchCase::IgnoreCase)
			|| CandidateClassName.Equals(TEXT("BP_Space_C"), ESearchCase::IgnoreCase);
	}

	float ComputeScaledBodyRadius(const AActor* Actor)
	{
		if (const ASRCelestialBody* CelestialBody = Cast<ASRCelestialBody>(Actor))
		{
			const FSRCelestialBodyData BodyData = CelestialBody->GetData();
			return IsValid(BodyData.StaticMesh.Get())
				? BodyData.StaticMesh->GetBounds().SphereRadius * FMath::Max(0.0f, BodyData.Scale)
				: 0.0f;
		}

		return 0.0f;
	}

	FVector SmoothDampVector(
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

	FQuat SmoothDampQuat(
		const FQuat& Current,
		const FQuat& Target,
		FVector& CurrentAngularVelocity,
		const float SmoothTime,
		const float DeltaTime)
	{
		if (DeltaTime <= UE_SMALL_NUMBER)
		{
			return Current.GetNormalized();
		}

		const FQuat NormalizedTarget = Target.GetNormalized();
		const FQuat NormalizedCurrent = Current.GetNormalized();
		FQuat RemainingRotation = (NormalizedTarget * NormalizedCurrent.Inverse()).GetNormalized();
		if (RemainingRotation.W < 0.0f)
		{
			RemainingRotation.X *= -1.0f;
			RemainingRotation.Y *= -1.0f;
			RemainingRotation.Z *= -1.0f;
			RemainingRotation.W *= -1.0f;
		}

		const FRotator RemainingRotator = RemainingRotation.Rotator().GetNormalized();
		const FVector RemainingDeltaDegrees(RemainingRotator.Pitch, RemainingRotator.Yaw, RemainingRotator.Roll);
		const FVector NewRemainingDeltaDegrees = SmoothDampVector(
			RemainingDeltaDegrees,
			FVector::ZeroVector,
			CurrentAngularVelocity,
			SmoothTime,
			DeltaTime);

		if (NewRemainingDeltaDegrees.SizeSquared() <= KINDA_SMALL_NUMBER
			&& CurrentAngularVelocity.SizeSquared() <= KINDA_SMALL_NUMBER)
		{
			CurrentAngularVelocity = FVector::ZeroVector;
			return NormalizedTarget;
		}

		const FQuat NewRemainingRotation = FRotator(
			NewRemainingDeltaDegrees.X,
			NewRemainingDeltaDegrees.Y,
			NewRemainingDeltaDegrees.Z).Quaternion().GetNormalized();
		return (NewRemainingRotation.Inverse() * NormalizedTarget).GetNormalized();
	}

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
		FocusTrackingDelta = SmoothDampVector(
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

void ASRCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (DragHoldAction)
		{
			EnhancedInputComponent->BindAction(DragHoldAction, ETriggerEvent::Started, this, &ASRCameraPawn::HandleDragHoldStarted);
			EnhancedInputComponent->BindAction(DragHoldAction, ETriggerEvent::Completed, this, &ASRCameraPawn::HandleDragHoldCompleted);
			EnhancedInputComponent->BindAction(DragHoldAction, ETriggerEvent::Canceled, this, &ASRCameraPawn::HandleDragHoldCompleted);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires DragHoldAction before input binding."));
		}

		if (FocusSurfaceDragHoldAction)
		{
			EnhancedInputComponent->BindAction(FocusSurfaceDragHoldAction, ETriggerEvent::Started, this, &ASRCameraPawn::HandleFocusSurfaceDragHoldStarted);
			EnhancedInputComponent->BindAction(FocusSurfaceDragHoldAction, ETriggerEvent::Completed, this, &ASRCameraPawn::HandleFocusSurfaceDragHoldCompleted);
			EnhancedInputComponent->BindAction(FocusSurfaceDragHoldAction, ETriggerEvent::Canceled, this, &ASRCameraPawn::HandleFocusSurfaceDragHoldCompleted);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRCameraPawn requires FocusSurfaceDragHoldAction before left-click focus surface drag binding."));
		}

		if (DragDeltaAction)
		{
			EnhancedInputComponent->BindAction(DragDeltaAction, ETriggerEvent::Triggered, this, &ASRCameraPawn::HandleDragDelta);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires DragDeltaAction before input binding."));
		}

		if (ZoomAction)
		{
			EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ASRCameraPawn::HandleZoom);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires ZoomAction before input binding."));
		}

		if (FocusSurfaceAction)
		{
			EnhancedInputComponent->BindAction(FocusSurfaceAction, ETriggerEvent::Triggered, this, &ASRCameraPawn::HandleFocusSurface);
			EnhancedInputComponent->BindAction(FocusSurfaceAction, ETriggerEvent::Completed, this, &ASRCameraPawn::HandleFocusSurfaceCompleted);
			EnhancedInputComponent->BindAction(FocusSurfaceAction, ETriggerEvent::Canceled, this, &ASRCameraPawn::HandleFocusSurfaceCompleted);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRCameraPawn requires FocusSurfaceAction before focus surface input binding."));
		}

		if (ResetFocusAction)
		{
			EnhancedInputComponent->BindAction(ResetFocusAction, ETriggerEvent::Started, this, &ASRCameraPawn::HandleResetFocus);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRCameraPawn requires ResetFocusAction before focus reset input binding."));
		}

		if (AlignFocusSurfaceGridAction)
		{
			EnhancedInputComponent->BindAction(AlignFocusSurfaceGridAction, ETriggerEvent::Started, this, &ASRCameraPawn::HandleAlignFocusSurfaceGrid);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRCameraPawn requires AlignFocusSurfaceGridAction before manual focus surface grid alignment binding."));
		}
	}
}

void ASRCameraPawn::FocusActor(AActor* NewFocusActor)
{
	FocusActorWithTransition(NewFocusActor, true);
}

void ASRCameraPawn::FocusActorWithTransition(AActor* NewFocusActor, bool bUseArcTransition)
{
	AActor* PreviousFocusedActor = FocusedActor.Get();
	FocusedActor = NewFocusActor;
	FocusDragOffset = FVector::ZeroVector;
	FocusSurfaceRotation = FQuat::Identity;
	FocusSurfaceTargetRotation = FQuat::Identity;
	FocusSurfaceRotationSmoothVelocity = FVector::ZeroVector;
	bIsResettingFocusSurfaceRotation = false;
	StopFocusArcTransition();
	ClearFocusSurfaceMotion();

	if (FocusedActor)
	{
		DragTargetLocation = GetFocusLocation();
		const FVector CurrentLocation = GetActorLocation();
		const FVector DesiredLocation = DragTargetLocation;
		FocusTrackingDelta = DesiredLocation - CurrentLocation;
		FocusTrackingDeltaVelocity = FVector::ZeroVector;

		if (!Camera)
		{
			UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires Camera before focusing an actor."));
			BroadcastFocusedActorChangedIfNeeded(PreviousFocusedActor);
			return;
		}

		const float CurrentCameraFieldOfView = Camera->FieldOfView;
		const float DesiredFocusZoom = USRCelestialBodyRuntimeLibrary::GetCelestialFocusZoomDistance(
			FocusedActor,
			CurrentCameraFieldOfView,
			DefaultFocusZoomMultiplier);
		ZoomDistanceTarget = ClampZoomDistance(DesiredFocusZoom);
		if (bUseArcTransition)
		{
			BeginFocusArcTransition(ZoomDistanceTarget);
		}
	}

	BroadcastFocusedActorChangedIfNeeded(PreviousFocusedActor);
}

void ASRCameraPawn::ClearFocusActor()
{
	AActor* PreviousFocusedActor = FocusedActor.Get();
	if (FocusedActor)
	{
		DragTargetLocation = GetActorLocation();
	}

	FocusedActor = nullptr;
	FocusDragOffset = FVector::ZeroVector;
	FocusTrackingDelta = FVector::ZeroVector;
	FocusTrackingDeltaVelocity = FVector::ZeroVector;
	FocusSurfaceRotation = FQuat::Identity;
	FocusSurfaceTargetRotation = FQuat::Identity;
	FocusSurfaceRotationSmoothVelocity = FVector::ZeroVector;
	bIsResettingFocusSurfaceRotation = false;
	StopFocusArcTransition();
	ClearFocusSurfaceMotion();
	BroadcastFocusedActorChangedIfNeeded(PreviousFocusedActor);
}

AActor* ASRCameraPawn::GetFocusedActor() const
{
	return FocusedActor;
}

float ASRCameraPawn::GetMaxZoomDistance() const
{
	const float SpaceSphereRadius = GetSpaceSphereRadius();
	return SpaceSphereRadius > KINDA_SMALL_NUMBER ? SpaceSphereRadius : BIG_NUMBER;
}

float ASRCameraPawn::GetScreenSpaceThicknessReferenceZoomDistance() const
{
	if (ScreenSpaceThicknessReferenceZoomDistance > KINDA_SMALL_NUMBER)
	{
		return ScreenSpaceThicknessReferenceZoomDistance;
	}

	return FMath::Max(1.0f, SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
}

float ASRCameraPawn::GetScreenSpaceThicknessReferenceFieldOfView() const
{
	if (ScreenSpaceThicknessReferenceFieldOfView > KINDA_SMALL_NUMBER)
	{
		return ScreenSpaceThicknessReferenceFieldOfView;
	}

	if (!Camera)
	{
		UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires Camera to resolve screen-space thickness FOV."));
		return 0.0f;
	}

	return FMath::Clamp(Camera->FieldOfView, 5.0f, 170.0f);
}

void ASRCameraPawn::SnapToFocusTarget()
{
	StopFocusArcTransition();
	FocusTrackingDelta = FVector::ZeroVector;
	FocusTrackingDeltaVelocity = FVector::ZeroVector;
	if (FocusedActor)
	{
		DragTargetLocation = GetFocusLocation() + FocusDragOffset;
	}

	const FVector DesiredLocation = ClampPivotLocationInsideSpace(DragTargetLocation);
	DragTargetLocation = DesiredLocation;

	if (SpringArm)
	{
		ZoomDistanceTarget = ClampZoomDistance(ZoomDistanceTarget);
		SpringArm->TargetArmLength = ZoomDistanceTarget;
		ApplyZoomDrivenViewRotation(SpringArm->TargetArmLength);
	}
	SetActorLocation(DesiredLocation);
}

void ASRCameraPawn::ResetFocus()
{
	if (!IsValid(FocusedActor))
	{
		return;
	}

	FocusDragOffset = FVector::ZeroVector;
	StopFocusArcTransition();
	FocusSurfaceTargetRotation = FQuat::Identity;
	FocusSurfaceRotationSmoothVelocity = FVector::ZeroVector;
	bIsResettingFocusSurfaceRotation = true;
	ClearFocusSurfaceMotion();
	bIsDragging = false;
	bHasDragStartMousePosition = false;

	DragTargetLocation = GetFocusLocation();

	const FVector CurrentLocation = GetActorLocation();
	const FVector DesiredLocation = DragTargetLocation;
	FocusTrackingDelta = DesiredLocation - CurrentLocation;
	FocusTrackingDeltaVelocity = FVector::ZeroVector;

	if (!Camera)
	{
		UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires Camera before resetting focused camera view."));
		return;
	}

	const float DesiredFocusZoom = USRCelestialBodyRuntimeLibrary::GetCelestialFocusZoomDistance(
		FocusedActor,
		Camera->FieldOfView,
		DefaultFocusZoomMultiplier);
	ZoomDistanceTarget = ClampZoomDistance(DesiredFocusZoom);
}

FSRFocusedActorChangedSignature& ASRCameraPawn::OnFocusedActorChanged()
{
	return FocusedActorChangedEvent;
}

void ASRCameraPawn::ApplyMappingContext()
{
	if (bMappingContextApplied)
	{
		return;
	}
	if (!DefaultMappingContext)
	{
		UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires DefaultMappingContext before applying input mapping."));
		return;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
			bMappingContextApplied = true;
		}
	}
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

void ASRCameraPawn::BroadcastFocusedActorChangedIfNeeded(AActor* PreviousFocusedActor)
{
	if (PreviousFocusedActor != FocusedActor)
	{
		FocusedActorChangedEvent.Broadcast(FocusedActor.Get());
	}
}

void ASRCameraPawn::BeginFocusArcTransition(float FinalZoomDistance)
{
	if (!IsValid(FocusedActor) || !SpringArm)
	{
		StopFocusArcTransition();
		return;
	}

	const FVector TargetLocation = ClampPivotLocationInsideSpace(GetFocusLocation() + FocusDragOffset);
	const FVector CurrentLocation = GetActorLocation();
	const float TravelDistance = FVector::Dist(CurrentLocation, TargetLocation);
	if (TravelDistance <= KINDA_SMALL_NUMBER)
	{
		StopFocusArcTransition();
		return;
	}

	FocusArcTransitionStartLocation = CurrentLocation;
	FocusArcTransitionElapsed = 0.0f;
	FocusArcTransitionStartZoomDistance = FMath::Max(0.0f, SpringArm->TargetArmLength);
	FocusArcTransitionFinalZoomDistance = ClampZoomDistance(FinalZoomDistance);
	const float DesiredPeakZoomDistance = FMath::Max(
		FMath::Max(FocusArcTransitionStartZoomDistance, FocusArcTransitionFinalZoomDistance),
		TravelDistance * FMath::Max(0.0f, FocusArcZoomOutDistanceMultiplier));
	FocusArcTransitionPeakZoomDistance = ClampZoomDistance(DesiredPeakZoomDistance);
	bIsFocusArcTransitionActive = true;
	FocusTrackingDelta = FVector::ZeroVector;
	FocusTrackingDeltaVelocity = FVector::ZeroVector;
}

void ASRCameraPawn::StopFocusArcTransition()
{
	bIsFocusArcTransitionActive = false;
	FocusArcTransitionElapsed = 0.0f;
	FocusArcTransitionStartLocation = FVector::ZeroVector;
	FocusArcTransitionStartZoomDistance = 0.0f;
	FocusArcTransitionFinalZoomDistance = 0.0f;
	FocusArcTransitionPeakZoomDistance = 0.0f;
}

bool ASRCameraPawn::UpdateFocusArcTransition(float DeltaSeconds, FVector& OutNewLocation)
{
	if (!bIsFocusArcTransitionActive)
	{
		return false;
	}

	if (DeltaSeconds <= UE_SMALL_NUMBER || !IsValid(FocusedActor))
	{
		StopFocusArcTransition();
		return false;
	}

	FocusArcTransitionElapsed += DeltaSeconds;
	const float SafeDuration = FMath::Max(0.10f, FocusArcTransitionDuration);
	const float Alpha = FMath::Clamp(FocusArcTransitionElapsed / SafeDuration, 0.0f, 1.0f);
	const float SmoothAlpha = Alpha * Alpha * (3.0f - (2.0f * Alpha));

	const FVector TargetLocation = ClampPivotLocationInsideSpace(GetFocusLocation() + FocusDragOffset);
	DragTargetLocation = TargetLocation;
	const float TravelDistance = FVector::Dist(FocusArcTransitionStartLocation, TargetLocation);
	const float ArcHeight = TravelDistance > KINDA_SMALL_NUMBER
		? FMath::Max(FMath::Max(0.0f, FocusArcMinHeight), TravelDistance * FMath::Max(0.0f, FocusArcHeightMultiplier))
		: 0.0f;
	const float ArcAlpha = 4.0f * SmoothAlpha * (1.0f - SmoothAlpha);
	OutNewLocation = FMath::Lerp(FocusArcTransitionStartLocation, TargetLocation, SmoothAlpha)
		- (FVector::XAxisVector * ArcHeight * ArcAlpha);

	const float ZoomPhaseAlpha = SmoothAlpha < 0.5f
		? SmoothAlpha * 2.0f
		: (SmoothAlpha - 0.5f) * 2.0f;
	ZoomDistanceTarget = SmoothAlpha < 0.5f
		? FMath::Lerp(FocusArcTransitionStartZoomDistance, FocusArcTransitionPeakZoomDistance, ZoomPhaseAlpha)
		: FMath::Lerp(FocusArcTransitionPeakZoomDistance, FocusArcTransitionFinalZoomDistance, ZoomPhaseAlpha);
	ZoomDistanceTarget = ClampZoomDistance(ZoomDistanceTarget);

	if (Alpha >= 1.0f - UE_SMALL_NUMBER)
	{
		OutNewLocation = TargetLocation;
		ZoomDistanceTarget = FocusArcTransitionFinalZoomDistance;
		StopFocusArcTransition();
	}

	return true;
}

void ASRCameraPawn::HandleDragHoldStarted()
{
	if (ASRPlayerController* PlayerController = Cast<ASRPlayerController>(GetController()))
	{
		if (PlayerController->ShouldHandleAssemblyPlacementDrag())
		{
			PlayerController->BeginAssemblyPlacementDrag();
			bIsDragging = false;
			bHasDragStartMousePosition = false;
			return;
		}
	}

	StopFocusArcTransition();
	bIsDragging = true;
	bHasDragStartMousePosition = false;

	if (bIsDragging)
	{
		FocusTrackingDelta = FVector::ZeroVector;
		FocusTrackingDeltaVelocity = FVector::ZeroVector;
		DragStartFocusDragOffset = FocusDragOffset;
		DragStartTargetLocation = DragTargetLocation;
		bHasDragStartMousePosition = GetMouseScreenPosition(DragStartMouseScreenPosition);
	}
}

void ASRCameraPawn::HandleDragHoldCompleted()
{
	if (ASRPlayerController* PlayerController = Cast<ASRPlayerController>(GetController()))
	{
		PlayerController->EndAssemblyPlacementDrag();
	}

	bIsDragging = false;
	bHasDragStartMousePosition = false;
}

void ASRCameraPawn::HandleFocusSurfaceDragHoldStarted()
{
	StopFocusArcTransition();
	bIsDraggingFocusSurface = ShouldDragFocusedSurface();
	if (!bIsDraggingFocusSurface)
	{
		return;
	}

	bIsDragging = false;
	bHasDragStartMousePosition = false;
	FocusSurfaceTargetRotation = FocusSurfaceRotation.GetNormalized();
	FocusSurfaceRotationSmoothVelocity = FVector::ZeroVector;
	bIsResettingFocusSurfaceRotation = false;
	FocusSurfaceAngularVelocity = FVector2D::ZeroVector;
	bIsFocusSurfaceActive = true;
}

void ASRCameraPawn::HandleFocusSurfaceDragHoldCompleted()
{
	bIsDraggingFocusSurface = false;
}

void ASRCameraPawn::HandleDragDelta(const FInputActionValue& Value)
{
	const FVector2D DragDelta = Value.Get<FVector2D>();
	if (DragDelta.IsNearlyZero())
	{
		return;
	}

	if (bIsDraggingFocusSurface)
	{
		HandleFocusSurfaceDrag(DragDelta);
		return;
	}

	if (ASRPlayerController* PlayerController = Cast<ASRPlayerController>(GetController()))
	{
		if (PlayerController->ContinueAssemblyPlacementDrag())
		{
			bIsDragging = false;
			bHasDragStartMousePosition = false;
			return;
		}
	}

	if (!bIsDragging)
	{
		return;
	}

	FVector2D CurrentMouseScreenPosition = FVector2D::ZeroVector;
	if (GetMouseScreenPosition(CurrentMouseScreenPosition))
	{
		if (!bHasDragStartMousePosition)
		{
			DragStartFocusDragOffset = FocusDragOffset;
			DragStartTargetLocation = DragTargetLocation;
			DragStartMouseScreenPosition = CurrentMouseScreenPosition;
			bHasDragStartMousePosition = true;
			return;
		}

		const FVector DragOffsetDelta = ConvertScreenDragToDragOffset(DragStartMouseScreenPosition, CurrentMouseScreenPosition);

		if (FocusedActor)
		{
			FocusDragOffset = DragStartFocusDragOffset + DragOffsetDelta;
			DragTargetLocation = GetFocusLocation() + FocusDragOffset;
			DragTargetLocation = ClampPivotLocationInsideSpace(DragTargetLocation);
			FocusDragOffset = DragTargetLocation - GetFocusLocation();
			return;
		}

		DragTargetLocation = DragStartTargetLocation + DragOffsetDelta;
		DragTargetLocation = ClampPivotLocationInsideSpace(DragTargetLocation);
		return;
	}

	bHasDragStartMousePosition = false;
}

void ASRCameraPawn::HandleZoom(const FInputActionValue& Value)
{
	const float AxisValue = Value.Get<float>();
	if (FMath::IsNearlyZero(AxisValue))
	{
		return;
	}

	ZoomDistanceTarget = ClampZoomDistance(ZoomDistanceTarget - (AxisValue * GetZoomSpeed()));
}

void ASRCameraPawn::HandleFocusSurface(const FInputActionValue& Value)
{
	FocusSurfaceInput = Value.Get<FVector2D>();
	if (FocusSurfaceInput.IsNearlyZero())
	{
		bIsFocusSurfaceActive = false;
		return;
	}

	bIsFocusSurfaceActive = ShouldAllowFocusSurface();
}

void ASRCameraPawn::HandleFocusSurfaceCompleted()
{
	FocusSurfaceInput = FVector2D::ZeroVector;
	bIsFocusSurfaceActive = false;
}

void ASRCameraPawn::HandleResetFocus()
{
	ResetFocus();
}

void ASRCameraPawn::HandleAlignFocusSurfaceGrid()
{
	if (!ShouldAllowFocusSurface())
	{
		return;
	}

	const float CurrentZoomDistance = FMath::Max(1.0f, SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	const FQuat BaseViewQuat = GetViewRotationForZoom(CurrentZoomDistance).Quaternion();
	const FQuat ViewQuat = (BaseViewQuat * FocusSurfaceRotation.GetNormalized()).GetNormalized();

	float RollRadians = 0.0f;
	if (!TryComputeFocusSurfaceGridAlignmentRoll(ViewQuat, CurrentZoomDistance, RollRadians)
		|| FMath::IsNearlyZero(RollRadians))
	{
		return;
	}

	const FVector ViewForward = ViewQuat.RotateVector(FVector::ForwardVector).GetSafeNormal();
	if (ViewForward.IsNearlyZero())
	{
		return;
	}

	const FQuat AlignedViewQuat = (FQuat(ViewForward, RollRadians).GetNormalized() * ViewQuat).GetNormalized();
	FocusSurfaceTargetRotation = (BaseViewQuat.Inverse() * AlignedViewQuat).GetNormalized();
	FocusSurfaceRotationSmoothVelocity = FVector::ZeroVector;
	bIsResettingFocusSurfaceRotation = true;
	ClearFocusSurfaceMotion();
}

float ASRCameraPawn::GetScreenSpaceInputScale(float CurrentZoomDistance) const
{
	const float ReferenceZoomDistance = FMath::Max(1.0f, GetScreenSpaceThicknessReferenceZoomDistance());
	const float SafeCurrentZoomDistance = FMath::Max(1.0f, CurrentZoomDistance);
	const float ReferenceFieldOfView = FMath::Clamp(GetScreenSpaceThicknessReferenceFieldOfView(), 5.0f, 170.0f);
	if (!Camera)
	{
		UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires Camera to compute screen-space input scale."));
		return 1.0f;
	}

	const float CurrentFieldOfView = FMath::Clamp(Camera->FieldOfView, 5.0f, 170.0f);
	const float ReferenceTanHalfFieldOfView = FMath::Tan(FMath::DegreesToRadians(ReferenceFieldOfView * 0.5f));
	const float CurrentTanHalfFieldOfView = FMath::Tan(FMath::DegreesToRadians(CurrentFieldOfView * 0.5f));
	if (ReferenceTanHalfFieldOfView <= UE_SMALL_NUMBER)
	{
		return 1.0f;
	}

	const float ScreenSpaceInputScale = (SafeCurrentZoomDistance * CurrentTanHalfFieldOfView)
		/ (ReferenceZoomDistance * ReferenceTanHalfFieldOfView);
	return FMath::Max(ScreenSpaceInputScale, UE_SMALL_NUMBER);
}

float ASRCameraPawn::GetZoomSpeed() const
{
	const float SafeBaseZoomSpeed = FMath::Max(0.0f, ZoomSpeed);
	if (SafeBaseZoomSpeed <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return SafeBaseZoomSpeed * GetScreenSpaceInputScale(
		ZoomDistanceTarget) * FMath::Max(0.0f, ZoomInputScaleMultiplier);
}

float ASRCameraPawn::GetMinimumZoomDistance() const
{
	if (!IsValid(FocusedActor))
	{
		return 0.0f;
	}

	FVector BodyCenter = FVector::ZeroVector;
	float BodyRadius = 0.0f;
	if (!ResolveCelestialCameraAvoidanceSphere(FocusedActor, BodyCenter, BodyRadius))
	{
		return 0.0f;
	}

	return BodyRadius;
}

float ASRCameraPawn::GetSpaceSphereRadius() const
{
	FVector SpaceCenter = FVector::ZeroVector;
	float SpaceRadius = 0.0f;
	return ResolveSpaceBoundary(SpaceCenter, SpaceRadius) ? SpaceRadius : 0.0f;
}

float ASRCameraPawn::ClampZoomDistance(float ZoomDistance) const
{
	const float MaximumZoomDistance = GetMaxZoomDistance();
	if (!FMath::IsFinite(MaximumZoomDistance))
	{
		return FMath::Max(0.0f, ZoomDistance);
	}

	const float MinimumZoomDistance = FMath::Max(0.0f, GetMinimumZoomDistance());
	const float SafeMaximumZoomDistance = FMath::Max(MinimumZoomDistance, MaximumZoomDistance);
	return FMath::Clamp(ZoomDistance, MinimumZoomDistance, SafeMaximumZoomDistance);
}

bool ASRCameraPawn::ResolveSpaceBoundary(FVector& OutCenter, float& OutRadius) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	OutCenter = FVector::ZeroVector;
	OutRadius = 0.0f;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const AActor* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == this)
		{
			continue;
		}

		if (!IsSpaceBoundaryActor(Candidate))
		{
			continue;
		}

		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Candidate);
		Candidate->GetComponents(PrimitiveComponents);
		for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (!IsValid(PrimitiveComponent))
			{
				continue;
			}

			const float CandidateRadius = PrimitiveComponent->Bounds.SphereRadius;
			if (CandidateRadius > OutRadius)
			{
				OutCenter = PrimitiveComponent->Bounds.Origin;
				OutRadius = CandidateRadius;
			}
		}

		if (OutRadius <= KINDA_SMALL_NUMBER)
		{
			FVector ActorOrigin = FVector::ZeroVector;
			FVector ActorExtent = FVector::ZeroVector;
			Candidate->GetActorBounds(false, ActorOrigin, ActorExtent);
			const float ActorRadius = ActorExtent.Size();
			if (ActorRadius > OutRadius)
			{
				OutCenter = ActorOrigin;
				OutRadius = ActorRadius;
			}
		}
	}

	return OutRadius > KINDA_SMALL_NUMBER;
}

FVector ASRCameraPawn::ClampPivotLocationInsideSpace(const FVector& CandidateLocation) const
{
	FVector SpaceCenter = FVector::ZeroVector;
	float SpaceRadius = 0.0f;
	if (!ResolveSpaceBoundary(SpaceCenter, SpaceRadius))
	{
		return CandidateLocation;
	}

	const FVector SpaceToCandidate = CandidateLocation - SpaceCenter;
	const float SafeRadius = FMath::Max(0.0f, SpaceRadius);
	if (SpaceToCandidate.SizeSquared() <= FMath::Square(SafeRadius))
	{
		return CandidateLocation;
	}

	return SpaceCenter + (SpaceToCandidate.GetSafeNormal() * SafeRadius);
}

float ASRCameraPawn::ClampZoomDistanceAgainstSpace(float ZoomDistance, const FVector& CandidatePawnLocation) const
{
	FVector SpaceCenter = FVector::ZeroVector;
	float SpaceRadius = 0.0f;
	if (!ResolveSpaceBoundary(SpaceCenter, SpaceRadius))
	{
		return ZoomDistance;
	}

	const float SafeSpaceRadius = FMath::Max(0.0f, SpaceRadius - FMath::Max(0.0f, CameraSurfacePadding));
	if (SafeSpaceRadius <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const FVector CameraDirection = GetCameraDirectionFromPivot();
	if (CameraDirection.SizeSquared() <= UE_SMALL_NUMBER)
	{
		return ZoomDistance;
	}

	const FVector SpaceToPivot = CandidatePawnLocation - SpaceCenter;
	const float B = FVector::DotProduct(SpaceToPivot, CameraDirection);
	const float C = SpaceToPivot.SizeSquared() - FMath::Square(SafeSpaceRadius);
	const float Discriminant = FMath::Square(B) - C;
	if (Discriminant < 0.0f)
	{
		return 0.0f;
	}

	const float ExitDistance = -B + FMath::Sqrt(Discriminant);
	const float MaximumZoomDistance = FMath::Max(0.0f, ExitDistance);
	return FMath::Min(FMath::Max(0.0f, ZoomDistance), MaximumZoomDistance);
}

bool ASRCameraPawn::ResolveCelestialCameraAvoidanceSphere(const AActor* Actor, FVector& OutCenter, float& OutRadius) const
{
	OutCenter = FVector::ZeroVector;
	OutRadius = 0.0f;

	if (!USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(Actor))
	{
		return false;
	}

	if (USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(Actor))
	{
		return false;
	}

	OutRadius = ComputeScaledBodyRadius(Actor);
	if (OutRadius <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutCenter = Actor->GetActorLocation();

	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Actor);
	Actor->GetComponents(PrimitiveComponents);
	float BestRadius = 0.0f;
	for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent)
			|| !PrimitiveComponent->IsVisible()
			|| PrimitiveComponent->ComponentHasTag(TEXT("StarRovers.GravityLine"))
			|| PrimitiveComponent->ComponentHasTag(TEXT("StarRovers.GravityLineRoot"))
			|| PrimitiveComponent->ComponentHasTag(TEXT("StarRovers.GravityLineSegment"))
			|| PrimitiveComponent->ComponentHasTag(TEXT("StarRovers.RotationAxisLine"))
			|| PrimitiveComponent->ComponentHasTag(TEXT("StarRovers.RotationAxisLineRoot")))
		{
			continue;
		}

		const float CandidateRadius = PrimitiveComponent->Bounds.SphereRadius;
		if (CandidateRadius > BestRadius)
		{
			BestRadius = CandidateRadius;
			OutCenter = PrimitiveComponent->Bounds.Origin;
		}
	}

	OutRadius += FMath::Max(0.0f, CameraSurfacePadding);
	return true;
}

FVector ASRCameraPawn::GetCameraDirectionFromPivot() const
{
	if (Camera)
	{
		const FVector CurrentCameraOffset = Camera->GetComponentLocation() - GetActorLocation();
		if (CurrentCameraOffset.SizeSquared() > UE_SMALL_NUMBER)
		{
			return CurrentCameraOffset.GetSafeNormal();
		}
	}

	if (SpringArm)
	{
		return (-SpringArm->GetForwardVector()).GetSafeNormal();
	}

	return FVector::ForwardVector;
}

float ASRCameraPawn::ClampZoomDistanceAgainstCelestialBodies(float ZoomDistance, const FVector& CandidatePawnLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return ZoomDistance;
	}

	const FVector CameraDirection = GetCameraDirectionFromPivot();
	if (CameraDirection.SizeSquared() <= UE_SMALL_NUMBER)
	{
		return ZoomDistance;
	}

	float AdjustedZoomDistance = FMath::Max(0.0f, ZoomDistance);

	constexpr int32 MaxAvoidancePasses = 3;
	for (int32 PassIndex = 0; PassIndex < MaxAvoidancePasses; ++PassIndex)
	{
		bool bAdjustedThisPass = false;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const AActor* CandidateBody = *It;
			if (!IsValid(CandidateBody) || CandidateBody == this)
			{
				continue;
			}

			FVector BodyCenter = FVector::ZeroVector;
			float BodyRadius = 0.0f;
			if (!ResolveCelestialCameraAvoidanceSphere(CandidateBody, BodyCenter, BodyRadius))
			{
				continue;
			}

			const FVector PivotToBody = CandidatePawnLocation - BodyCenter;
			const float B = FVector::DotProduct(PivotToBody, CameraDirection);
			const float C = PivotToBody.SizeSquared() - FMath::Square(BodyRadius);
			const float Discriminant = FMath::Square(B) - C;
			if (Discriminant < 0.0f)
			{
				continue;
			}

			const float SqrtDiscriminant = FMath::Sqrt(Discriminant);
			const float EntryDistance = -B - SqrtDiscriminant;
			const float ExitDistance = -B + SqrtDiscriminant;
			if (ExitDistance < 0.0f)
			{
				continue;
			}

			constexpr float BoundaryTolerance = 0.1f;
			const float SafeEntryDistance = FMath::Max(0.0f, EntryDistance);
			const bool bPivotIsInsideBody = EntryDistance <= 0.0f;
			const bool bCameraWouldBeInsideBody = bPivotIsInsideBody
				? AdjustedZoomDistance < ExitDistance - BoundaryTolerance
				: AdjustedZoomDistance > EntryDistance + BoundaryTolerance && AdjustedZoomDistance < ExitDistance - BoundaryTolerance;
			if (!bCameraWouldBeInsideBody)
			{
				continue;
			}

			AdjustedZoomDistance = EntryDistance > 0.0f ? SafeEntryDistance : ExitDistance;
			bAdjustedThisPass = true;
		}

		if (!bAdjustedThisPass)
		{
			break;
		}
	}

	return AdjustedZoomDistance;
}

float ASRCameraPawn::GetObliqueViewBlendAlpha(float ZoomDistance) const
{
	if (!UseObliqueView)
	{
		return 0.0f;
	}

	const float MinimumZoomDistance = 0.0f;
	const float MaximumZoomDistance = FMath::Max(1.0f, GetMaxZoomDistance());
	const float StartRatio = FMath::Clamp(ObliqueViewStart, 0.0f, 1.0f);
	const float EndRatio = FMath::Clamp(ObliqueViewEnd, StartRatio + UE_SMALL_NUMBER, 1.0f);
	const float StartZoomDistance = FMath::Lerp(MinimumZoomDistance, MaximumZoomDistance, StartRatio);
	const float EndZoomDistance = FMath::Lerp(MinimumZoomDistance, MaximumZoomDistance, EndRatio);
	const float RawAlpha = (FMath::Max(0.0f, ZoomDistance) - StartZoomDistance) / FMath::Max(EndZoomDistance - StartZoomDistance, UE_SMALL_NUMBER);
	const float ClampedAlpha = FMath::Clamp(RawAlpha, 0.0f, 1.0f);
	const float SmoothAlpha = ClampedAlpha * ClampedAlpha * (3.0f - (2.0f * ClampedAlpha));
	return SmoothAlpha;
}

FRotator ASRCameraPawn::GetViewRotationForZoom(float ZoomDistance) const
{
	const float BlendAlpha = GetObliqueViewBlendAlpha(ZoomDistance);
	return FRotator(
		FMath::Lerp(NearViewRotation.Pitch, FarViewRotation.Pitch, BlendAlpha),
		FMath::Lerp(NearViewRotation.Yaw, FarViewRotation.Yaw, BlendAlpha),
		FMath::Lerp(NearViewRotation.Roll, FarViewRotation.Roll, BlendAlpha)).GetNormalized();
}

void ASRCameraPawn::ApplyZoomDrivenViewRotation(float ZoomDistance)
{
	if (!SpringArm)
	{
		return;
	}

	ObliqueViewStart = FMath::Clamp(ObliqueViewStart, 0.0f, 1.0f);
	ObliqueViewEnd = FMath::Clamp(ObliqueViewEnd, ObliqueViewStart + UE_SMALL_NUMBER, 1.0f);
	const FRotator BaseViewRotation = GetViewRotationForZoom(ZoomDistance);
	if (ShouldAllowFocusSurface())
	{
		const FQuat BaseViewQuat = BaseViewRotation.Quaternion();
		const FQuat SurfaceLookQuat = FocusSurfaceRotation.GetNormalized();
		const FQuat ViewQuat = (BaseViewQuat * SurfaceLookQuat).GetNormalized();
		SpringArm->SetWorldRotation(ViewQuat.Rotator().GetNormalized());
		return;
	}

	SpringArm->SetWorldRotation(BaseViewRotation);
}

bool ASRCameraPawn::ShouldAllowFocusSurface() const
{
	if (!IsValid(FocusedActor) || !USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(FocusedActor))
	{
		return false;
	}

	return true;
}

bool ASRCameraPawn::TryComputeFocusSurfaceGridAlignmentRoll(
	const FQuat& ViewQuat,
	float ZoomDistance,
	float& OutRollRadians) const
{
	OutRollRadians = 0.0f;
	if (!IsValid(FocusedActor) || !SpringArm)
	{
		return false;
	}

	const USRPlanetSurfaceGrid* SurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(FocusedActor);
	if (!IsValid(SurfaceGrid) || SurfaceGrid->GetCellCount() <= 0)
	{
		return false;
	}

	const FVector ViewForward = ViewQuat.RotateVector(FVector::ForwardVector).GetSafeNormal();
	const FVector ViewUp = ViewQuat.RotateVector(FVector::UpVector).GetSafeNormal();
	if (ViewForward.IsNearlyZero() || ViewUp.IsNearlyZero())
	{
		return false;
	}

	const float SafeZoomDistance = FMath::Max(1.0f, ZoomDistance);
	const FVector CameraLocation = GetActorLocation() - (ViewForward * SafeZoomDistance);
	FSRPlanetSurfaceGridCell HitCell;
	FVector HitLocation = FVector::ZeroVector;
	if (!SurfaceGrid->RaycastCell(CameraLocation, ViewForward, HitCell, HitLocation))
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

	float BestRollDistanceRadians = BIG_NUMBER;
	float BestRollTargetRadians = 0.0f;
	auto ConsiderProjectedGridAxis = [
		&BestRollDistanceRadians,
		&BestRollTargetRadians,
		&ViewForward,
		&ViewUp](const FVector& WorldAxis)
	{
		const FVector ProjectedAxis = FVector::VectorPlaneProject(WorldAxis, ViewForward).GetSafeNormal();
		if (ProjectedAxis.IsNearlyZero())
		{
			return;
		}

		const float SinAngle = FVector::DotProduct(ViewForward, FVector::CrossProduct(ViewUp, ProjectedAxis));
		const float CosAngle = FMath::Clamp(FVector::DotProduct(ViewUp, ProjectedAxis), -1.0f, 1.0f);
		const float CandidateRollRadians = FMath::Atan2(SinAngle, CosAngle);
		const float CandidateRollDistanceRadians = FMath::Abs(CandidateRollRadians);
		if (CandidateRollDistanceRadians < BestRollDistanceRadians)
		{
			BestRollDistanceRadians = CandidateRollDistanceRadians;
			BestRollTargetRadians = CandidateRollRadians;
		}
	};

	ConsiderProjectedGridAxis(GridUWorld);
	ConsiderProjectedGridAxis(-GridUWorld);
	ConsiderProjectedGridAxis(GridVWorld);
	ConsiderProjectedGridAxis(-GridVWorld);
	if (BestRollDistanceRadians >= BIG_NUMBER)
	{
		return false;
	}

	OutRollRadians = BestRollTargetRadians;
	return true;
}

void ASRCameraPawn::UpdateFocusSurface(float DeltaSeconds)
{
	if (DeltaSeconds <= UE_SMALL_NUMBER || !ShouldAllowFocusSurface())
	{
		ClearFocusSurfaceMotion();
		return;
	}

	const FVector2D CombinedLookInput = FocusSurfaceInput.GetClampedToMaxSize(1.0f);
	const bool bHasDirectInput = !CombinedLookInput.IsNearlyZero();
	const float SafeLookSpeed = FMath::Max(0.0f, FocusSurfaceSpeed);
	const float SafeMinInertiaSpeed = FMath::Max(0.0f, FocusSurfaceMinInertiaSpeed);
	bool bAppliedDirectInput = false;

	const float InputInterpRate = bHasDirectInput
		? FMath::Max(0.0f, FocusSurfaceInputAcceleration)
		: FMath::Max(0.0f, FocusSurfaceInputDeceleration);
	if (InputInterpRate <= KINDA_SMALL_NUMBER)
	{
		FocusSurfaceAcceleratedInput = CombinedLookInput;
	}
	else
	{
		FocusSurfaceAcceleratedInput.X = FMath::FInterpConstantTo(FocusSurfaceAcceleratedInput.X, CombinedLookInput.X, DeltaSeconds, InputInterpRate);
		FocusSurfaceAcceleratedInput.Y = FMath::FInterpConstantTo(FocusSurfaceAcceleratedInput.Y, CombinedLookInput.Y, DeltaSeconds, InputInterpRate);
		FocusSurfaceAcceleratedInput = FocusSurfaceAcceleratedInput.GetClampedToMaxSize(1.0f);
	}

	if (!bHasDirectInput && FocusSurfaceAcceleratedInput.IsNearlyZero())
	{
		FocusSurfaceAcceleratedInput = FVector2D::ZeroVector;
	}

	if (!FocusSurfaceAcceleratedInput.IsNearlyZero() && SafeLookSpeed > KINDA_SMALL_NUMBER)
	{
		ApplyFocusSurfaceDelta(FVector2D(-FocusSurfaceAcceleratedInput.X, FocusSurfaceAcceleratedInput.Y) * SafeLookSpeed * DeltaSeconds);
		if (bHasDirectInput)
		{
			FocusSurfaceAngularVelocity = FVector2D::ZeroVector;
		}
		bAppliedDirectInput = true;
	}

	if (!bHasDirectInput && !bIsDraggingFocusSurface && !FocusSurfaceAngularVelocity.IsNearlyZero(SafeMinInertiaSpeed))
	{
		ApplyFocusSurfaceDelta(FocusSurfaceAngularVelocity * DeltaSeconds);

		const float SafeDamping = FMath::Max(0.0f, FocusSurfaceInertiaDamping);
		if (SafeDamping <= KINDA_SMALL_NUMBER)
		{
			FocusSurfaceAngularVelocity = FVector2D::ZeroVector;
		}
		else
		{
			FocusSurfaceAngularVelocity.X = FMath::FInterpTo(FocusSurfaceAngularVelocity.X, 0.0f, DeltaSeconds, SafeDamping);
			FocusSurfaceAngularVelocity.Y = FMath::FInterpTo(FocusSurfaceAngularVelocity.Y, 0.0f, DeltaSeconds, SafeDamping);
		}
	}

	if (FocusSurfaceAngularVelocity.IsNearlyZero(SafeMinInertiaSpeed))
	{
		FocusSurfaceAngularVelocity = FVector2D::ZeroVector;
	}

	bIsFocusSurfaceActive = bAppliedDirectInput || bIsDraggingFocusSurface || !FocusSurfaceAngularVelocity.IsNearlyZero();
}

void ASRCameraPawn::UpdateFocusSurfaceRotation(float DeltaSeconds)
{
	if (DeltaSeconds <= UE_SMALL_NUMBER)
	{
		return;
	}

	if (!ShouldAllowFocusSurface())
	{
		FocusSurfaceRotation = FQuat::Identity;
		FocusSurfaceTargetRotation = FQuat::Identity;
		FocusSurfaceRotationSmoothVelocity = FVector::ZeroVector;
		bIsResettingFocusSurfaceRotation = false;
		return;
	}

	if (!bIsResettingFocusSurfaceRotation)
	{
		return;
	}

	FocusSurfaceRotation = SmoothDampQuat(
		FocusSurfaceRotation,
		FocusSurfaceTargetRotation,
		FocusSurfaceRotationSmoothVelocity,
		FocusFollowSmoothTime,
		DeltaSeconds);

	const FQuat RemainingRotation = (FocusSurfaceTargetRotation.GetNormalized() * FocusSurfaceRotation.GetNormalized().Inverse()).GetNormalized();
	const float RemainingAngleDegrees = FMath::RadiansToDegrees(RemainingRotation.GetAngle());
	if (RemainingAngleDegrees <= 0.05f && FocusSurfaceRotationSmoothVelocity.SizeSquared() <= KINDA_SMALL_NUMBER)
	{
		FocusSurfaceRotation = FocusSurfaceTargetRotation.GetNormalized();
		FocusSurfaceRotationSmoothVelocity = FVector::ZeroVector;
		bIsResettingFocusSurfaceRotation = false;
		bIsFocusSurfaceActive = false;
		return;
	}

	bIsFocusSurfaceActive = true;
}

void ASRCameraPawn::ApplyFocusSurfaceDelta(const FVector2D& DegreesDelta)
{
	if (DegreesDelta.IsNearlyZero())
	{
		return;
	}

	FocusSurfaceRotationSmoothVelocity = FVector::ZeroVector;
	bIsResettingFocusSurfaceRotation = false;

	FQuat CurrentRotation = FocusSurfaceRotation.GetNormalized();
	if (!FMath::IsNearlyZero(DegreesDelta.X))
	{
		const FVector CurrentUpAxis = CurrentRotation.RotateVector(FVector::UpVector).GetSafeNormal();
		const FQuat YawDelta(CurrentUpAxis, FMath::DegreesToRadians(DegreesDelta.X));
		CurrentRotation = (YawDelta * CurrentRotation).GetNormalized();
	}

	if (!FMath::IsNearlyZero(DegreesDelta.Y))
	{
		const FVector CurrentRightAxis = CurrentRotation.RotateVector(FVector::RightVector).GetSafeNormal();
		const FQuat PitchDelta(CurrentRightAxis, FMath::DegreesToRadians(DegreesDelta.Y));
		CurrentRotation = (PitchDelta * CurrentRotation).GetNormalized();
	}

	FocusSurfaceRotation = CurrentRotation;
	FocusSurfaceTargetRotation = CurrentRotation;
}

void ASRCameraPawn::RefreshScreenSpaceThicknessReferenceView()
{
	ScreenSpaceThicknessReferenceZoomDistance = FMath::Max(1.0f, SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	if (!Camera)
	{
		UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires Camera to refresh screen-space thickness reference FOV."));
		ScreenSpaceThicknessReferenceFieldOfView = 0.0f;
		return;
	}
	ScreenSpaceThicknessReferenceFieldOfView = FMath::Clamp(Camera->FieldOfView, 5.0f, 170.0f);
}

void ASRCameraPawn::UpdateDynamicMeshVisibility()
{
	if (!Camera)
	{
		return;
	}

	constexpr double MinRefreshIntervalSeconds = 0.10;
	constexpr float MinCameraMoveDistance = 25.0f;
	constexpr float MinZoomDelta = 25.0f;
	constexpr float MinRotationDeltaDegrees = 0.25f;

	const UWorld* World = GetWorld();
	const double CurrentTime = World ? World->GetTimeSeconds() : 0.0;
	const FVector CameraLocation = Camera->GetComponentLocation();
	const FRotator CameraRotation = Camera->GetComponentRotation();
	const float CurrentZoomDistance = SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget;
	AActor* CurrentFocusedActor = FocusedActor.Get();

	const bool bFocusChanged = LastDynamicMeshVisibilityFocusedActor.Get() != CurrentFocusedActor;
	const bool bMovedEnough = FVector::DistSquared(CameraLocation, LastDynamicMeshVisibilityCameraLocation) >= FMath::Square(MinCameraMoveDistance);
	const bool bZoomedEnough = FMath::Abs(CurrentZoomDistance - LastDynamicMeshVisibilityZoomDistance) >= MinZoomDelta;
	const bool bRotatedEnough = !CameraRotation.Equals(LastDynamicMeshVisibilityCameraRotation, MinRotationDeltaDegrees);
	const bool bIntervalElapsed = (CurrentTime - LastDynamicMeshVisibilityUpdateTime) >= MinRefreshIntervalSeconds;
	if (bHasDynamicMeshVisibilityState && !bFocusChanged && !bMovedEnough && !bZoomedEnough && !bRotatedEnough && !bIntervalElapsed)
	{
		return;
	}

	AActor* DirectionalLightTarget = nullptr;
	if (ApplyCelestialBodyMeshVisibility(DirectionalLightTarget))
	{
		ConfigureDirectionalLight(DirectionalLightTarget);
	}
	else
	{
		ConfigureDirectionalLight(nullptr);
	}

	LastDynamicMeshVisibilityCameraLocation = CameraLocation;
	LastDynamicMeshVisibilityCameraRotation = CameraRotation;
	LastDynamicMeshVisibilityFocusedActor = CurrentFocusedActor;
	LastDynamicMeshVisibilityZoomDistance = CurrentZoomDistance;
	LastDynamicMeshVisibilityUpdateTime = CurrentTime;
	bHasDynamicMeshVisibilityState = true;
}

bool ASRCameraPawn::ApplyCelestialBodyMeshVisibility(AActor*& OutDirectionalLightTarget)
{
	OutDirectionalLightTarget = nullptr;

	USRCelestialBodyRegistrySubsystem* CelestialRegistry = FindCelestialRegistry();
	if (!CelestialRegistry)
	{
		return false;
	}

	TArray<AActor*> CelestialBodies;
	CelestialRegistry->GetCelestialBodies(CelestialBodies);
	if (CelestialBodies.IsEmpty())
	{
		CelestialRegistry->RefreshCelestialBodies();
		CelestialRegistry->GetCelestialBodies(CelestialBodies);
		if (CelestialBodies.IsEmpty())
		{
			return false;
		}
	}

	TArray<ASRCelestialBody*> ValidCelestialBodies;
	ValidCelestialBodies.Reserve(CelestialBodies.Num());
	ASRCelestialBody* FocusedDynamicBody = nullptr;
	ASRCelestialBody* BestNonFocusedDynamicBody = nullptr;
	float BestDynamicMeshTargetScreenSizeRatio = 0.0f;
	for (AActor* BodyActor : CelestialBodies)
	{
		ASRCelestialBody* CelestialBody = Cast<ASRCelestialBody>(BodyActor);
		if (!IsValid(CelestialBody))
		{
			continue;
		}

		ValidCelestialBodies.Add(CelestialBody);
		float ScreenSizeRatio = 0.0f;
		if (!ShouldUseDynamicMesh(BodyActor, ScreenSizeRatio) || USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(BodyActor))
		{
			continue;
		}

		if (BodyActor == FocusedActor.Get())
		{
			FocusedDynamicBody = CelestialBody;
			OutDirectionalLightTarget = BodyActor;
			BestDynamicMeshTargetScreenSizeRatio = TNumericLimits<float>::Max();
			continue;
		}

		if (!FocusedDynamicBody && ScreenSizeRatio > BestDynamicMeshTargetScreenSizeRatio)
		{
			BestNonFocusedDynamicBody = CelestialBody;
			OutDirectionalLightTarget = BodyActor;
			BestDynamicMeshTargetScreenSizeRatio = ScreenSizeRatio;
		}
	}

	ASRCelestialBody* DynamicBody = FocusedDynamicBody ? FocusedDynamicBody : BestNonFocusedDynamicBody;
	bool bHasStaticMeshBody = false;
	for (ASRCelestialBody* CelestialBody : ValidCelestialBodies)
	{
		const bool bUseDynamicMesh = CelestialBody == DynamicBody;
		CelestialBody->SetCelestialBodyMesh(bUseDynamicMesh);
		bHasStaticMeshBody |= !bUseDynamicMesh;
	}

	if (AActor* PrimaryStarActor = CelestialRegistry->GetPrimaryStarActor())
	{
		if (UPointLightComponent* StarPointLight = PrimaryStarActor->FindComponentByClass<UPointLightComponent>())
		{
			StarPointLight->SetVisibility(bHasStaticMeshBody || !IsValid(OutDirectionalLightTarget));
		}
	}

	return true;
}

bool ASRCameraPawn::ShouldUseDynamicMesh(const AActor* BodyActor, float& OutScreenSizeRatio) const
{
	constexpr float NonFocusedDynamicMeshScreenSizeThreshold = 0.15f;

	OutScreenSizeRatio = 0.0f;
	const ASRCelestialBody* CelestialBody = Cast<ASRCelestialBody>(BodyActor);
	if (!IsValid(CelestialBody) || !Camera)
	{
		return false;
	}

	const FVector CameraLocation = Camera->GetComponentLocation();
	const FVector CameraForward = Camera->GetForwardVector().GetSafeNormal();
	const FVector CameraToBody = BodyActor->GetActorLocation() - CameraLocation;
	const float Depth = FVector::DotProduct(CameraToBody, CameraForward);
	if (Depth <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float BodyRadius = ComputeScaledBodyRadius(BodyActor);
	if (BodyRadius <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float SafeFieldOfViewDegrees = FMath::Clamp(Camera->FieldOfView, 5.0f, 170.0f);
	const float TanHalfFieldOfView = FMath::Tan(FMath::DegreesToRadians(SafeFieldOfViewDegrees * 0.5f));
	if (TanHalfFieldOfView <= UE_SMALL_NUMBER)
	{
		return false;
	}

	float AspectRatio = 16.0f / 9.0f;
	if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;
		PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
		if (ViewportWidth > 0 && ViewportHeight > 0)
		{
			AspectRatio = static_cast<float>(ViewportWidth) / static_cast<float>(ViewportHeight);
		}
	}

	const float HalfFrustumHeight = Depth * TanHalfFieldOfView;
	const float HalfFrustumWidth = HalfFrustumHeight * FMath::Max(AspectRatio, UE_SMALL_NUMBER);
	const float HorizontalOffset = FMath::Abs(FVector::DotProduct(CameraToBody, Camera->GetRightVector()));
	const float VerticalOffset = FMath::Abs(FVector::DotProduct(CameraToBody, Camera->GetUpVector()));
	if (HorizontalOffset > HalfFrustumWidth + BodyRadius || VerticalOffset > HalfFrustumHeight + BodyRadius)
	{
		return false;
	}

	OutScreenSizeRatio = USRCelestialBodyRuntimeLibrary::GetScreenScale(
		BodyActor,
		CameraLocation,
		CameraForward,
		Camera->FieldOfView);
	if (BodyActor == FocusedActor.Get())
	{
		return true;
	}

	return OutScreenSizeRatio >= NonFocusedDynamicMeshScreenSizeThreshold;
}

void ASRCameraPawn::ConfigureDirectionalLight(AActor* LightingTarget)
{
	ADirectionalLight* DirectionalLightActor = FindDirectionalLightActor();
	UDirectionalLightComponent* DirectionalLightComponent = IsValid(DirectionalLightActor)
		? DirectionalLightActor->FindComponentByClass<UDirectionalLightComponent>()
		: nullptr;
	if (!IsValid(DirectionalLightComponent))
	{
		return;
	}

	USRCelestialBodyRegistrySubsystem* CelestialRegistry = FindCelestialRegistry();
	const AActor* PrimaryStarActor = CelestialRegistry ? CelestialRegistry->GetPrimaryStarActor() : nullptr;
	const bool bCanUseDirectionalLight = IsValid(PrimaryStarActor) && IsValid(LightingTarget);
	DirectionalLightComponent->SetVisibility(bCanUseDirectionalLight);
	if (!bCanUseDirectionalLight)
	{
		return;
	}

	const FVector StarToTargetDirection = (LightingTarget->GetActorLocation() - PrimaryStarActor->GetActorLocation()).GetSafeNormal();
	if (StarToTargetDirection.SizeSquared() <= UE_SMALL_NUMBER)
	{
		return;
	}

	DirectionalLightActor->SetActorRotation(StarToTargetDirection.Rotation());
}

ADirectionalLight* ASRCameraPawn::FindDirectionalLightActor() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		ADirectionalLight* CandidateLight = *It;
		if (IsValid(CandidateLight))
		{
			return CandidateLight;
		}
	}

	return nullptr;
}

USRCelestialBodyRegistrySubsystem* ASRCameraPawn::FindCelestialRegistry() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USRCelestialBodyRegistrySubsystem>() : nullptr;
}

bool ASRCameraPawn::HasExitedFocusedActorGravityField() const
{
	if (!IsValid(FocusedActor))
	{
		return false;
	}

	const USRGravityParent* GravityParent = FocusedActor->FindComponentByClass<USRGravityParent>();
	if (!IsValid(GravityParent))
	{
		return false;
	}

	const float GravityRadius = GravityParent->GetGravityRadius();
	if (GravityRadius <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	return FocusDragOffset.SizeSquared() > FMath::Square(GravityRadius);
}

bool ASRCameraPawn::GetMouseScreenPosition(FVector2D& OutMouseScreenPosition) const
{
	OutMouseScreenPosition = FVector2D::ZeroVector;

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return false;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!PlayerController->GetMousePosition(MouseX, MouseY))
	{
		return false;
	}

	OutMouseScreenPosition = FVector2D(MouseX, MouseY);
	return true;
}

FVector ASRCameraPawn::ConvertScreenDragToDragOffset(const FVector2D& StartScreenPosition, const FVector2D& CurrentScreenPosition) const
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController || !Camera)
	{
		return FVector::ZeroVector;
	}

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	if (ViewportWidth <= 0 || ViewportHeight <= 0)
	{
		return FVector::ZeroVector;
	}

	const FVector PlaneOrigin = DragStartTargetLocation;
	const FVector PlaneNormal = Camera
		? Camera->GetForwardVector().GetSafeNormal()
		: SpringArm ? SpringArm->GetForwardVector().GetSafeNormal() : FVector::ForwardVector;
	if (PlaneNormal.SizeSquared() <= UE_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float DistanceToDragPlane = FVector::DotProduct(PlaneOrigin - Camera->GetComponentLocation(), PlaneNormal);
	if (DistanceToDragPlane <= UE_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float ReferenceZoomDistance = FMath::Max(1.0f, GetScreenSpaceThicknessReferenceZoomDistance());
	const float ReferenceFieldOfView = FMath::Clamp(GetScreenSpaceThicknessReferenceFieldOfView(), 5.0f, 170.0f);
	const float ReferenceTanHalfFieldOfView = FMath::Tan(FMath::DegreesToRadians(ReferenceFieldOfView * 0.5f));
	const float ReferenceWorldUnitsPerPixelVertical = (ReferenceZoomDistance * ReferenceTanHalfFieldOfView * 2.0f)
		/ static_cast<float>(ViewportHeight);
	const float AdaptiveScale = GetScreenSpaceInputScale(DistanceToDragPlane)
		* FMath::Max(0.0f, LeftDragInputScaleMultiplier);
	const float WorldUnitsPerPixelVertical = ReferenceWorldUnitsPerPixelVertical * AdaptiveScale;
	const float WorldUnitsPerPixelHorizontal = WorldUnitsPerPixelVertical;

	const FVector2D ScreenDelta = CurrentScreenPosition - StartScreenPosition;
	return (-Camera->GetRightVector() * (ScreenDelta.X * WorldUnitsPerPixelHorizontal))
		+ (Camera->GetUpVector() * (ScreenDelta.Y * WorldUnitsPerPixelVertical));
}

FVector ASRCameraPawn::GetFocusLocation() const
{
	if (!FocusedActor)
	{
		return DragTargetLocation;
	}

	return FocusedActor->GetActorLocation();
}

bool ASRCameraPawn::ShouldDragFocusedSurface() const
{
	return ShouldAllowFocusSurface();
}

void ASRCameraPawn::HandleFocusSurfaceDrag(const FVector2D& DragDelta)
{
	if (!ShouldAllowFocusSurface() || DragDelta.IsNearlyZero())
	{
		return;
	}

	const float CurrentZoomDistance = FMath::Max(1.0f, SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	const float UnclampedAdaptiveScale = GetScreenSpaceInputScale(CurrentZoomDistance)
		* FMath::Max(0.0f, RightDragInputScaleMultiplier);
	const float SafeRightDragInputScaleMax = FMath::Max(0.0f, RightDragInputScaleMax);
	const float AdaptiveScale = SafeRightDragInputScaleMax > KINDA_SMALL_NUMBER
		? FMath::Min(UnclampedAdaptiveScale, SafeRightDragInputScaleMax)
		: UnclampedAdaptiveScale;
	const FVector2D DegreesDelta(
		DragDelta.X * FMath::Max(0.0f, SurfaceRotateSensitivity) * AdaptiveScale,
		DragDelta.Y * FMath::Max(0.0f, SurfaceRotateSensitivity) * AdaptiveScale);
	if (DegreesDelta.IsNearlyZero())
	{
		return;
	}

	ApplyFocusSurfaceDelta(DegreesDelta);

	const UWorld* World = GetWorld();
	const float DeltaSeconds = World ? World->GetDeltaSeconds() : 0.0f;
	if (DeltaSeconds > UE_SMALL_NUMBER)
	{
		FocusSurfaceAngularVelocity = DegreesDelta / DeltaSeconds;
	}
	bIsFocusSurfaceActive = true;
}

void ASRCameraPawn::ClearFocusSurfaceMotion()
{
	FocusSurfaceInput = FVector2D::ZeroVector;
	FocusSurfaceAcceleratedInput = FVector2D::ZeroVector;
	FocusSurfaceAngularVelocity = FVector2D::ZeroVector;
	bIsFocusSurfaceActive = false;
	bIsDraggingFocusSurface = false;
}
