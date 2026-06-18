#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "Templates/Function.h"

namespace StarRovers::Surface::Interaction
{
	struct FSRSurfaceGridDisplayCoord
	{
		ESRCubeSphereFace Face = ESRCubeSphereFace::PositiveX;
		int32 X = 0;
		int32 Y = 0;
	};

	struct FSRSurfaceGridDisplayFaceBasis
	{
		FVector Normal = FVector::ForwardVector;
		FVector AxisX = FVector::RightVector;
		FVector AxisY = FVector::UpVector;
	};

	struct FSRSurfaceGridDisplayWalkBasis
	{
		FIntPoint AxisX = FIntPoint(1, 0);
		FIntPoint AxisY = FIntPoint(0, 1);
	};

	class FSRSurfaceGridDisplayMapper
	{
	public:
		explicit FSRSurfaceGridDisplayMapper(int32 InFaceResolution);

		FSRSurfaceGridDisplayCoord CanonicalToDisplay(const FSRPlanetSurfaceGridCellId& CellId) const;
		FSRPlanetSurfaceGridCellId DisplayToCanonical(const FSRSurfaceGridDisplayCoord& Coord) const;
		bool TryRotateWalkBasisAcrossEdge(
			ESRCubeSphereFace SourceFace,
			ESRCubeSphereFace TargetFace,
			FSRSurfaceGridDisplayWalkBasis& WalkBasis) const;
		bool TryStepDisplayCoord(
			const FSRSurfaceGridDisplayCoord& FromCoord,
			const FIntPoint& LocalStep,
			FSRSurfaceGridDisplayCoord& OutCoord,
			bool& bOutCrossedEdge) const;

	private:
		enum class ESRGridDisplayEdge : uint8
		{
			NegativeX,
			PositiveX,
			NegativeY,
			PositiveY
		};

		FSRSurfaceGridDisplayFaceBasis GetDisplayFaceBasis(ESRCubeSphereFace Face) const;
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
			const FSRSurfaceGridDisplayCoord& From,
			ESRGridDisplayEdge Edge,
			FSRSurfaceGridDisplayCoord& Out) const;

		int32 SafeFaceResolution = 1;
	};

	class FSRSurfaceGridInteractionPatchBuilder
	{
	public:
		static bool BuildPatchCellIds(
			const FSRPlanetSurfaceGridCellId& CenterCellId,
			int32 FaceResolution,
			TFunctionRef<bool(const FSRPlanetSurfaceGridCellId&)> IsValidCell,
			TArray<FSRPlanetSurfaceGridCellId>& OutCellIds);

	private:
		static constexpr int32 PatchRadius = 2;
	};
}
