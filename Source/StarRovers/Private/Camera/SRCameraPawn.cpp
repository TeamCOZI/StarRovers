#include "Camera/SRCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Components/SceneComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Gravity/SRGravityParent.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

namespace StarRoversInputPaths
{
	static constexpr TCHAR DefaultMappingContext[] = TEXT("/Game/BlueprintClasses/Core/IMC_SR.IMC_SR");
	static constexpr TCHAR DragHoldAction[] = TEXT("/Game/BlueprintClasses/Core/IA_DragHold.IA_DragHold");
	static constexpr TCHAR DragDeltaAction[] = TEXT("/Game/BlueprintClasses/Core/IA_DragDelta.IA_DragDelta");
	static constexpr TCHAR ZoomAction[] = TEXT("/Game/BlueprintClasses/Core/IA_Zoom.IA_Zoom");
}

namespace
{
	constexpr float DefaultCameraFieldOfView = 30.0f;
	constexpr float DefaultDragInterpSpeed = 10.0f;
	constexpr float DefaultZoomInterpSpeed = 8.0f;
	constexpr float DefaultMinimumZoomDistance = 1.0f;
	constexpr float DefaultCelestialBodyCollisionPadding = 1.0f;
	constexpr float DefaultFocusZoomMultiplier = 3.0f;

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
	SpringArm->TargetArmLength = 12000.0f;
	SpringArm->bDoCollisionTest = false;
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritYaw = false;
	SpringArm->bInheritRoll = false;
	SpringArm->SetUsingAbsoluteRotation(true);
	SpringArm->SetRelativeRotation(FRotator::ZeroRotator);

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
	DragTargetLocation = FVector::ZeroVector;
	ZoomDistanceTarget = SpringArm->TargetArmLength;
	bIsDragging = false;
	bMappingContextApplied = false;
	bIsRotatingFocusedBody = false;
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
	ZoomDistanceTarget = ClampZoomDistanceTarget(SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	ApplyZoomDrivenViewRotation(SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	RefreshScreenSpaceThicknessReferenceView();
	FocusTrackingDelta = FVector::ZeroVector;
	FocusTrackingDeltaVelocity = FVector::ZeroVector;

	ApplyMappingContext();
	ApplyZoomDrivenViewRotation(SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
}

void ASRCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ApplyMappingContext();

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

	NewLocation.X = FixedPlaneX;
	SetActorLocation(NewLocation);

	if (SpringArm)
	{
		const float MinimumSafeZoomDistance = GetMinimumSafeZoomDistance();
		const float MaximumAllowedZoomDistance = GetMaxZoomDistanceForMinimumSafeDistance(MinimumSafeZoomDistance);
		ZoomDistanceTarget = FMath::Clamp(ZoomDistanceTarget, MinimumSafeZoomDistance, MaximumAllowedZoomDistance);

		const float InterpolatedZoom = FMath::FInterpTo(SpringArm->TargetArmLength, ZoomDistanceTarget, DeltaSeconds, DefaultZoomInterpSpeed);
		SpringArm->TargetArmLength = FMath::Clamp(InterpolatedZoom, MinimumSafeZoomDistance, MaximumAllowedZoomDistance);
		ApplyZoomDrivenViewRotation(SpringArm->TargetArmLength);
	}
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
	}
}

void ASRCameraPawn::FocusActor(AActor* NewFocusActor)
{
	AActor* PreviousFocusedActor = FocusedActor.Get();
	FocusedActor = NewFocusActor;
	FocusDragOffset = FVector::ZeroVector;

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
		ZoomDistanceTarget = ClampZoomDistanceTarget(DesiredFocusZoom);
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
	BroadcastFocusedActorChangedIfNeeded(PreviousFocusedActor);
}

AActor* ASRCameraPawn::GetFocusedActor() const
{
	return FocusedActor;
}

float ASRCameraPawn::GetMaxZoomDistance() const
{
	return FMath::Max(GetMinimumSafeZoomDistance(), GetMaxZoomDistanceForMinimumSafeDistance(GetMinimumSafeZoomDistance()));
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

	const FVector DesiredLocation(FixedPlaneX, DragTargetLocation.Y, DragTargetLocation.Z);
	SetActorLocation(DesiredLocation);

	if (SpringArm)
	{
		ZoomDistanceTarget = ClampZoomDistanceTarget(ZoomDistanceTarget);
		SpringArm->TargetArmLength = ZoomDistanceTarget;
		ApplyZoomDrivenViewRotation(SpringArm->TargetArmLength);
	}
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

	ZoomDistanceTarget = ClampZoomDistanceTarget(ZoomDistanceTarget - (AxisValue * GetZoomSpeed()));
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

float ASRCameraPawn::GetMinimumSafeZoomDistance() const
{
	if (!IsValid(FocusedActor))
	{
		return DefaultMinimumZoomDistance;
	}

	const float FocusedBodyRadius = USRCelestialBodyRuntimeLibrary::GetCelestialApproximateRadius(FocusedActor);
	if (FocusedBodyRadius <= KINDA_SMALL_NUMBER)
	{
		return DefaultMinimumZoomDistance;
	}

	return FMath::Max(DefaultMinimumZoomDistance, FocusedBodyRadius + DefaultCelestialBodyCollisionPadding);
}

float ASRCameraPawn::GetMaxZoomDistanceForMinimumSafeDistance(float MinimumSafeZoomDistance) const
{
	const float SpaceSphereRadius = GetSpaceSphereRadius();
	if (SpaceSphereRadius > KINDA_SMALL_NUMBER)
	{
		return FMath::Max(MinimumSafeZoomDistance, SpaceSphereRadius);
	}

	return BIG_NUMBER;
}

float ASRCameraPawn::GetSpaceSphereRadius() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	float LargestRadius = 0.0f;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const AActor* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == this)
		{
			continue;
		}

		const FString CandidateName = Candidate->GetName();
		const FString CandidateClassName = Candidate->GetClass() ? Candidate->GetClass()->GetName() : FString();
		const bool bLooksLikeSpaceSphere =
			CandidateName.Contains(TEXT("SpaceSphere"), ESearchCase::IgnoreCase)
			|| CandidateName.Contains(TEXT("SpaceSkySphere"), ESearchCase::IgnoreCase)
			|| CandidateName.Contains(TEXT("Space Sky Sphere"), ESearchCase::IgnoreCase)
			|| CandidateName.Equals(TEXT("BP_Space"), ESearchCase::IgnoreCase)
			|| CandidateName.StartsWith(TEXT("BP_Space_"), ESearchCase::IgnoreCase)
			|| CandidateClassName.Contains(TEXT("SpaceSphere"), ESearchCase::IgnoreCase)
			|| CandidateClassName.Contains(TEXT("SpaceSkySphere"), ESearchCase::IgnoreCase)
			|| CandidateClassName.Equals(TEXT("BP_Space_C"), ESearchCase::IgnoreCase);
		if (!bLooksLikeSpaceSphere)
		{
			continue;
		}

		LargestRadius = FMath::Max(LargestRadius, Candidate->GetActorScale3D().GetAbsMax() * 49.0f);
	}

	return LargestRadius;
}

float ASRCameraPawn::GetObliqueViewBlendAlpha(float ZoomDistance) const
{
	if (!bUseObliqueView)
	{
		return 0.0f;
	}

	const float MinimumSafeZoomDistance = GetMinimumSafeZoomDistance();
	const float MaximumZoomDistance = FMath::Max(MinimumSafeZoomDistance, GetMaxZoomDistanceForMinimumSafeDistance(MinimumSafeZoomDistance));
	const float StartRatio = FMath::Clamp(ObliqueViewStart, 0.0f, 1.0f);
	const float EndRatio = FMath::Clamp(ObliqueViewEnd, StartRatio + UE_SMALL_NUMBER, 1.0f);
	const float StartZoomDistance = FMath::Lerp(MinimumSafeZoomDistance, MaximumZoomDistance, StartRatio);
	const float EndZoomDistance = FMath::Lerp(MinimumSafeZoomDistance, MaximumZoomDistance, EndRatio);
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
	SpringArm->SetWorldRotation(GetViewRotationForZoom(ZoomDistance));
}

float ASRCameraPawn::ClampZoomDistanceTarget(float ProposedZoomDistanceTarget) const
{
	const float MinimumSafeZoomDistance = GetMinimumSafeZoomDistance();
	const float MaximumZoomDistance = GetMaxZoomDistanceForMinimumSafeDistance(MinimumSafeZoomDistance);
	return FMath::Clamp(ProposedZoomDistanceTarget, MinimumSafeZoomDistance, MaximumZoomDistance);
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
