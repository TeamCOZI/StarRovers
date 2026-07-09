#include "Gravity/SRGravityParent.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Rendering/SRCelestialRingMeshComponent.h"

namespace
{
	const FName SRGravityParentLineTag(TEXT("StarRovers.GravityLine"));
	const FName SRGravityParentLineRootTag(TEXT("StarRovers.GravityLineRoot"));
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
	ShowGravityLine = true;
	GravityLineColor = FLinearColor(0.45f, 1.0f, 0.45f, 1.0f);
	GravityLineOpacity = 0.85f;
	GravityLineSegments = 96;
	GravityLineThickness = 20.0f;

	RecomputeDerivedValues();
}

void USRGravityParent::OnRegister()
{
	Super::OnRegister();

	EnsureGravityRingVisual();
	RecomputeDerivedValues();
	RegisteredSources.AddUnique(this);
}

void USRGravityParent::OnUnregister()
{
	ReleaseGravityRingVisual();
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

void USRGravityParent::ConfigureGravity(
	float NewMass,
	float NewGravityRatio,
	float NewGravityRadiusRatio,
	bool bNewShowGravityLine,
	const FLinearColor& NewGravityLineColor,
	float NewGravityLineOpacity,
	int32 NewGravityLineSegments,
	float NewGravityLineThickness)
{
	Mass = NewMass;
	GravityRatio = NewGravityRatio;
	GravityRadiusRatio = NewGravityRadiusRatio;
	ShowGravityLine = bNewShowGravityLine;
	GravityLineColor = NewGravityLineColor;
	GravityLineOpacity = NewGravityLineOpacity;
	GravityLineSegments = NewGravityLineSegments;
	GravityLineThickness = NewGravityLineThickness;
	RecomputeDerivedValues();
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
	return ShowGravityLine && GravityRadius > KINDA_SMALL_NUMBER;
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
	EnsureGravityRingVisual();
	if (!IsValid(GravityRingVisual))
	{
		const AActor* Owner = GetOwner();
		if (IsValid(Owner) && Owner->HasActorBegunPlay() && ShouldShowGravityLine())
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("USRGravityParent cannot draw gravity line for owner '%s': GravityRingVisual is null after lookup, ShowGravityLine=true, GravityRadius=%.2f, and no registered USRCelestialRingMeshComponent named 'GravityRingVisual' was available."),
				*Owner->GetName(),
				GravityRadius);
		}
		SetComponentTickEnabled(false);
		return;
	}

	if (!ShouldShowGravityLine() || !IsValid(GetOwner()))
	{
		GravityRingVisual->ClearRingVisual();
		SetComponentTickEnabled(false);
		return;
	}

	GravityRingVisual->UpdateRingVisual(
		GetOwner()->GetActorLocation(),
		GravityRadius,
		FLinearColor(GravityLineColor.R, GravityLineColor.G, GravityLineColor.B, GetGravityLineOpacity()),
		GetGravityLineThickness(),
		GravityLineSegments);

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

void USRGravityParent::EnsureGravityRingVisual()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || IsValid(GravityRingVisual))
	{
		return;
	}

	TInlineComponentArray<USRCelestialRingMeshComponent*> RingComponents(Owner);
	Owner->GetComponents(RingComponents);
	for (USRCelestialRingMeshComponent* RingComponent : RingComponents)
	{
		if (IsValid(RingComponent) && RingComponent->GetFName() == TEXT("GravityRingVisual"))
		{
			GravityRingVisual = RingComponent;
			break;
		}
	}

	if (IsValid(GravityRingVisual))
	{
		GravityRingVisual->SetMobility(EComponentMobility::Movable);
		GravityRingVisual->SetUsingAbsoluteLocation(true);
		GravityRingVisual->SetUsingAbsoluteRotation(true);
		GravityRingVisual->SetUsingAbsoluteScale(true);
		GravityRingVisual->ComponentTags.AddUnique(SRGravityParentLineTag);
		GravityRingVisual->ComponentTags.AddUnique(SRGravityParentLineRootTag);
		return;
	}

	return;
}

void USRGravityParent::ReleaseGravityRingVisual()
{
	if (!IsValid(GravityRingVisual))
	{
		return;
	}

	GravityRingVisual->ClearRingVisual();
	GravityRingVisual = nullptr;
}
