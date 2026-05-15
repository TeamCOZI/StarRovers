#include "Gravity/SRGravityParent.h"

#include "Components/LineBatchComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Visual/SRLineThicknessUtils.h"

namespace
{
	const FName SRGravityParentLineTag(TEXT("StarRovers.GravityLine"));
	const FName SRGravityParentLineRootTag(TEXT("StarRovers.GravityLineRoot"));
	constexpr uint8 GravityLineDepthPriority = SDPG_World;

	void AppendGravityCircleLines(
		ULineBatchComponent* LineBatcher,
		const FVector& Center,
		const float Radius,
		const int32 SegmentCount,
		const FLinearColor& Color,
		const float BaseThickness,
		const FSRCameraInfo& CameraInfo,
		const float ReferenceViewDepth,
		const float ReferenceFieldOfViewDegrees)
	{
		if (!IsValid(LineBatcher) || Radius <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const int32 SafeSegmentCount = FMath::Max(3, SegmentCount);
		const FColor SegmentColor = Color.ToFColor(true);
		FVector PreviousPoint = Center + FVector(0.0f, Radius, 0.0f);

		for (int32 SegmentIndex = 1; SegmentIndex <= SafeSegmentCount; ++SegmentIndex)
		{
			const float Alpha = static_cast<float>(SegmentIndex) / static_cast<float>(SafeSegmentCount);
			const float AngleRadians = Alpha * 2.0f * PI;
			const FVector CurrentPoint = Center + FVector(
				0.0f,
				FMath::Cos(AngleRadians) * Radius,
				FMath::Sin(AngleRadians) * Radius);
			const FVector SegmentMidpoint = (PreviousPoint + CurrentPoint) * 0.5f;
			const float Thickness = FSRLineThicknessUtils::ComputeWorldThicknessAtLocation(
				CameraInfo,
				SegmentMidpoint,
				BaseThickness,
				ReferenceViewDepth,
				ReferenceFieldOfViewDegrees);

			LineBatcher->DrawLine(
				PreviousPoint,
				CurrentPoint,
				SegmentColor,
				GravityLineDepthPriority,
				FMath::Max(0.0f, Thickness),
				0.0f);

			PreviousPoint = CurrentPoint;
		}
	}

}

TArray<TWeakObjectPtr<USRGravityParent>> USRGravityParent::RegisteredSources;

USRGravityParent::USRGravityParent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	Mass = 1000.0f;
	GravityRatio = 1.0f;
	GravityRadiusRatio = 10.0f;
	Gravity = 0.0f;
	GravityRadius = 0.0f;
	bShowGravityLine = true;
	GravityLineColor = FLinearColor(0.45f, 1.0f, 0.45f, 1.0f);
	GravityLineOpacity = 0.85f;
	GravityLineSegments = 96;
	GravityLineThickness = 20.0f;

	RecomputeDerivedValues();
}

void USRGravityParent::OnRegister()
{
	Super::OnRegister();

	EnsureGravityLineBatch();
	RecomputeDerivedValues();
	RegisteredSources.AddUnique(this);
}

void USRGravityParent::OnUnregister()
{
	ReleaseGravityLineBatch();
	RegisteredSources.RemoveSingleSwap(this);

	Super::OnUnregister();
}

void USRGravityParent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ShouldShowGravityLine())
	{
		RefreshGravityLine();
	}
}

#if WITH_EDITOR
void USRGravityParent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	RecomputeDerivedValues();
}
#endif

void USRGravityParent::RecomputeDerivedValues()
{
	Mass = FMath::Max(0.0f, Mass);
	GravityRatio = FMath::Max(0.0f, GravityRatio);
	GravityRadiusRatio = FMath::Max(0.0f, GravityRadiusRatio);
	GravityLineOpacity = FMath::Clamp(GravityLineOpacity, 0.0f, 1.0f);
	GravityLineSegments = FMath::Max(3, GravityLineSegments);
	GravityLineThickness = FMath::Max(0.0f, GravityLineThickness);
	Gravity = Mass * GravityRatio;
	GravityRadius = Mass * GravityRadiusRatio;
	RefreshGravityLine();
}

float USRGravityParent::GetMass() const
{
	return Mass;
}

float USRGravityParent::GetGravityStrength() const
{
	return Gravity;
}

float USRGravityParent::GetGravityRadius() const
{
	return GravityRadius;
}

FVector USRGravityParent::GetGravityAccelerationAtWorldLocation(const FVector& WorldLocation) const
{
	if (!GetOwner())
	{
		return FVector::ZeroVector;
	}

	const FVector SourceLocation = GetOwner()->GetActorLocation();
	const FVector ToSource = SourceLocation - WorldLocation;
	const float DistanceSquared = ToSource.SizeSquared();
	if (DistanceSquared <= KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float Distance = FMath::Sqrt(DistanceSquared);
	if (GravityRadius > 0.0f && Distance > GravityRadius)
	{
		return FVector::ZeroVector;
	}

	const float AccelerationMagnitude = Gravity / FMath::Max(DistanceSquared, 1.0f);
	return ToSource / Distance * AccelerationMagnitude;
}

bool USRGravityParent::ShouldShowGravityLine() const
{
	return bShowGravityLine && GravityRadius > KINDA_SMALL_NUMBER;
}

FLinearColor USRGravityParent::GetGravityLineColor() const
{
	return GravityLineColor;
}

float USRGravityParent::GetGravityLineOpacity() const
{
	return FMath::Clamp(GravityLineOpacity, 0.0f, 1.0f);
}

int32 USRGravityParent::GetGravityLineSegments() const
{
	return FMath::Max(3, GravityLineSegments);
}

float USRGravityParent::GetGravityLineThickness() const
{
	return FMath::Max(0.0f, GravityLineThickness);
}

void USRGravityParent::RefreshGravityLine()
{
	EnsureGravityLineBatch();
	if (!IsValid(GravityLineBatch))
	{
		const AActor* Owner = GetOwner();
		if (IsValid(Owner) && Owner->HasActorBegunPlay() && ShouldShowGravityLine())
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("USRGravityParent cannot draw gravity line for owner '%s': GravityLineBatch is null after lookup, bShowGravityLine=true, GravityRadius=%.2f, and no registered ULineBatchComponent named 'Gravity Line Batch' was available."),
				*Owner->GetName(),
				GravityRadius);
		}
		SetComponentTickEnabled(false);
		return;
	}

	GravityLineBatch->Flush();

	if (!ShouldShowGravityLine() || !IsValid(GetOwner()))
	{
		SetComponentTickEnabled(false);
		return;
	}

	FSRCameraInfo CameraInfo;
	FSRLineThicknessUtils::TryBuildPrimaryCameraInfo(GetWorld(), CameraInfo);

	float ReferenceViewDepth = FSRLineThicknessUtils::DefaultReferenceViewDepth;
	float ReferenceFieldOfViewDegrees = FSRLineThicknessUtils::DefaultReferenceFieldOfViewDegrees;
	FSRLineThicknessUtils::ResolveReferenceView(GetWorld(), ReferenceViewDepth, ReferenceFieldOfViewDegrees);

	AppendGravityCircleLines(
		GravityLineBatch,
		GetOwner()->GetActorLocation(),
		GravityRadius,
		GravityLineSegments,
		FLinearColor(GravityLineColor.R, GravityLineColor.G, GravityLineColor.B, GetGravityLineOpacity()),
		GetGravityLineThickness(),
		CameraInfo,
		ReferenceViewDepth,
		ReferenceFieldOfViewDegrees);

	SetComponentTickEnabled(true);
}

void USRGravityParent::GetRegisteredSourcesForWorld(const UWorld* World, TArray<USRGravityParent*>& OutSources)
{
	OutSources.Reset();

	for (int32 Index = RegisteredSources.Num() - 1; Index >= 0; --Index)
	{
		USRGravityParent* SourceComponent = RegisteredSources[Index].Get();
		if (!IsValid(SourceComponent))
		{
			RegisteredSources.RemoveAtSwap(Index);
			continue;
		}

		if (World && SourceComponent->GetWorld() != World)
		{
			continue;
		}

		OutSources.Add(SourceComponent);
	}
}

void USRGravityParent::EnsureGravityLineBatch()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || IsValid(GravityLineBatch))
	{
		return;
	}

	TInlineComponentArray<ULineBatchComponent*> LineBatchComponents(Owner);
	Owner->GetComponents(LineBatchComponents);
	for (ULineBatchComponent* LineBatchComponent : LineBatchComponents)
	{
		if (IsValid(LineBatchComponent) && LineBatchComponent->GetFName() == TEXT("Gravity Line Batch"))
		{
			GravityLineBatch = LineBatchComponent;
			break;
		}
	}

	if (IsValid(GravityLineBatch))
	{
		GravityLineBatch->SetMobility(EComponentMobility::Movable);
		GravityLineBatch->SetUsingAbsoluteLocation(true);
		GravityLineBatch->SetUsingAbsoluteRotation(true);
		GravityLineBatch->SetUsingAbsoluteScale(true);
		GravityLineBatch->ComponentTags.AddUnique(SRGravityParentLineTag);
		GravityLineBatch->ComponentTags.AddUnique(SRGravityParentLineRootTag);
		return;
	}

	return;
}

void USRGravityParent::ReleaseGravityLineBatch()
{
	if (!IsValid(GravityLineBatch))
	{
		return;
	}

	GravityLineBatch->Flush();
	GravityLineBatch = nullptr;
}
