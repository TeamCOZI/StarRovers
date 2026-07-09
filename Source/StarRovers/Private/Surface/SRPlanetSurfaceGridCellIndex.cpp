#include "SRPlanetSurfaceGridCellIndex.h"

namespace StarRovers::SurfaceGridCellIndex
{
	int32 GetFlatCellIndex(const FSRPlanetSurfaceGridCellId& CellId, int32 FaceResolution)
	{
		if (!CellId.IsValid(FaceResolution))
		{
			return INDEX_NONE;
		}

		return ((static_cast<int32>(CellId.Face) * FaceResolution) + CellId.CellY) * FaceResolution + CellId.CellX;
	}

	bool GetCellIndex(
		const FSRPlanetSurfaceGridCellIndexState& CellIndexState,
		const TArray<FSRPlanetSurfaceGridCell>& Cells,
		int32 FaceResolution,
		const FSRPlanetSurfaceGridCellId& CellId,
		int32& OutIndex)
	{
		const int32 FlatIndex = GetFlatCellIndex(CellId, FaceResolution);
		if (CellIndexState.IndexByFlatId.IsValidIndex(FlatIndex))
		{
			OutIndex = CellIndexState.IndexByFlatId[FlatIndex];
			return Cells.IsValidIndex(OutIndex);
		}

		if (const int32* FoundIndex = CellIndexState.IndexById.Find(CellId))
		{
			OutIndex = *FoundIndex;
			return Cells.IsValidIndex(OutIndex);
		}

		OutIndex = INDEX_NONE;
		return false;
	}

	void RebuildCellIndex(
		FSRPlanetSurfaceGridCellIndexState& CellIndexState,
		const TArray<FSRPlanetSurfaceGridCell>& Cells,
		int32 FaceResolution)
	{
		CellIndexState.IndexById.Reset();
		CellIndexState.IndexByFlatId.Init(INDEX_NONE, 6 * FaceResolution * FaceResolution);

		for (int32 CellIndex = 0; CellIndex < Cells.Num(); ++CellIndex)
		{
			const int32 FlatIndex = GetFlatCellIndex(Cells[CellIndex].CellId, FaceResolution);
			if (CellIndexState.IndexByFlatId.IsValidIndex(FlatIndex))
			{
				CellIndexState.IndexByFlatId[FlatIndex] = CellIndex;
			}
			else
			{
				CellIndexState.IndexById.Add(Cells[CellIndex].CellId, CellIndex);
			}
		}
	}

	TArray<int32> BuildFlatCellIndexFromMap(
		const TMap<FSRPlanetSurfaceGridCellId, int32>& CellIndexById,
		int32 FaceResolution)
	{
		const int32 SafeFaceResolution = FMath::Max(1, FaceResolution);
		const int32 ExpectedFlatCellCount = 6 * SafeFaceResolution * SafeFaceResolution;
		TArray<int32> CellIndexByFlatId;
		CellIndexByFlatId.Init(INDEX_NONE, ExpectedFlatCellCount);

		for (const TPair<FSRPlanetSurfaceGridCellId, int32>& CellIndexPair : CellIndexById)
		{
			const int32 FlatIndex = GetFlatCellIndex(CellIndexPair.Key, FaceResolution);
			if (CellIndexByFlatId.IsValidIndex(FlatIndex))
			{
				CellIndexByFlatId[FlatIndex] = CellIndexPair.Value;
			}
		}

		return CellIndexByFlatId;
	}

	bool TryAssignFlatCellIndex(
		FSRPlanetSurfaceGridCellIndexState& CellIndexState,
		TArray<int32>&& NewCellIndexByFlatId,
		int32 FaceResolution)
	{
		const int32 ExpectedFlatCellCount = 6 * FaceResolution * FaceResolution;
		if (NewCellIndexByFlatId.Num() != ExpectedFlatCellCount)
		{
			return false;
		}

		CellIndexState.IndexById.Reset();
		CellIndexState.IndexByFlatId = MoveTemp(NewCellIndexByFlatId);
		return true;
	}

	FSRPlanetSurfaceGridCellInfo BuildCellInfo(const FSRPlanetSurfaceGridCell& Cell, int32 FaceResolution)
	{
		FSRPlanetSurfaceGridCellInfo CellInfo;
		CellInfo.CellId = Cell.CellId;
		CellInfo.FaceResolution = FaceResolution;
		CellInfo.FaceCellIndex = Cell.CellId.IsValid(FaceResolution)
			? Cell.CellId.CellY * FaceResolution + Cell.CellId.CellX
			: Cell.CellId.CellX;
		CellInfo.DisplayCellX = Cell.CellId.CellX;
		CellInfo.DisplayCellY = Cell.CellId.CellY;
		if (Cell.CellId.Face == ESRCubeSphereFace::PositiveZ || Cell.CellId.Face == ESRCubeSphereFace::NegativeZ)
		{
			const int32 SafeFaceResolution = FMath::Max(1, FaceResolution);
			CellInfo.DisplayCellX = SafeFaceResolution - 1 - Cell.CellId.CellX;
			CellInfo.DisplayCellY = SafeFaceResolution - 1 - Cell.CellId.CellY;
		}
		CellInfo.DisplayCellIndex = Cell.CellId.IsValid(FaceResolution)
			? CellInfo.DisplayCellY * FaceResolution + CellInfo.DisplayCellX
			: CellInfo.DisplayCellX;
		CellInfo.FaceUVMin = Cell.FaceUVMin;
		CellInfo.FaceUVMax = Cell.FaceUVMax;
		CellInfo.FaceUVCenter = (Cell.FaceUVMin + Cell.FaceUVMax) * 0.5f;
		CellInfo.LocalCenter = Cell.LocalCenter;
		CellInfo.LocalNormal = Cell.LocalNormal;
		const FVector LatitudeDirection = Cell.LocalCenter.GetSafeNormal();
		const FVector FallbackLatitudeDirection = LatitudeDirection.IsNearlyZero()
			? Cell.LocalNormal.GetSafeNormal()
			: LatitudeDirection;
		const float LatitudeSin = static_cast<float>(FMath::Clamp(FallbackLatitudeDirection.Z, -1.0, 1.0));
		CellInfo.LatitudeDegrees = FMath::RadiansToDegrees(FMath::Asin(LatitudeSin));
		CellInfo.ApproxSurfaceArea = Cell.ApproxSurfaceArea;
		CellInfo.Biome = Cell.Biome;
		CellInfo.BiomeId = Cell.BiomeId;
		CellInfo.WaterRole = Cell.WaterRole;
		CellInfo.SurfaceTemperature = Cell.SurfaceTemperature;
		CellInfo.TemperatureState = Cell.TemperatureState;
		CellInfo.Neighbors = Cell.Neighbors;
		CellInfo.bOccupied = Cell.bOccupied;
		CellInfo.OccupantId = Cell.OccupantId;
		CellInfo.bCanConstruct = !Cell.bOccupied;
		return CellInfo;
	}

	FSRPlanetSurfaceGridCellInfo ResolveRuntimeCellInfo(
		const FSRPlanetSurfaceGridCellInfo& CellInfo,
		const FTransform& ComponentTransform)
	{
		FSRPlanetSurfaceGridCellInfo RuntimeCellInfo = CellInfo;
		RuntimeCellInfo.WorldCenter = ComponentTransform.TransformPosition(CellInfo.LocalCenter);
		RuntimeCellInfo.WorldNormal = ComponentTransform.TransformVectorNoScale(CellInfo.LocalNormal).GetSafeNormal();
		if (RuntimeCellInfo.WorldNormal.IsNearlyZero())
		{
			RuntimeCellInfo.WorldNormal = FVector::UpVector;
		}
		return RuntimeCellInfo;
	}

	void RebuildCellInfoIndex(
		FSRPlanetSurfaceGridCellIndexState& CellIndexState,
		const TArray<FSRPlanetSurfaceGridCell>& Cells,
		int32 FaceResolution)
	{
		CellIndexState.InfoById.Reset();
		CellIndexState.InfoByFlatId.Reset();
		CellIndexState.InfoByFlatId.SetNum(6 * FaceResolution * FaceResolution);

		int32 FaceCellCounts[6] = {};
		for (const FSRPlanetSurfaceGridCell& Cell : Cells)
		{
			FSRPlanetSurfaceGridCellInfo CellInfo = BuildCellInfo(Cell, FaceResolution);
			int32& FaceCellCount = FaceCellCounts[static_cast<int32>(Cell.CellId.Face)];
			CellInfo.FaceCellIndex = FaceCellCount;
			++FaceCellCount;
			StoreCellInfo(CellIndexState, FaceResolution, CellInfo);
		}
	}

	bool GetStoredCellInfoById(
		const FSRPlanetSurfaceGridCellIndexState& CellIndexState,
		const TArray<FSRPlanetSurfaceGridCell>& Cells,
		int32 FaceResolution,
		const FSRPlanetSurfaceGridCellId& CellId,
		FSRPlanetSurfaceGridCellInfo& OutCellInfo)
	{
		const int32 FlatIndex = GetFlatCellIndex(CellId, FaceResolution);
		if (CellIndexState.InfoByFlatId.IsValidIndex(FlatIndex)
			&& CellIndexState.IndexByFlatId.IsValidIndex(FlatIndex)
			&& Cells.IsValidIndex(CellIndexState.IndexByFlatId[FlatIndex])
			&& CellIndexState.InfoByFlatId[FlatIndex].CellId == CellId)
		{
			OutCellInfo = CellIndexState.InfoByFlatId[FlatIndex];
			return true;
		}

		if (const FSRPlanetSurfaceGridCellInfo* FoundCellInfo = CellIndexState.InfoById.Find(CellId))
		{
			OutCellInfo = *FoundCellInfo;
			return true;
		}

		return false;
	}

	void StoreCellInfo(
		FSRPlanetSurfaceGridCellIndexState& CellIndexState,
		int32 FaceResolution,
		const FSRPlanetSurfaceGridCellInfo& CellInfo)
	{
		const int32 FlatIndex = GetFlatCellIndex(CellInfo.CellId, FaceResolution);
		if (CellIndexState.InfoByFlatId.IsValidIndex(FlatIndex))
		{
			CellIndexState.InfoByFlatId[FlatIndex] = CellInfo;
		}
		else
		{
			CellIndexState.InfoById.Add(CellInfo.CellId, CellInfo);
		}
	}
}
