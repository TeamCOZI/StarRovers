#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "Templates/Function.h"

namespace StarRovers::Surface::Interaction
{
	struct FSRPlanetSurfaceGridDisplayCoord
	{
		ESRCubeSphereFace Face = ESRCubeSphereFace::PositiveX;
		int32 X = 0;
		int32 Y = 0;
	};

	struct FSRPlanetSurfaceGridDisplayFaceBasis
	{
		FVector Normal = FVector::ForwardVector;
		FVector AxisX = FVector::RightVector;
		FVector AxisY = FVector::UpVector;
	};

	struct FSRPlanetSurfaceGridDisplayWalkBasis
	{
		FIntPoint AxisX = FIntPoint(1, 0);
		FIntPoint AxisY = FIntPoint(0, 1);
	};

	class FSRPlanetSurfaceGridDisplayMapper
	{
	public:
		explicit FSRPlanetSurfaceGridDisplayMapper(int32 InFaceResolution);

		FSRPlanetSurfaceGridDisplayCoord CanonicalToDisplay(const FSRPlanetSurfaceGridCellId& CellId) const;
		FSRPlanetSurfaceGridCellId DisplayToCanonical(const FSRPlanetSurfaceGridDisplayCoord& Coord) const;
		bool TryRotateWalkBasisAcrossEdge(
			ESRCubeSphereFace SourceFace,
			ESRCubeSphereFace TargetFace,
			FSRPlanetSurfaceGridDisplayWalkBasis& WalkBasis) const;
		bool TryStepDisplayCoord(
			const FSRPlanetSurfaceGridDisplayCoord& FromCoord,
			const FIntPoint& LocalStep,
			FSRPlanetSurfaceGridDisplayCoord& OutCoord,
			bool& bOutCrossedEdge) const;

	private:
		enum class ESRGridDisplayEdge : uint8
		{
			NegativeX,
			PositiveX,
			NegativeY,
			PositiveY
		};

		FSRPlanetSurfaceGridDisplayFaceBasis GetDisplayFaceBasis(ESRCubeSphereFace Face) const;
		FVector ResolveDisplayDirection(ESRCubeSphereFace Face, const FIntPoint& LocalDirection) const;
		bool TryResolveLocalDisplayDirection(
			ESRCubeSphereFace Face,
			const FVector& WorldDirection,
			FIntPoint& OutLocalDirection) const;
		FVector RotateDisplayDirectionAcrossEdge(
			const FVector& SourceWorldDirection,
			ESRCubeSphereFace SourceFace,
			ESRCubeSphereFace TargetFace) const;
		bool TryTransitionDisplayEdge(
			const FSRPlanetSurfaceGridDisplayCoord& From,
			ESRGridDisplayEdge Edge,
			FSRPlanetSurfaceGridDisplayCoord& Out) const;

		int32 SafeFaceResolution = 1;
	};

	class FSRPlanetSurfaceGridInteractionPatchBuilder
	{
	public:
		static bool BuildPatchCellIds(
			const FSRPlanetSurfaceGridCellId& CenterCellId,
			int32 FaceResolution,
			TFunctionRef<bool(const FSRPlanetSurfaceGridCellId&)> IsValidCell,
			TArray<FSRPlanetSurfaceGridCellId>& OutCellIds,
			int32 PatchSize = 5);
	};
}
