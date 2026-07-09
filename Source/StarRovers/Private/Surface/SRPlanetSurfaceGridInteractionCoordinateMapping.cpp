#include "Surface/SRPlanetSurfaceGridInteractionCoordinateMapping.h"

namespace
{
	struct FSRPlanetSurfaceGridCubeFaceBasis
	{
		FVector Normal = FVector::ForwardVector;
		FVector AxisU = FVector::RightVector;
		FVector AxisV = FVector::UpVector;
	};

	FSRPlanetSurfaceGridCubeFaceBasis GetCubeFaceBasis(ESRCubeSphereFace Face)
	{
		switch (Face)
		{
		case ESRCubeSphereFace::PositiveX:
			return { FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) };
		case ESRCubeSphereFace::NegativeX:
			return { FVector(-1.0f, 0.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) };
		case ESRCubeSphereFace::PositiveY:
			return { FVector(0.0f, 1.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) };
		case ESRCubeSphereFace::NegativeY:
			return { FVector(0.0f, -1.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) };
		case ESRCubeSphereFace::PositiveZ:
			return { FVector(0.0f, 0.0f, 1.0f), FVector(0.0f, 1.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f) };
		case ESRCubeSphereFace::NegativeZ:
		default:
			return { FVector(0.0f, 0.0f, -1.0f), FVector(0.0f, 1.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f) };
		}
	}
}

namespace StarRovers::Surface::Interaction
{
	FSRPlanetSurfaceGridDisplayMapper::FSRPlanetSurfaceGridDisplayMapper(int32 InFaceResolution)
		: SafeFaceResolution(FMath::Max(1, InFaceResolution))
	{
	}

	FSRPlanetSurfaceGridDisplayCoord FSRPlanetSurfaceGridDisplayMapper::CanonicalToDisplay(const FSRPlanetSurfaceGridCellId& CellId) const
	{
		FSRPlanetSurfaceGridDisplayCoord Coord;
		Coord.Face = CellId.Face;
		Coord.X = CellId.CellX;
		Coord.Y = CellId.CellY;
		if (CellId.Face == ESRCubeSphereFace::PositiveZ || CellId.Face == ESRCubeSphereFace::NegativeZ)
		{
			Coord.X = SafeFaceResolution - 1 - CellId.CellX;
			Coord.Y = SafeFaceResolution - 1 - CellId.CellY;
		}

		return Coord;
	}

	FSRPlanetSurfaceGridCellId FSRPlanetSurfaceGridDisplayMapper::DisplayToCanonical(const FSRPlanetSurfaceGridDisplayCoord& Coord) const
	{
		FSRPlanetSurfaceGridCellId CellId;
		CellId.Face = Coord.Face;
		CellId.CellX = Coord.X;
		CellId.CellY = Coord.Y;
		if (Coord.Face == ESRCubeSphereFace::PositiveZ || Coord.Face == ESRCubeSphereFace::NegativeZ)
		{
			CellId.CellX = SafeFaceResolution - 1 - Coord.X;
			CellId.CellY = SafeFaceResolution - 1 - Coord.Y;
		}

		return CellId;
	}

	bool FSRPlanetSurfaceGridDisplayMapper::TryRotateWalkBasisAcrossEdge(
		ESRCubeSphereFace SourceFace,
		ESRCubeSphereFace TargetFace,
		FSRPlanetSurfaceGridDisplayWalkBasis& WalkBasis) const
	{
		const FVector SourceAxisX = ResolveDisplayDirection(SourceFace, WalkBasis.AxisX);
		const FVector SourceAxisY = ResolveDisplayDirection(SourceFace, WalkBasis.AxisY);
		const FVector TargetAxisX = RotateDisplayDirectionAcrossEdge(SourceAxisX, SourceFace, TargetFace);
		const FVector TargetAxisY = RotateDisplayDirectionAcrossEdge(SourceAxisY, SourceFace, TargetFace);
		return TryResolveLocalDisplayDirection(TargetFace, TargetAxisX, WalkBasis.AxisX)
			&& TryResolveLocalDisplayDirection(TargetFace, TargetAxisY, WalkBasis.AxisY);
	}

	bool FSRPlanetSurfaceGridDisplayMapper::TryStepDisplayCoord(
		const FSRPlanetSurfaceGridDisplayCoord& FromCoord,
		const FIntPoint& LocalStep,
		FSRPlanetSurfaceGridDisplayCoord& OutCoord,
		bool& bOutCrossedEdge) const
	{
		OutCoord = FromCoord;
		bOutCrossedEdge = false;
		if (LocalStep.X == 0 && LocalStep.Y == 0)
		{
			return true;
		}

		if (LocalStep.X != 0)
		{
			const int32 TargetX = FromCoord.X + (LocalStep.X < 0 ? -1 : 1);
			if (TargetX >= 0 && TargetX < SafeFaceResolution)
			{
				OutCoord.X = TargetX;
				return true;
			}

			bOutCrossedEdge = true;
			return TryTransitionDisplayEdge(
				FromCoord,
				LocalStep.X < 0 ? ESRGridDisplayEdge::NegativeX : ESRGridDisplayEdge::PositiveX,
				OutCoord);
		}

		if (LocalStep.Y != 0)
		{
			const int32 TargetY = FromCoord.Y + (LocalStep.Y < 0 ? -1 : 1);
			if (TargetY >= 0 && TargetY < SafeFaceResolution)
			{
				OutCoord.Y = TargetY;
				return true;
			}

			bOutCrossedEdge = true;
			return TryTransitionDisplayEdge(
				FromCoord,
				LocalStep.Y < 0 ? ESRGridDisplayEdge::NegativeY : ESRGridDisplayEdge::PositiveY,
				OutCoord);
		}

		return false;
	}

	FSRPlanetSurfaceGridDisplayFaceBasis FSRPlanetSurfaceGridDisplayMapper::GetDisplayFaceBasis(ESRCubeSphereFace Face) const
	{
		const FSRPlanetSurfaceGridCubeFaceBasis CubeBasis = GetCubeFaceBasis(Face);
		FSRPlanetSurfaceGridDisplayFaceBasis Basis;
		Basis.Normal = CubeBasis.Normal;
		Basis.AxisX = CubeBasis.AxisU;
		Basis.AxisY = CubeBasis.AxisV;
		if (Face == ESRCubeSphereFace::PositiveZ || Face == ESRCubeSphereFace::NegativeZ)
		{
			Basis.AxisX *= -1.0f;
			Basis.AxisY *= -1.0f;
		}
		return Basis;
	}

	FVector FSRPlanetSurfaceGridDisplayMapper::ResolveDisplayDirection(
		ESRCubeSphereFace Face,
		const FIntPoint& LocalDirection) const
	{
		const FSRPlanetSurfaceGridDisplayFaceBasis Basis = GetDisplayFaceBasis(Face);
		return (Basis.AxisX * static_cast<float>(LocalDirection.X))
			+ (Basis.AxisY * static_cast<float>(LocalDirection.Y));
	}

	bool FSRPlanetSurfaceGridDisplayMapper::TryResolveLocalDisplayDirection(
		ESRCubeSphereFace Face,
		const FVector& WorldDirection,
		FIntPoint& OutLocalDirection) const
	{
		const FSRPlanetSurfaceGridDisplayFaceBasis Basis = GetDisplayFaceBasis(Face);
		const float AxisXDot = FVector::DotProduct(WorldDirection, Basis.AxisX);
		const float AxisYDot = FVector::DotProduct(WorldDirection, Basis.AxisY);
		if (FMath::Abs(AxisXDot) > 0.999f)
		{
			OutLocalDirection = FIntPoint(AxisXDot > 0.0f ? 1 : -1, 0);
			return true;
		}
		if (FMath::Abs(AxisYDot) > 0.999f)
		{
			OutLocalDirection = FIntPoint(0, AxisYDot > 0.0f ? 1 : -1);
			return true;
		}

		return false;
	}

	FVector FSRPlanetSurfaceGridDisplayMapper::RotateDisplayDirectionAcrossEdge(
		const FVector& SourceWorldDirection,
		ESRCubeSphereFace SourceFace,
		ESRCubeSphereFace TargetFace) const
	{
		const FSRPlanetSurfaceGridDisplayFaceBasis SourceBasis = GetDisplayFaceBasis(SourceFace);
		const FSRPlanetSurfaceGridDisplayFaceBasis TargetBasis = GetDisplayFaceBasis(TargetFace);
		const float TargetNormalDot = FVector::DotProduct(SourceWorldDirection, TargetBasis.Normal);
		const FVector SharedEdgeDirection = SourceWorldDirection - (TargetBasis.Normal * TargetNormalDot);
		return SharedEdgeDirection + ((-SourceBasis.Normal) * TargetNormalDot);
	}

	bool FSRPlanetSurfaceGridDisplayMapper::TryTransitionDisplayEdge(
		const FSRPlanetSurfaceGridDisplayCoord& From,
		ESRGridDisplayEdge Edge,
		FSRPlanetSurfaceGridDisplayCoord& Out) const
	{
		const int32 MaxCell = SafeFaceResolution - 1;
		switch (From.Face)
		{
		case ESRCubeSphereFace::PositiveX:
			switch (Edge)
			{
			case ESRGridDisplayEdge::NegativeX:
				Out = { ESRCubeSphereFace::NegativeY, MaxCell, From.Y };
				return true;
			case ESRGridDisplayEdge::PositiveX:
				Out = { ESRCubeSphereFace::PositiveY, 0, From.Y };
				return true;
			case ESRGridDisplayEdge::NegativeY:
				Out = { ESRCubeSphereFace::NegativeZ, MaxCell - From.X, 0 };
				return true;
			case ESRGridDisplayEdge::PositiveY:
				Out = { ESRCubeSphereFace::PositiveZ, MaxCell - From.X, MaxCell };
				return true;
			}
			break;
		case ESRCubeSphereFace::NegativeX:
			switch (Edge)
			{
			case ESRGridDisplayEdge::NegativeX:
				Out = { ESRCubeSphereFace::PositiveY, MaxCell, From.Y };
				return true;
			case ESRGridDisplayEdge::PositiveX:
				Out = { ESRCubeSphereFace::NegativeY, 0, From.Y };
				return true;
			case ESRGridDisplayEdge::NegativeY:
				Out = { ESRCubeSphereFace::NegativeZ, From.X, MaxCell };
				return true;
			case ESRGridDisplayEdge::PositiveY:
				Out = { ESRCubeSphereFace::PositiveZ, From.X, 0 };
				return true;
			}
			break;
		case ESRCubeSphereFace::PositiveY:
			switch (Edge)
			{
			case ESRGridDisplayEdge::NegativeX:
				Out = { ESRCubeSphereFace::PositiveX, MaxCell, From.Y };
				return true;
			case ESRGridDisplayEdge::PositiveX:
				Out = { ESRCubeSphereFace::NegativeX, 0, From.Y };
				return true;
			case ESRGridDisplayEdge::NegativeY:
				Out = { ESRCubeSphereFace::NegativeZ, 0, From.X };
				return true;
			case ESRGridDisplayEdge::PositiveY:
				Out = { ESRCubeSphereFace::PositiveZ, 0, MaxCell - From.X };
				return true;
			}
			break;
		case ESRCubeSphereFace::NegativeY:
			switch (Edge)
			{
			case ESRGridDisplayEdge::NegativeX:
				Out = { ESRCubeSphereFace::NegativeX, MaxCell, From.Y };
				return true;
			case ESRGridDisplayEdge::PositiveX:
				Out = { ESRCubeSphereFace::PositiveX, 0, From.Y };
				return true;
			case ESRGridDisplayEdge::NegativeY:
				Out = { ESRCubeSphereFace::NegativeZ, MaxCell, MaxCell - From.X };
				return true;
			case ESRGridDisplayEdge::PositiveY:
				Out = { ESRCubeSphereFace::PositiveZ, MaxCell, From.X };
				return true;
			}
			break;
		case ESRCubeSphereFace::PositiveZ:
			switch (Edge)
			{
			case ESRGridDisplayEdge::NegativeX:
				Out = { ESRCubeSphereFace::PositiveY, MaxCell - From.Y, MaxCell };
				return true;
			case ESRGridDisplayEdge::PositiveX:
				Out = { ESRCubeSphereFace::NegativeY, From.Y, MaxCell };
				return true;
			case ESRGridDisplayEdge::NegativeY:
				Out = { ESRCubeSphereFace::NegativeX, From.X, MaxCell };
				return true;
			case ESRGridDisplayEdge::PositiveY:
				Out = { ESRCubeSphereFace::PositiveX, MaxCell - From.X, MaxCell };
				return true;
			}
			break;
		case ESRCubeSphereFace::NegativeZ:
			switch (Edge)
			{
			case ESRGridDisplayEdge::NegativeX:
				Out = { ESRCubeSphereFace::PositiveY, From.Y, 0 };
				return true;
			case ESRGridDisplayEdge::PositiveX:
				Out = { ESRCubeSphereFace::NegativeY, MaxCell - From.Y, 0 };
				return true;
			case ESRGridDisplayEdge::NegativeY:
				Out = { ESRCubeSphereFace::PositiveX, MaxCell - From.X, 0 };
				return true;
			case ESRGridDisplayEdge::PositiveY:
				Out = { ESRCubeSphereFace::NegativeX, From.X, 0 };
				return true;
			}
			break;
		default:
			break;
		}

		return false;
	}

	bool FSRPlanetSurfaceGridInteractionPatchBuilder::BuildPatchCellIds(
		const FSRPlanetSurfaceGridCellId& CenterCellId,
		int32 FaceResolution,
		TFunctionRef<bool(const FSRPlanetSurfaceGridCellId&)> IsValidCell,
		TArray<FSRPlanetSurfaceGridCellId>& OutCellIds)
	{
		OutCellIds.Reset();

		if (!IsValidCell(CenterCellId))
		{
			return false;
		}

		const int32 SafeFaceResolution = FMath::Max(1, FaceResolution);
		const FSRPlanetSurfaceGridDisplayMapper DisplayMapper(SafeFaceResolution);

		auto TryStepWalker = [&DisplayMapper, &IsValidCell](
			FSRPlanetSurfaceGridDisplayCoord& CurrentCoord,
			FSRPlanetSurfaceGridDisplayWalkBasis& WalkBasis,
			bool bIsGlobalX,
			int32 StepSign)
		{
			const FIntPoint LocalAxis = bIsGlobalX ? WalkBasis.AxisX : WalkBasis.AxisY;
			const FIntPoint LocalStep(LocalAxis.X * StepSign, LocalAxis.Y * StepSign);
			FSRPlanetSurfaceGridDisplayCoord NextCoord;
			bool bCrossedEdge = false;
			if (!DisplayMapper.TryStepDisplayCoord(CurrentCoord, LocalStep, NextCoord, bCrossedEdge))
			{
				return false;
			}

			if (bCrossedEdge && !DisplayMapper.TryRotateWalkBasisAcrossEdge(CurrentCoord.Face, NextCoord.Face, WalkBasis))
			{
				return false;
			}

			const FSRPlanetSurfaceGridCellId NextCellId = DisplayMapper.DisplayToCanonical(NextCoord);
			if (!IsValidCell(NextCellId))
			{
				return false;
			}

			CurrentCoord = NextCoord;
			return true;
		};

		auto TryWalkPatchCellId = [&DisplayMapper, &TryStepWalker, CenterCellId](
			int32 OffsetX,
			int32 OffsetY,
			bool bWalkXFirst,
			FSRPlanetSurfaceGridCellId& OutCellId)
		{
			FSRPlanetSurfaceGridDisplayCoord CurrentCoord = DisplayMapper.CanonicalToDisplay(CenterCellId);
			FSRPlanetSurfaceGridDisplayWalkBasis WalkBasis;
			auto WalkAxis = [&TryStepWalker, &CurrentCoord, &WalkBasis](int32 Offset, bool bIsX)
			{
				const int32 Step = Offset < 0 ? -1 : 1;
				for (int32 StepIndex = 0; StepIndex < FMath::Abs(Offset); ++StepIndex)
				{
					if (!TryStepWalker(CurrentCoord, WalkBasis, bIsX, Step))
					{
						return false;
					}
				}
				return true;
			};

			if (bWalkXFirst)
			{
				if (!WalkAxis(OffsetX, true) || !WalkAxis(OffsetY, false))
				{
					return false;
				}
			}
			else if (!WalkAxis(OffsetY, false) || !WalkAxis(OffsetX, true))
			{
				return false;
			}

			OutCellId = DisplayMapper.DisplayToCanonical(CurrentCoord);
			return true;
		};

		TSet<FSRPlanetSurfaceGridCellId> PatchCellIds;
		PatchCellIds.Reserve((PatchRadius * 2 + 1) * (PatchRadius * 2 + 1));
		OutCellIds.Reserve((PatchRadius * 2 + 1) * (PatchRadius * 2 + 1));

		auto AddPatchCellId = [&IsValidCell, &PatchCellIds, &OutCellIds](const FSRPlanetSurfaceGridCellId& PatchCellId)
		{
			if (!IsValidCell(PatchCellId) || PatchCellIds.Contains(PatchCellId))
			{
				return;
			}

			PatchCellIds.Add(PatchCellId);
			OutCellIds.Add(PatchCellId);
		};
		auto AddWalkedPatchCellId = [&TryWalkPatchCellId, &AddPatchCellId](int32 OffsetX, int32 OffsetY, bool bWalkXFirst)
		{
			FSRPlanetSurfaceGridCellId PatchCellId;
			if (TryWalkPatchCellId(OffsetX, OffsetY, bWalkXFirst, PatchCellId))
			{
				AddPatchCellId(PatchCellId);
			}
		};
		auto GetOverflowOffset = [](int32 DirectionSign, int32 OverflowIndex, int32 OverflowCount)
		{
			const int32 DistanceFromCenter = PatchRadius - OverflowCount + 1 + OverflowIndex;
			return DirectionSign > 0 ? DistanceFromCenter : -DistanceFromCenter;
		};
		auto GetCornerOverflowSpan = [](int32 XOverflow, int32 YOverflow)
		{
			if (XOverflow <= 0 || YOverflow <= 0)
			{
				return 0;
			}

			return FMath::Min(XOverflow, YOverflow);
		};
		auto GetSideOverflowSpan = [](int32 SideOverflow, int32 PerpendicularNegativeOverflow, int32 PerpendicularPositiveOverflow)
		{
			int32 EffectiveOverflow = SideOverflow;
			if (PerpendicularNegativeOverflow > 0)
			{
				EffectiveOverflow = FMath::Min(EffectiveOverflow, PerpendicularNegativeOverflow);
			}
			if (PerpendicularPositiveOverflow > 0)
			{
				EffectiveOverflow = FMath::Min(EffectiveOverflow, PerpendicularPositiveOverflow);
			}
			return EffectiveOverflow;
		};

		const FSRPlanetSurfaceGridDisplayCoord CenterDisplayCoord = DisplayMapper.CanonicalToDisplay(CenterCellId);
		const int32 DesiredMinX = CenterDisplayCoord.X - PatchRadius;
		const int32 DesiredMaxX = CenterDisplayCoord.X + PatchRadius;
		const int32 DesiredMinY = CenterDisplayCoord.Y - PatchRadius;
		const int32 DesiredMaxY = CenterDisplayCoord.Y + PatchRadius;

		const int32 ClippedMinX = FMath::Clamp(DesiredMinX, 0, SafeFaceResolution - 1);
		const int32 ClippedMaxX = FMath::Clamp(DesiredMaxX, 0, SafeFaceResolution - 1);
		const int32 ClippedMinY = FMath::Clamp(DesiredMinY, 0, SafeFaceResolution - 1);
		const int32 ClippedMaxY = FMath::Clamp(DesiredMaxY, 0, SafeFaceResolution - 1);

		const int32 NegativeXOverflow = FMath::Max(0, -DesiredMinX);
		const int32 PositiveXOverflow = FMath::Max(0, DesiredMaxX - (SafeFaceResolution - 1));
		const int32 NegativeYOverflow = FMath::Max(0, -DesiredMinY);
		const int32 PositiveYOverflow = FMath::Max(0, DesiredMaxY - (SafeFaceResolution - 1));

		const int32 NegativeXSideOverflow = GetSideOverflowSpan(NegativeXOverflow, NegativeYOverflow, PositiveYOverflow);
		const int32 PositiveXSideOverflow = GetSideOverflowSpan(PositiveXOverflow, NegativeYOverflow, PositiveYOverflow);
		const int32 NegativeYSideOverflow = GetSideOverflowSpan(NegativeYOverflow, NegativeXOverflow, PositiveXOverflow);
		const int32 PositiveYSideOverflow = GetSideOverflowSpan(PositiveYOverflow, NegativeXOverflow, PositiveXOverflow);

		for (int32 CellY = ClippedMinY; CellY <= ClippedMaxY; ++CellY)
		{
			for (int32 CellX = ClippedMinX; CellX <= ClippedMaxX; ++CellX)
			{
				FSRPlanetSurfaceGridDisplayCoord PatchCoord = CenterDisplayCoord;
				PatchCoord.X = CellX;
				PatchCoord.Y = CellY;
				const FSRPlanetSurfaceGridCellId PatchCellId = DisplayMapper.DisplayToCanonical(PatchCoord);
				AddPatchCellId(PatchCellId);
			}
		}

		for (int32 CellY = ClippedMinY; CellY <= ClippedMaxY; ++CellY)
		{
			for (int32 OverflowIndex = 0; OverflowIndex < NegativeXSideOverflow; ++OverflowIndex)
			{
				const int32 OffsetX = GetOverflowOffset(-1, OverflowIndex, NegativeXOverflow);
				AddWalkedPatchCellId(OffsetX, CellY - CenterDisplayCoord.Y, true);
			}
		}
		for (int32 CellY = ClippedMinY; CellY <= ClippedMaxY; ++CellY)
		{
			for (int32 OverflowIndex = 0; OverflowIndex < PositiveXSideOverflow; ++OverflowIndex)
			{
				const int32 OffsetX = GetOverflowOffset(1, OverflowIndex, PositiveXOverflow);
				AddWalkedPatchCellId(OffsetX, CellY - CenterDisplayCoord.Y, true);
			}
		}
		for (int32 CellX = ClippedMinX; CellX <= ClippedMaxX; ++CellX)
		{
			for (int32 OverflowIndex = 0; OverflowIndex < NegativeYSideOverflow; ++OverflowIndex)
			{
				const int32 OffsetY = GetOverflowOffset(-1, OverflowIndex, NegativeYOverflow);
				AddWalkedPatchCellId(CellX - CenterDisplayCoord.X, OffsetY, false);
			}
		}
		for (int32 CellX = ClippedMinX; CellX <= ClippedMaxX; ++CellX)
		{
			for (int32 OverflowIndex = 0; OverflowIndex < PositiveYSideOverflow; ++OverflowIndex)
			{
				const int32 OffsetY = GetOverflowOffset(1, OverflowIndex, PositiveYOverflow);
				AddWalkedPatchCellId(CellX - CenterDisplayCoord.X, OffsetY, false);
			}
		}

		auto AddCornerOverflow = [
			&AddWalkedPatchCellId,
			&GetOverflowOffset,
			&GetCornerOverflowSpan](
			int32 DirectionX,
			int32 XOverflow,
			int32 DirectionY,
			int32 YOverflow)
		{
			const int32 CornerSpan = GetCornerOverflowSpan(XOverflow, YOverflow);
			if (CornerSpan <= 0)
			{
				return;
			}

			const bool bWalkXFirst = XOverflow <= YOverflow;
			for (int32 OverflowYIndex = 0; OverflowYIndex < CornerSpan; ++OverflowYIndex)
			{
				const int32 OffsetY = GetOverflowOffset(DirectionY, OverflowYIndex, YOverflow);
				for (int32 OverflowXIndex = 0; OverflowXIndex < CornerSpan; ++OverflowXIndex)
				{
					const int32 OffsetX = GetOverflowOffset(DirectionX, OverflowXIndex, XOverflow);
					AddWalkedPatchCellId(OffsetX, OffsetY, bWalkXFirst);
				}
			}
		};

		AddCornerOverflow(-1, NegativeXOverflow, -1, NegativeYOverflow);
		AddCornerOverflow(1, PositiveXOverflow, -1, NegativeYOverflow);
		AddCornerOverflow(-1, NegativeXOverflow, 1, PositiveYOverflow);
		AddCornerOverflow(1, PositiveXOverflow, 1, PositiveYOverflow);

		return !OutCellIds.IsEmpty();
	}
}
