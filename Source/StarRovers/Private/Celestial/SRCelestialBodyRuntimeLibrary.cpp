#include "Celestial/SRCelestialBodyRuntimeLibrary.h"

#include "Celestial/SRCelestialBody.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Simulation/SROrbit.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "UObject/UnrealType.h"

namespace CelestialBodyPropertyNames
{
	static const FName DisplayName(TEXT("DisplayName"));
	static const FName ParentBody(TEXT("ParentBody"));
	static const FName OrbitRadius(TEXT("OrbitRadius"));
	static const FName OrbitPeriod(TEXT("OrbitPeriod"));
	static const FName StartingPhase(TEXT("StartingPhase"));
	static const FName FocusZoomMultiplier(TEXT("FocusZoomMultiplier"));
	static const FName ApproximateRadius(TEXT("ApproximateRadius"));
	static const FName CanConstruct(TEXT("CanConstruct"));
}

namespace CelestialBodyComponentTags
{
	static const FName GravityLine(TEXT("StarRovers.GravityLine"));
	static const FName GravityLineRoot(TEXT("StarRovers.GravityLineRoot"));
	static const FName GravityLineSegment(TEXT("StarRovers.GravityLineSegment"));
}

namespace
{
	FText MakeEnumValueText(const UEnum* Enum, const int64 Value)
	{
		if (!Enum)
		{
			return FText::AsNumber(Value);
		}

		FText DisplayText = Enum->GetDisplayNameTextByValue(Value);
		if (DisplayText.IsEmpty())
		{
			DisplayText = FText::FromString(Enum->GetNameStringByValue(Value));
		}

		return DisplayText;
	}

	FProperty* FindRuntimeLibraryPropertyInClassHierarchy(const UClass* InClass, const FName PropertyName)
	{
		for (const UStruct* Struct = InClass; Struct; Struct = Struct->GetSuperStruct())
		{
			if (FProperty* Property = Struct->FindPropertyByName(PropertyName))
			{
				return Property;
			}
		}

		return nullptr;
	}

	bool IsLikelyCelestialClass(const AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return false;
		}

		const FString ClassPath = Actor->GetClass()->GetPathName();
		return ClassPath.Contains(TEXT("/Game/BlueprintClasses/Celestial/"));
	}

	bool IsLikelyStarClass(const AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return false;
		}

		const FString ClassPath = Actor->GetClass()->GetPathName();
		return ClassPath.Contains(TEXT("/Game/BlueprintClasses/Celestial/BP_Star."));
	}

	bool TryGetFloatPropertyValue(const AActor* Actor, const FName PropertyName, float& OutValue)
	{
		if (!IsValid(Actor))
		{
			return false;
		}

		FProperty* Property = FindRuntimeLibraryPropertyInClassHierarchy(Actor->GetClass(), PropertyName);
		if (!Property)
		{
			return false;
		}

		if (const FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
		{
			OutValue = FloatProperty->GetPropertyValue_InContainer(Actor);
			return true;
		}

		if (const FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
		{
			OutValue = static_cast<float>(DoubleProperty->GetPropertyValue_InContainer(Actor));
			return true;
		}

		if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
		{
			OutValue = NumericProperty->IsFloatingPoint()
				? static_cast<float>(NumericProperty->GetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Actor)))
				: static_cast<float>(NumericProperty->GetSignedIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Actor)));
			return true;
		}

		return false;
	}

	bool TryGetBoolPropertyValue(const AActor* Actor, const FName PropertyName, bool& OutValue)
	{
		OutValue = false;

		if (!IsValid(Actor))
		{
			return false;
		}

		FProperty* Property = FindRuntimeLibraryPropertyInClassHierarchy(Actor->GetClass(), PropertyName);
		const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property);
		if (!BoolProperty)
		{
			return false;
		}

		OutValue = BoolProperty->GetPropertyValue_InContainer(Actor);
		return true;
	}

	bool TryGetActorPropertyValue(const AActor* Actor, const FName PropertyName, AActor*& OutActor)
	{
		OutActor = nullptr;

		if (!IsValid(Actor))
		{
			return false;
		}

		FProperty* Property = FindRuntimeLibraryPropertyInClassHierarchy(Actor->GetClass(), PropertyName);
		if (!Property)
		{
			return false;
		}

		const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property);
		if (!ObjectProperty)
		{
			return false;
		}

		OutActor = Cast<AActor>(ObjectProperty->GetObjectPropertyValue_InContainer(Actor));
		return true;
	}

	bool TryGetTextLikePropertyValue(const AActor* Actor, const FName PropertyName, FText& OutValue)
	{
		if (!IsValid(Actor))
		{
			return false;
		}

		FProperty* Property = FindRuntimeLibraryPropertyInClassHierarchy(Actor->GetClass(), PropertyName);
		if (!Property)
		{
			return false;
		}

		if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))
		{
			OutValue = TextProperty->GetPropertyValue_InContainer(Actor);
			return true;
		}

		if (const FStrProperty* StringProperty = CastField<FStrProperty>(Property))
		{
			OutValue = FText::FromString(StringProperty->GetPropertyValue_InContainer(Actor));
			return true;
		}

		if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
		{
			OutValue = FText::FromName(NameProperty->GetPropertyValue_InContainer(Actor));
			return true;
		}

		if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			if (const UObject* ObjectValue = ObjectProperty->GetObjectPropertyValue_InContainer(Actor))
			{
				OutValue = FText::FromString(ObjectValue->GetName());
				return true;
			}
		}

		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Actor);
			const int64 Value = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
			OutValue = MakeEnumValueText(EnumProperty->GetEnum(), Value);
			return true;
		}

		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			const uint8 Value = ByteProperty->GetPropertyValue_InContainer(Actor);
			OutValue = ByteProperty->Enum
				? MakeEnumValueText(ByteProperty->Enum, Value)
				: FText::AsNumber(Value);
			return true;
		}

		if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
		{
			const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Actor);
			if (NumericProperty->IsFloatingPoint())
			{
				OutValue = FText::AsNumber(NumericProperty->GetFloatingPointPropertyValue(ValuePtr));
				return true;
			}

			if (NumericProperty->IsInteger())
			{
				OutValue = FText::AsNumber(NumericProperty->GetSignedIntPropertyValue(ValuePtr));
				return true;
			}
		}

		return false;
	}

	void LogMissingCelestialData(const AActor* Actor, const TCHAR* FieldName)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Celestial body actor '%s' is missing required %s data."),
			IsValid(Actor) ? *Actor->GetName() : TEXT("<InvalidActor>"),
			FieldName ? FieldName : TEXT("<UnknownField>"));
	}

	bool IsExcludedCelestialVisualComponent(const UPrimitiveComponent* PrimitiveComponent)
	{
		if (!IsValid(PrimitiveComponent))
		{
			return true;
		}

		return PrimitiveComponent->ComponentHasTag(CelestialBodyComponentTags::GravityLine)
			|| PrimitiveComponent->ComponentHasTag(CelestialBodyComponentTags::GravityLineRoot)
			|| PrimitiveComponent->ComponentHasTag(CelestialBodyComponentTags::GravityLineSegment);
	}

	float GetLargestVisualSphereRadius(const AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return 0.0f;
		}

		float LargestRadius = 0.0f;

		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Actor);
		Actor->GetComponents(PrimitiveComponents);
		for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (!IsValid(PrimitiveComponent) || IsExcludedCelestialVisualComponent(PrimitiveComponent))
			{
				continue;
			}

			if (!PrimitiveComponent->IsVisible())
			{
				continue;
			}

			LargestRadius = FMath::Max(LargestRadius, PrimitiveComponent->Bounds.SphereRadius);
		}

		if (LargestRadius > KINDA_SMALL_NUMBER)
		{
			return LargestRadius;
		}

		if (const UStaticMeshComponent* RootStaticMeshComponent = Cast<UStaticMeshComponent>(Actor->GetRootComponent()))
		{
			if (!IsExcludedCelestialVisualComponent(RootStaticMeshComponent)
				&& IsValid(RootStaticMeshComponent->GetStaticMesh())
				&& RootStaticMeshComponent->Bounds.SphereRadius > KINDA_SMALL_NUMBER)
			{
				return RootStaticMeshComponent->Bounds.SphereRadius;
			}
		}

		return 0.0f;
	}

	float GetAuthoredCelestialRadius(const AActor* Actor)
	{
		if (const ASRCelestialBody* ProceduralBody = Cast<ASRCelestialBody>(Actor))
		{
			const FSRCelestialBodySpec BodySpec = ProceduralBody->GetSpec();
			if (BodySpec.ApproximateRadius > KINDA_SMALL_NUMBER)
			{
				return BodySpec.ApproximateRadius;
			}
		}

		float AuthoredApproximateRadius = 0.0f;
		if (TryGetFloatPropertyValue(Actor, CelestialBodyPropertyNames::ApproximateRadius, AuthoredApproximateRadius)
			&& AuthoredApproximateRadius > KINDA_SMALL_NUMBER)
		{
			return AuthoredApproximateRadius;
		}

		return 0.0f;
	}
}

bool USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(const AActor* Actor)
{
	if (Cast<ASRCelestialBody>(Actor))
	{
		return true;
	}

	float UnusedFloat = 0.0f;
	AActor* UnusedParent = nullptr;

	return IsLikelyCelestialClass(Actor)
		|| (TryGetActorPropertyValue(Actor, CelestialBodyPropertyNames::ParentBody, UnusedParent)
		&& TryGetFloatPropertyValue(Actor, CelestialBodyPropertyNames::OrbitRadius, UnusedFloat)
		&& TryGetFloatPropertyValue(Actor, CelestialBodyPropertyNames::OrbitPeriod, UnusedFloat)
		&& TryGetFloatPropertyValue(Actor, CelestialBodyPropertyNames::StartingPhase, UnusedFloat)
		&& TryGetFloatPropertyValue(Actor, CelestialBodyPropertyNames::FocusZoomMultiplier, UnusedFloat));
}

bool USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(const AActor* Actor)
{
	if (!IsCelestialBodyActor(Actor))
	{
		return false;
	}

	if (const ASRCelestialBody* ProceduralBody = Cast<ASRCelestialBody>(Actor))
	{
		return ProceduralBody->GetBodyCategory() == ESRCelestialBodyCategory::Star;
	}

	if (IsLikelyStarClass(Actor))
	{
		return true;
	}

	return false;
}

bool USRCelestialBodyRuntimeLibrary::GetCelestialParentBody(const AActor* Actor, AActor*& OutParentBody)
{
	if (const ASRCelestialBody* ProceduralBody = Cast<ASRCelestialBody>(Actor))
	{
		if (const USROrbit* OrbitComponent = ProceduralBody->GetOrbitComponent())
		{
			OutParentBody = OrbitComponent->GetParentBody();
			return true;
		}
	}

	return TryGetActorPropertyValue(Actor, CelestialBodyPropertyNames::ParentBody, OutParentBody);
}

bool USRCelestialBodyRuntimeLibrary::GetCelestialRootBody(const AActor* Actor, AActor*& OutRootBody)
{
	OutRootBody = nullptr;

	if (!IsCelestialBodyActor(Actor))
	{
		return false;
	}

	constexpr int32 MaxHierarchyDepth = 32;
	AActor* CurrentBody = const_cast<AActor*>(Actor);
	for (int32 DepthIndex = 0; DepthIndex < MaxHierarchyDepth; ++DepthIndex)
	{
		AActor* ParentBody = nullptr;
		if (!GetCelestialParentBody(CurrentBody, ParentBody) || !IsValid(ParentBody))
		{
			OutRootBody = CurrentBody;
			return IsValid(OutRootBody);
		}

		if (ParentBody == CurrentBody)
		{
			break;
		}

		CurrentBody = ParentBody;
	}

	OutRootBody = CurrentBody;
	return IsValid(OutRootBody);
}

bool USRCelestialBodyRuntimeLibrary::GetCelestialPrimaryStar(const AActor* Actor, AActor*& OutPrimaryStar)
{
	OutPrimaryStar = nullptr;

	if (!IsCelestialBodyActor(Actor))
	{
		return false;
	}

	if (IsCelestialStarActor(Actor))
	{
		OutPrimaryStar = const_cast<AActor*>(Actor);
		return true;
	}

	AActor* RootBody = nullptr;
	if (!GetCelestialRootBody(Actor, RootBody) || !IsValid(RootBody) || !IsCelestialStarActor(RootBody))
	{
		return false;
	}

	OutPrimaryStar = RootBody;
	return true;
}

bool USRCelestialBodyRuntimeLibrary::GetCelestialOrbitRadius(const AActor* Actor, float& OutOrbitRadius)
{
	if (const ASRCelestialBody* ProceduralBody = Cast<ASRCelestialBody>(Actor))
	{
		if (const USROrbit* OrbitComponent = ProceduralBody->GetOrbitComponent())
		{
			OutOrbitRadius = OrbitComponent->GetOrbitRadius();
			return true;
		}
	}

	return TryGetFloatPropertyValue(Actor, CelestialBodyPropertyNames::OrbitRadius, OutOrbitRadius);
}

bool USRCelestialBodyRuntimeLibrary::GetCelestialOrbitPeriod(const AActor* Actor, float& OutOrbitPeriod)
{
	if (const ASRCelestialBody* ProceduralBody = Cast<ASRCelestialBody>(Actor))
	{
		if (const USROrbit* OrbitComponent = ProceduralBody->GetOrbitComponent())
		{
			OutOrbitPeriod = OrbitComponent->GetOrbitPeriodSeconds();
			return true;
		}
	}

	return TryGetFloatPropertyValue(Actor, CelestialBodyPropertyNames::OrbitPeriod, OutOrbitPeriod);
}

bool USRCelestialBodyRuntimeLibrary::GetCelestialStartingPhase(const AActor* Actor, float& OutStartingPhaseDegrees)
{
	if (const ASRCelestialBody* ProceduralBody = Cast<ASRCelestialBody>(Actor))
	{
		if (const USROrbit* OrbitComponent = ProceduralBody->GetOrbitComponent())
		{
			OutStartingPhaseDegrees = OrbitComponent->GetStartingPhaseDegrees();
			return true;
		}
	}

	return TryGetFloatPropertyValue(Actor, CelestialBodyPropertyNames::StartingPhase, OutStartingPhaseDegrees);
}

float USRCelestialBodyRuntimeLibrary::GetCelestialFocusZoomDistance(const AActor* Actor, float CameraFieldOfViewDegrees, float FramingPadding)
{
	if (!IsValid(Actor))
	{
		LogMissingCelestialData(Actor, TEXT("actor"));
		return 0.0f;
	}

	const float SafeFieldOfViewDegrees = FMath::Clamp(CameraFieldOfViewDegrees, 5.0f, 170.0f);
	float FocusZoomMultiplier = 0.0f;
	if (!TryGetFloatPropertyValue(Actor, CelestialBodyPropertyNames::FocusZoomMultiplier, FocusZoomMultiplier))
	{
		LogMissingCelestialData(Actor, TEXT("FocusZoomMultiplier"));
		return 0.0f;
	}
	FocusZoomMultiplier = FMath::Max(0.0f, FocusZoomMultiplier);

	float VisualRadius = GetAuthoredCelestialRadius(Actor);
	if (VisualRadius <= KINDA_SMALL_NUMBER)
	{
		VisualRadius = GetLargestVisualSphereRadius(Actor);
	}
	if (VisualRadius > KINDA_SMALL_NUMBER)
	{
		const float HalfFieldOfViewRadians = FMath::DegreesToRadians(SafeFieldOfViewDegrees * 0.5f);
		const float FramedDistance = VisualRadius / FMath::Tan(HalfFieldOfViewRadians);
		return FMath::Max(0.0f, FramedDistance * FocusZoomMultiplier);
	}

	LogMissingCelestialData(Actor, TEXT("ApproximateRadius"));
	return 0.0f;
}

FText USRCelestialBodyRuntimeLibrary::GetCelestialDisplayName(const AActor* Actor)
{
	FText DisplayName;
	if (TryGetTextLikePropertyValue(Actor, CelestialBodyPropertyNames::DisplayName, DisplayName) && !DisplayName.IsEmpty())
	{
		return DisplayName;
	}

	LogMissingCelestialData(Actor, TEXT("DisplayName"));
	return FText::GetEmpty();
}

bool USRCelestialBodyRuntimeLibrary::GetCelestialCanConstruct(const AActor* Actor)
{
	if (const ASRCelestialBody* ProceduralBody = Cast<ASRCelestialBody>(Actor))
	{
		return ProceduralBody->GetSpec().bCanConstruct;
	}

	bool bCanConstruct = false;
	TryGetBoolPropertyValue(Actor, CelestialBodyPropertyNames::CanConstruct, bCanConstruct);
	return bCanConstruct;
}

float USRCelestialBodyRuntimeLibrary::GetCelestialApproximateRadius(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		LogMissingCelestialData(Actor, TEXT("actor"));
		return 0.0f;
	}

	const float AuthoredRadius = GetAuthoredCelestialRadius(Actor);
	if (AuthoredRadius > KINDA_SMALL_NUMBER)
	{
		return AuthoredRadius;
	}

	const float LargestRadius = GetLargestVisualSphereRadius(Actor);
	if (LargestRadius > KINDA_SMALL_NUMBER)
	{
		return LargestRadius;
	}

	LogMissingCelestialData(Actor, TEXT("ApproximateRadius"));
	return 0.0f;
}

USRPlanetSurfaceGrid* USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(const AActor* Actor)
{
	if (const ASRCelestialBody* ProceduralBody = Cast<ASRCelestialBody>(Actor))
	{
		return ProceduralBody->GetSurfaceGrid();
	}

	return IsValid(Actor)
		? Actor->FindComponentByClass<USRPlanetSurfaceGrid>()
		: nullptr;
}

FSRCelestialBodyFocusInfo USRCelestialBodyRuntimeLibrary::BuildCelestialBodyFocusInfo(const AActor* Actor)
{
	FSRCelestialBodyFocusInfo FocusInfo;
	if (!IsCelestialBodyActor(Actor))
	{
		return FocusInfo;
	}

	FocusInfo.Actor = const_cast<AActor*>(Actor);
	FocusInfo.DisplayName = GetCelestialDisplayName(Actor);
	FocusInfo.bCanConstruct = GetCelestialCanConstruct(Actor);
	FocusInfo.bHasSurfaceGrid = IsValid(FindPlanetSurfaceGrid(Actor));
	FocusInfo.bIsValid = true;
	return FocusInfo;
}
