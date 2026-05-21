#include "Camera/SRCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
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
#include "UObject/ConstructorHelpers.h"

namespace StarRoversInputPaths
{
	static constexpr TCHAR DefaultMappingContext[] = TEXT("/Game/BlueprintClasses/Core/IMC_SR.IMC_SR");
	static constexpr TCHAR DragHoldAction[] = TEXT("/Game/BlueprintClasses/Core/IA_DragHold.IA_DragHold");
	static constexpr TCHAR DragDeltaAction[] = TEXT("/Game/BlueprintClasses/Core/IA_DragDelta.IA_DragDelta");
	static constexpr TCHAR ZoomAction[] = TEXT("/Game/BlueprintClasses/Core/IA_Zoom.IA_Zoom");
	static constexpr TCHAR FocusedSurfaceLookAction[] = TEXT("/Game/BlueprintClasses/Core/IA_FocusedSurfaceLook.IA_FocusedSurfaceLook");
	static constexpr TCHAR ResetFocusCameraAction[] = TEXT("/Game/BlueprintClasses/Core/IA_ResetFocusCamera.IA_ResetFocusCamera");
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
}

ASRCameraPawn::ASRCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	SetRootComponent(SceneRoot);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(SceneRoot);

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
	CameraSurfacePadding = 100.0f;
	if (Camera)
	{
		Camera->SetFieldOfView(DefaultCameraFieldOfView);
	}
	bUseObliqueView = true;
	NearViewRotation = FRotator::ZeroRotator;
	FarViewRotation = FRotator(60.0f, 0.0f, 0.0f);
	ObliqueViewStart = 0.3f;
	ObliqueViewEnd = 1.0f;
	FocusFollowSmoothTime = 0.35f;
	SurfaceRotateSensitivity = 0.2f;
	FocusedSurfaceLookSpeed = 60.0f;
	FocusedSurfaceLookMaxPitch = 75.0f;
	DragTargetLocation = FVector::ZeroVector;
	ZoomDistanceTarget = SpringArm->TargetArmLength;
	bIsDragging = false;
	bMappingContextApplied = false;
	bIsRotatingFocusedBody = false;
	bIsFocusedSurfaceLookActive = false;
	bHasDragStartMousePosition = false;
	FocusDragOffset = FVector::ZeroVector;
	FixedPlaneX = 0.0f;
	FocusTrackingDelta = FVector::ZeroVector;
	FocusTrackingDeltaVelocity = FVector::ZeroVector;
	ScreenSpaceThicknessReferenceZoomDistance = 0.0f;
	ScreenSpaceThicknessReferenceFieldOfView = 0.0f;
	DragStartMouseScreenPosition = FVector2D::ZeroVector;
	DragStartFocusDragOffset = FVector::ZeroVector;
	DragStartTargetLocation = FVector::ZeroVector;
	FocusedSurfaceLookInput = FVector2D::ZeroVector;
	FocusedSurfaceLookOffset = FRotator::ZeroRotator;

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

	static ConstructorHelpers::FObjectFinder<UInputAction> FocusedSurfaceLookFinder(StarRoversInputPaths::FocusedSurfaceLookAction);
	if (FocusedSurfaceLookFinder.Succeeded())
	{
		FocusedSurfaceLookAction = FocusedSurfaceLookFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRCameraPawn requires FocusedSurfaceLookAction at '%s' for focused surface camera look."), StarRoversInputPaths::FocusedSurfaceLookAction);
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> ResetFocusCameraFinder(StarRoversInputPaths::ResetFocusCameraAction);
	if (ResetFocusCameraFinder.Succeeded())
	{
		ResetFocusCameraAction = ResetFocusCameraFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRCameraPawn requires ResetFocusCameraAction at '%s' for focused camera reset."), StarRoversInputPaths::ResetFocusCameraAction);
	}

	ApplyZoomDrivenViewRotation(SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	RefreshScreenSpaceThicknessReferenceView();
}

void ASRCameraPawn::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyZoomDrivenViewRotation(SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	RefreshScreenSpaceThicknessReferenceView();
}

void ASRCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	FixedPlaneX = GetActorLocation().X;
	DragTargetLocation = GetActorLocation();
	DragTargetLocation.X = FixedPlaneX;
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
	UpdateFocusedSurfaceLook(DeltaSeconds);

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
				DragTargetLocation.X = FixedPlaneX;
			}
		}
		else
		{
			ClearFocusActor();
		}
	}

	const FVector DesiredLocation(FixedPlaneX, DragTargetLocation.Y, DragTargetLocation.Z);
	FVector NewLocation = DesiredLocation;
	if (bIsDragging)
	{
		FocusTrackingDelta = FVector::ZeroVector;
		FocusTrackingDeltaVelocity = FVector::ZeroVector;
		NewLocation = DesiredLocation;
	}
	else if (FocusedActor)
	{
		FocusTrackingDelta = SmoothDampVector(
			FocusTrackingDelta,
			FVector::ZeroVector,
			FocusTrackingDeltaVelocity,
			FocusFollowSmoothTime,
			DeltaSeconds);
		FocusTrackingDelta.X = 0.0f;
		FocusTrackingDeltaVelocity.X = 0.0f;

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

	if (SpringArm)
	{
		ZoomDistanceTarget = ClampZoomDistance(ZoomDistanceTarget);
		SpringArm->TargetArmLength = ClampZoomDistance(SpringArm->TargetArmLength);
		ApplyZoomDrivenViewRotation(ZoomDistanceTarget);
		ZoomDistanceTarget = ClampZoomDistanceAgainstCelestialBodies(ZoomDistanceTarget, NewLocation);

		const float InterpolatedZoom = FMath::FInterpTo(SpringArm->TargetArmLength, ZoomDistanceTarget, DeltaSeconds, DefaultZoomInterpSpeed);
		SpringArm->TargetArmLength = ClampZoomDistanceAgainstCelestialBodies(ClampZoomDistance(InterpolatedZoom), NewLocation);
		ApplyZoomDrivenViewRotation(SpringArm->TargetArmLength);
	}

	NewLocation.X = FixedPlaneX;
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

		if (FocusedSurfaceLookAction)
		{
			EnhancedInputComponent->BindAction(FocusedSurfaceLookAction, ETriggerEvent::Triggered, this, &ASRCameraPawn::HandleFocusedSurfaceLook);
			EnhancedInputComponent->BindAction(FocusedSurfaceLookAction, ETriggerEvent::Completed, this, &ASRCameraPawn::HandleFocusedSurfaceLookCompleted);
			EnhancedInputComponent->BindAction(FocusedSurfaceLookAction, ETriggerEvent::Canceled, this, &ASRCameraPawn::HandleFocusedSurfaceLookCompleted);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRCameraPawn requires FocusedSurfaceLookAction before focused surface look input binding."));
		}

		if (ResetFocusCameraAction)
		{
			EnhancedInputComponent->BindAction(ResetFocusCameraAction, ETriggerEvent::Started, this, &ASRCameraPawn::HandleResetFocusedCameraView);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRCameraPawn requires ResetFocusCameraAction before focused camera reset input binding."));
		}
	}
}

void ASRCameraPawn::FocusActor(AActor* NewFocusActor)
{
	AActor* PreviousFocusedActor = FocusedActor.Get();
	FocusedActor = NewFocusActor;
	FocusDragOffset = FVector::ZeroVector;
	FocusedSurfaceLookInput = FVector2D::ZeroVector;
	FocusedSurfaceLookOffset = FRotator::ZeroRotator;
	bIsFocusedSurfaceLookActive = false;

	if (FocusedActor)
	{
		DragTargetLocation = GetFocusLocation();
		DragTargetLocation.X = FixedPlaneX;
		const FVector CurrentLocation(FixedPlaneX, GetActorLocation().Y, GetActorLocation().Z);
		const FVector DesiredLocation(FixedPlaneX, DragTargetLocation.Y, DragTargetLocation.Z);
		FocusTrackingDelta = DesiredLocation - CurrentLocation;
		FocusTrackingDelta.X = 0.0f;
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
	}

	BroadcastFocusedActorChangedIfNeeded(PreviousFocusedActor);
}

void ASRCameraPawn::ClearFocusActor()
{
	AActor* PreviousFocusedActor = FocusedActor.Get();
	if (FocusedActor)
	{
		DragTargetLocation = FVector(FixedPlaneX, GetActorLocation().Y, GetActorLocation().Z);
	}

	FocusedActor = nullptr;
	FocusDragOffset = FVector::ZeroVector;
	FocusTrackingDelta = FVector::ZeroVector;
	FocusTrackingDeltaVelocity = FVector::ZeroVector;
	FocusedSurfaceLookInput = FVector2D::ZeroVector;
	FocusedSurfaceLookOffset = FRotator::ZeroRotator;
	bIsFocusedSurfaceLookActive = false;
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
	FocusTrackingDelta = FVector::ZeroVector;
	FocusTrackingDeltaVelocity = FVector::ZeroVector;
	if (FocusedActor)
	{
		DragTargetLocation = GetFocusLocation() + FocusDragOffset;
		DragTargetLocation.X = FixedPlaneX;
	}

	const FVector DesiredLocation(FixedPlaneX, DragTargetLocation.Y, DragTargetLocation.Z);

	if (SpringArm)
	{
		ZoomDistanceTarget = ClampZoomDistance(ZoomDistanceTarget);
		SpringArm->TargetArmLength = ZoomDistanceTarget;
		ApplyZoomDrivenViewRotation(SpringArm->TargetArmLength);
	}
	SetActorLocation(DesiredLocation);
}

void ASRCameraPawn::ResetFocusedCameraView()
{
	if (!IsValid(FocusedActor))
	{
		return;
	}

	FocusDragOffset = FVector::ZeroVector;
	FocusedSurfaceLookInput = FVector2D::ZeroVector;
	FocusedSurfaceLookOffset = FRotator::ZeroRotator;
	bIsFocusedSurfaceLookActive = false;
	bIsDragging = false;
	bIsRotatingFocusedBody = false;
	bHasDragStartMousePosition = false;

	DragTargetLocation = GetFocusLocation();
	DragTargetLocation.X = FixedPlaneX;

	const FVector CurrentLocation(FixedPlaneX, GetActorLocation().Y, GetActorLocation().Z);
	const FVector DesiredLocation(FixedPlaneX, DragTargetLocation.Y, DragTargetLocation.Z);
	FocusTrackingDelta = DesiredLocation - CurrentLocation;
	FocusTrackingDelta.X = 0.0f;
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
	ApplyZoomDrivenViewRotation(ZoomDistanceTarget);
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

void ASRCameraPawn::BroadcastFocusedActorChangedIfNeeded(AActor* PreviousFocusedActor)
{
	if (PreviousFocusedActor != FocusedActor)
	{
		FocusedActorChangedEvent.Broadcast(FocusedActor.Get());
	}
}

void ASRCameraPawn::HandleDragHoldStarted()
{
	bIsRotatingFocusedBody = ShouldRotateFocusedBody();
	bIsDragging = !bIsRotatingFocusedBody;
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
	bIsDragging = false;
	bIsRotatingFocusedBody = false;
	bHasDragStartMousePosition = false;
}

void ASRCameraPawn::HandleDragDelta(const FInputActionValue& Value)
{
	const FVector2D DragDelta = Value.Get<FVector2D>();
	if (DragDelta.IsNearlyZero())
	{
		return;
	}

	if (bIsRotatingFocusedBody)
	{
		HandleFocusedBodyRotation(DragDelta);
		return;
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

		const FVector2D ScreenDelta = CurrentMouseScreenPosition - DragStartMouseScreenPosition;
		const FVector DragOffsetDelta = ConvertScreenDeltaToDragOffset(ScreenDelta);

		if (FocusedActor)
		{
			FocusDragOffset = DragStartFocusDragOffset + DragOffsetDelta;
			FocusDragOffset.X = 0.0f;
			DragTargetLocation = GetFocusLocation() + FocusDragOffset;
			DragTargetLocation.X = FixedPlaneX;
			FocusDragOffset = DragTargetLocation - GetFocusLocation();
			FocusDragOffset.X = 0.0f;
			return;
		}

		DragTargetLocation = DragStartTargetLocation + DragOffsetDelta;
		DragTargetLocation.X = FixedPlaneX;
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

void ASRCameraPawn::HandleFocusedSurfaceLook(const FInputActionValue& Value)
{
	FocusedSurfaceLookInput = Value.Get<FVector2D>();
	if (FocusedSurfaceLookInput.IsNearlyZero())
	{
		bIsFocusedSurfaceLookActive = false;
		return;
	}

	bIsFocusedSurfaceLookActive = ShouldAllowFocusedSurfaceLook();
}

void ASRCameraPawn::HandleFocusedSurfaceLookCompleted()
{
	FocusedSurfaceLookInput = FVector2D::ZeroVector;
	bIsFocusedSurfaceLookActive = false;
}

void ASRCameraPawn::HandleResetFocusedCameraView()
{
	ResetFocusedCameraView();
}

float ASRCameraPawn::GetZoomSpeed() const
{
	const float SafeBaseZoomSpeed = FMath::Max(0.0f, ZoomSpeed);
	if (SafeBaseZoomSpeed <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float ReferenceZoomDistance = FMath::Max(1.0f, GetScreenSpaceThicknessReferenceZoomDistance());
	const float CurrentZoomDistance = FMath::Max(1.0f, ZoomDistanceTarget);
	const float ReferenceFieldOfView = FMath::Clamp(GetScreenSpaceThicknessReferenceFieldOfView(), 5.0f, 170.0f);
	if (!Camera)
	{
		UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires Camera to compute zoom speed."));
		return 0.0f;
	}
	const float CurrentFieldOfView = FMath::Clamp(Camera->FieldOfView, 5.0f, 170.0f);
	const float ReferenceTanHalfFieldOfView = FMath::Tan(FMath::DegreesToRadians(ReferenceFieldOfView * 0.5f));
	const float CurrentTanHalfFieldOfView = FMath::Tan(FMath::DegreesToRadians(CurrentFieldOfView * 0.5f));
	if (ReferenceTanHalfFieldOfView <= UE_SMALL_NUMBER)
	{
		return SafeBaseZoomSpeed;
	}

	const float ScreenSpaceZoomScale = (CurrentZoomDistance * CurrentTanHalfFieldOfView)
		/ (ReferenceZoomDistance * ReferenceTanHalfFieldOfView);
	return SafeBaseZoomSpeed * FMath::Max(ScreenSpaceZoomScale, UE_SMALL_NUMBER);
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

bool ASRCameraPawn::ResolveCelestialCameraAvoidanceSphere(const AActor* Actor, FVector& OutCenter, float& OutRadius) const
{
	OutCenter = FVector::ZeroVector;
	OutRadius = 0.0f;

	if (!USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(Actor))
	{
		return false;
	}

	OutRadius = USRCelestialBodyRuntimeLibrary::GetCelestialApproximateRadius(Actor);
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
			|| PrimitiveComponent->ComponentHasTag(TEXT("StarRovers.GravityLineSegment")))
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
	if (!bUseObliqueView)
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
	if (ShouldAllowFocusedSurfaceLook())
	{
		const FQuat BaseViewQuat = BaseViewRotation.Quaternion();
		const FQuat SurfaceLookQuat = FQuat(FocusedSurfaceLookOffset);
		SpringArm->SetWorldRotation((BaseViewQuat * SurfaceLookQuat).Rotator().GetNormalized());
		return;
	}

	SpringArm->SetWorldRotation(BaseViewRotation);
}

bool ASRCameraPawn::ShouldAllowFocusedSurfaceLook() const
{
	if (!IsValid(FocusedActor) || !USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(FocusedActor))
	{
		return false;
	}

	return !USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(FocusedActor);
}

void ASRCameraPawn::UpdateFocusedSurfaceLook(float DeltaSeconds)
{
	const FVector2D CombinedLookInput = FocusedSurfaceLookInput.GetClampedToMaxSize(1.0f);
	if (CombinedLookInput.IsNearlyZero() || DeltaSeconds <= UE_SMALL_NUMBER || !ShouldAllowFocusedSurfaceLook())
	{
		bIsFocusedSurfaceLookActive = false;
		return;
	}

	const float SafeLookSpeed = FMath::Max(0.0f, FocusedSurfaceLookSpeed);
	if (SafeLookSpeed <= KINDA_SMALL_NUMBER)
	{
		bIsFocusedSurfaceLookActive = false;
		return;
	}

	const float MaxPitch = FMath::Clamp(FocusedSurfaceLookMaxPitch, 0.0f, 89.0f);
	FocusedSurfaceLookOffset.Yaw += CombinedLookInput.X * SafeLookSpeed * DeltaSeconds;
	FocusedSurfaceLookOffset.Pitch = FMath::Clamp(
		FocusedSurfaceLookOffset.Pitch + (CombinedLookInput.Y * SafeLookSpeed * DeltaSeconds),
		-MaxPitch,
		MaxPitch);
	FocusedSurfaceLookOffset.Roll = 0.0f;
	FocusedSurfaceLookOffset.Normalize();
	bIsFocusedSurfaceLookActive = true;
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
	AActor* DirectionalLightTarget = nullptr;
	if (ApplyCelestialBodyMeshVisibility(DirectionalLightTarget))
	{
		ConfigureDirectionalLight(DirectionalLightTarget);
	}
	else
	{
		ConfigureDirectionalLight(nullptr);
	}
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

	bool bHasStaticMeshBody = false;
	float BestDynamicMeshTargetScreenSizeRatio = 0.0f;
	for (AActor* BodyActor : CelestialBodies)
	{
		ASRCelestialBody* CelestialBody = Cast<ASRCelestialBody>(BodyActor);
		if (!IsValid(CelestialBody))
		{
			continue;
		}

		float ScreenSizeRatio = 0.0f;
		const bool bUseDynamicMesh = ShouldUseDynamicMesh(BodyActor, ScreenSizeRatio);
		CelestialBody->SetDynamicMeshEnabled(bUseDynamicMesh);
		bHasStaticMeshBody |= !bUseDynamicMesh;

		if (!bUseDynamicMesh || USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(BodyActor))
		{
			continue;
		}

		if (BodyActor == FocusedActor.Get())
		{
			OutDirectionalLightTarget = BodyActor;
			BestDynamicMeshTargetScreenSizeRatio = TNumericLimits<float>::Max();
			continue;
		}

		if (!IsValid(OutDirectionalLightTarget) && ScreenSizeRatio > BestDynamicMeshTargetScreenSizeRatio)
		{
			OutDirectionalLightTarget = BodyActor;
			BestDynamicMeshTargetScreenSizeRatio = ScreenSizeRatio;
		}
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

	const float BodyRadius = USRCelestialBodyRuntimeLibrary::GetCelestialApproximateRadius(BodyActor);
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

	OutScreenSizeRatio = FMath::Clamp(BodyRadius / (Depth * TanHalfFieldOfView), 0.0f, BIG_NUMBER);
	const float ThresholdRatio = CelestialBody->GetDynamicMeshThreshold();
	return OutScreenSizeRatio >= ThresholdRatio;
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

FVector ASRCameraPawn::ConvertScreenDeltaToDragOffset(const FVector2D& ScreenDelta) const
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

	const FVector CameraLocation = Camera->GetComponentLocation();
	const FVector PlaneOrigin(FixedPlaneX, 0.0f, 0.0f);
	const FVector CameraForward = Camera->GetForwardVector();
	const float ForwardToPlaneDot = FVector::DotProduct(CameraForward, FVector::ForwardVector);
	if (FMath::Abs(ForwardToPlaneDot) <= UE_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float DistanceToDragPlane = FMath::Abs(FVector::DotProduct(PlaneOrigin - CameraLocation, FVector::ForwardVector) / ForwardToPlaneDot);
	if (DistanceToDragPlane <= UE_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float VerticalFieldOfViewRadians = FMath::DegreesToRadians(Camera->FieldOfView);
	const float HalfPlaneHeight = DistanceToDragPlane * FMath::Tan(VerticalFieldOfViewRadians * 0.5f);
	const float HalfPlaneWidth = HalfPlaneHeight * (static_cast<float>(ViewportWidth) / static_cast<float>(ViewportHeight));
	const float WorldUnitsPerPixelHorizontal = (HalfPlaneWidth * 2.0f) / static_cast<float>(ViewportWidth);
	const float WorldUnitsPerPixelVertical = (HalfPlaneHeight * 2.0f) / static_cast<float>(ViewportHeight);

	FVector DragOffset = (-Camera->GetRightVector() * (ScreenDelta.X * WorldUnitsPerPixelHorizontal))
		+ (Camera->GetUpVector() * (ScreenDelta.Y * WorldUnitsPerPixelVertical));
	DragOffset.X = 0.0f;
	return DragOffset;
}

FVector ASRCameraPawn::GetFocusLocation() const
{
	if (!FocusedActor)
	{
		return FVector(FixedPlaneX, DragTargetLocation.Y, DragTargetLocation.Z);
	}

	const FVector FocusLocation = FocusedActor->GetActorLocation();
	return FVector(FixedPlaneX, FocusLocation.Y, FocusLocation.Z);
}

bool ASRCameraPawn::ShouldRotateFocusedBody() const
{
	if (!IsValid(FocusedActor) || !USRCelestialBodyRuntimeLibrary::GetCelestialCanConstruct(FocusedActor))
	{
		return false;
	}

	if (!USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(FocusedActor))
	{
		return false;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return false;
	}

	FHitResult HitResult;
	return PlayerController->GetHitResultUnderCursor(ECC_Visibility, false, HitResult)
		&& HitResult.GetActor() == FocusedActor;
}

void ASRCameraPawn::HandleFocusedBodyRotation(const FVector2D& DragDelta)
{
	if (!IsValid(FocusedActor))
	{
		return;
	}

	const FVector RotationUpAxis = Camera ? Camera->GetUpVector() : FVector::UpVector;
	const FVector RotationRightAxis = Camera ? Camera->GetRightVector() : FVector::RightVector;
	const float HorizontalDegrees = -DragDelta.X * SurfaceRotateSensitivity;
	const float VerticalDegrees = DragDelta.Y * SurfaceRotateSensitivity;
	const FQuat HorizontalRotation(RotationUpAxis, FMath::DegreesToRadians(HorizontalDegrees));
	const FQuat VerticalRotation(RotationRightAxis, FMath::DegreesToRadians(VerticalDegrees));

	FocusedActor->AddActorWorldRotation(VerticalRotation * HorizontalRotation, false, nullptr, ETeleportType::TeleportPhysics);
}
