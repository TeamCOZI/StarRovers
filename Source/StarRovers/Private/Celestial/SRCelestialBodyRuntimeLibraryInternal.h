#pragma once

#include "CoreMinimal.h"

class AActor;
class FProperty;
class UPrimitiveComponent;

namespace StarRovers::CelestialBodyRuntime
{
	namespace PropertyNames
	{
		extern const FName VariableName;
		extern const FName ParentBody;
		extern const FName OrbitRadius;
		extern const FName OrbitPeriod;
		extern const FName InitialAngle;
		extern const FName FocusZoomMultiplier;
		extern const FName CanConstruct;
	}

	namespace ComponentTags
	{
		extern const FName GravityLine;
		extern const FName GravityLineRoot;
		extern const FName GravityLineSegment;
		extern const FName RotationAxisLine;
		extern const FName RotationAxisLineRoot;
	}

	bool IsLikelyCelestialClass(const AActor* Actor);
	bool IsLikelyStarClass(const AActor* Actor);
	bool TryGetFloatPropertyValue(const AActor* Actor, const FName PropertyName, float& OutValue);
	bool TryGetBoolPropertyValue(const AActor* Actor, const FName PropertyName, bool& OutValue);
	bool TryGetActorPropertyValue(const AActor* Actor, const FName PropertyName, AActor*& OutActor);
	bool TryGetTextLikePropertyValue(const AActor* Actor, const FName PropertyName, FText& OutValue);
	void LogMissingCelestialData(const AActor* Actor, const TCHAR* FieldName);
	float GetScaledBodyRadius(const AActor* Actor);
}
