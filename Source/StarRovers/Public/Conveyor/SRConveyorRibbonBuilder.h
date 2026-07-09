#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorTypes.h"

class USRPlanetSurfaceGrid;

namespace UE::Geometry
{
	class FDynamicMesh3;
}

namespace StarRovers::Conveyor
{
	struct FSRConveyorRibbonBuildSettings
	{
		float BeltWidth = 1.0f;
		float BeltThickness = 1.0f;
		float BeltSurfaceOffset = 0.0f;
		float PCGSplineHeightOffset = 0.0f;
		FTransform ComponentTransform = FTransform::Identity;
	};

	struct STARROVERS_API FSRConveyorRibbonBuilder
	{
		static bool BuildPathSplinePoints(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const FSRConveyorBeltPath& BeltPath,
			const FSRConveyorRibbonBuildSettings& Settings,
			TArray<FVector>& OutWorldPoints,
			TArray<FVector>& OutWorldNormals);

		static bool BuildPathRibbon(
			UE::Geometry::FDynamicMesh3& BeltMesh,
			USRPlanetSurfaceGrid* SurfaceGrid,
			const FSRConveyorBeltPath& BeltPath,
			const FSRConveyorRibbonBuildSettings& Settings);

	private:
		static bool BuildPathPoints(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const FSRConveyorBeltPath& BeltPath,
			float HeightOffset,
			float BeltWidth,
			TArray<FVector>& OutWorldPoints,
			TArray<FVector>& OutWorldNormals);

		static float ResolveBeltHalfWidth(const TArray<FVector>& WorldPoints, float BeltWidth);
		static float ResolveBeltHalfThickness(float HalfWidth, float LayerHeight, float BeltThickness);
	};
}
