#include "Gravity/SRGravityChild.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Gravity/SRGravityParent.h"

USRGravityChild::USRGravityChild()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	bGravityEnabled = true;
	bUsePhysicsIfAvailable = true;
	bUseStrongestSourceOnly = false;
	ManualLinearDamping = 0.0f;
	LinearVelocity = FVector::ZeroVector;
	CurrentGravityAcceleration = FVector::ZeroVector;
}

void USRGravityChild::BeginPlay()
{
	Super::BeginPlay();

	UpdateTickState();
}

void USRGravityChild::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	CurrentGravityAcceleration = FVector::ZeroVector;
	CurrentPrimaryGravityActor = nullptr;

	if (!bGravityEnabled || DeltaTime <= 0.0f || !GetOwner() || !GetWorld())
	{
		return;
	}

	TArray<USRGravityParent*> GravitySources;
	USRGravityParent::GetRegisteredSourcesForWorld(GetWorld(), GravitySources);

	float StrongestAccelerationSizeSquared = -1.0f;
	FVector SummedAcceleration = FVector::ZeroVector;

	for (USRGravityParent* GravitySource : GravitySources)
	{
		if (!IsValid(GravitySource) || GravitySource->GetOwner() == GetOwner())
		{
			continue;
		}

		const FVector SourceAcceleration = GravitySource->GetGravityAccelerationAtWorldLocation(GetOwner()->GetActorLocation());
		const float SourceAccelerationSizeSquared = SourceAcceleration.SizeSquared();
		if (SourceAccelerationSizeSquared <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		if (SourceAccelerationSizeSquared > StrongestAccelerationSizeSquared)
		{
			StrongestAccelerationSizeSquared = SourceAccelerationSizeSquared;
			CurrentPrimaryGravityActor = GravitySource->GetOwner();
		}

		if (bUseStrongestSourceOnly)
		{
			if (SourceAccelerationSizeSquared >= CurrentGravityAcceleration.SizeSquared())
			{
				CurrentGravityAcceleration = SourceAcceleration;
			}
		}
		else
		{
			SummedAcceleration += SourceAcceleration;
		}
	}

	if (!bUseStrongestSourceOnly)
	{
		CurrentGravityAcceleration = SummedAcceleration;
	}

	if (CurrentGravityAcceleration.IsNearlyZero())
	{
		return;
	}

	if (bUsePhysicsIfAvailable)
	{
		if (UPrimitiveComponent* SimulatingPrimitive = FindSimulatingPrimitive())
		{
			SimulatingPrimitive->AddForce(CurrentGravityAcceleration, NAME_None, true);
			return;
		}
	}

	LinearVelocity += CurrentGravityAcceleration * DeltaTime;
	if (ManualLinearDamping > 0.0f)
	{
		LinearVelocity *= FMath::Max(0.0f, 1.0f - (ManualLinearDamping * DeltaTime));
	}

	GetOwner()->AddActorWorldOffset(LinearVelocity * DeltaTime, true);
}

void USRGravityChild::SetGravityEnabled(bool bEnabled)
{
	bGravityEnabled = bEnabled;
	UpdateTickState();
}

bool USRGravityChild::IsGravityEnabled() const
{
	return bGravityEnabled;
}

void USRGravityChild::ResetVelocity()
{
	LinearVelocity = FVector::ZeroVector;
}

FVector USRGravityChild::GetLinearVelocity() const
{
	return LinearVelocity;
}

FVector USRGravityChild::GetCurrentGravityAcceleration() const
{
	return CurrentGravityAcceleration;
}

AActor* USRGravityChild::GetCurrentPrimaryGravityActor() const
{
	return CurrentPrimaryGravityActor.Get();
}

void USRGravityChild::UpdateTickState()
{
	SetComponentTickEnabled(bGravityEnabled);
}

UPrimitiveComponent* USRGravityChild::FindSimulatingPrimitive() const
{
	if (!GetOwner())
	{
		return nullptr;
	}

	UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
	return RootPrimitive && RootPrimitive->IsSimulatingPhysics()
		? RootPrimitive
		: nullptr;
}
