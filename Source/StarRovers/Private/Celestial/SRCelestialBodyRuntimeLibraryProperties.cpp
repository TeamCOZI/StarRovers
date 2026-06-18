#include "SRCelestialBodyRuntimeLibraryInternal.h"

#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

namespace StarRovers::CelestialBodyRuntime
{
	namespace PropertyNames
	{
		const FName VariableName(TEXT("VariableName"));
		const FName ParentBody(TEXT("ParentBody"));
		const FName OrbitRadius(TEXT("OrbitRadius"));
		const FName OrbitPeriod(TEXT("OrbitPeriod"));
		const FName InitialAngle(TEXT("InitialAngle"));
		const FName FocusZoomMultiplier(TEXT("FocusZoomMultiplier"));
		const FName CanConstruct(TEXT("CanConstruct"));
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
}
