#include "Celestial/SRDynamicMeshBaseDataAsset.h"

#include "Surface/SRPlanetSurfaceGridLibrary.h"

namespace
{
	uint32 HashPrecomputedSourcePosition(const FVector& Position)
	{
		const FVector Direction = Position.GetSafeNormal();
		constexpr float DirectionQuantizationScale = 100000.0f;
		const int32 QuantizedX = FMath::RoundToInt(Direction.X * DirectionQuantizationScale);
		const int32 QuantizedY = FMath::RoundToInt(Direction.Y * DirectionQuantizationScale);
		const int32 QuantizedZ = FMath::RoundToInt(Direction.Z * DirectionQuantizationScale);
		return HashCombine(HashCombine(::GetTypeHash(QuantizedX), ::GetTypeHash(QuantizedY)), ::GetTypeHash(QuantizedZ));
	}

	FSRDynamicMeshBaseSourceMetadataCell BuildPrecomputedSourceMetadataCell(const FSRPlanetSurfaceGridCell& Cell)
	{
		FSRDynamicMeshBaseSourceMetadataCell MetadataCell;
		MetadataCell.CornerHash00 = HashPrecomputedSourcePosition(Cell.Corner00);
		MetadataCell.CornerHash10 = HashPrecomputedSourcePosition(Cell.Corner10);
		MetadataCell.CornerHash11 = HashPrecomputedSourcePosition(Cell.Corner11);
		MetadataCell.CornerHash01 = HashPrecomputedSourcePosition(Cell.Corner01);
		return MetadataCell;
	}
}

uint32 FSRDynamicMeshBaseSourceMetadataCell::GetCornerHash(int32 CornerIndex) const
{
	switch (CornerIndex)
	{
	case 0:
		return CornerHash00;
	case 1:
		return CornerHash10;
	case 2:
		return CornerHash11;
	case 3:
		return CornerHash01;
	default:
		return 0;
	}
}

void FSRDynamicMeshBasePrecomputedCell::SetFromSurfaceGridCell(const FSRPlanetSurfaceGridCell& SourceCell)
{
	CellId = SourceCell.CellId;
	LocalCenter = SourceCell.LocalCenter;
	LocalNormal = SourceCell.LocalNormal;
	Corner00 = SourceCell.Corner00;
	Corner10 = SourceCell.Corner10;
	Corner11 = SourceCell.Corner11;
	Corner01 = SourceCell.Corner01;
	FaceUVMin = SourceCell.FaceUVMin;
	FaceUVMax = SourceCell.FaceUVMax;
	ApproxSurfaceArea = SourceCell.ApproxSurfaceArea;
	Neighbors = SourceCell.Neighbors;
}

FSRPlanetSurfaceGridCell FSRDynamicMeshBasePrecomputedCell::ToSurfaceGridCell(float Scale) const
{
	FSRPlanetSurfaceGridCell Cell;
	Cell.CellId = CellId;
	Cell.LocalCenter = LocalCenter * Scale;
	Cell.LocalNormal = LocalNormal;
	Cell.Corner00 = Corner00 * Scale;
	Cell.Corner10 = Corner10 * Scale;
	Cell.Corner11 = Corner11 * Scale;
	Cell.Corner01 = Corner01 * Scale;
	Cell.FaceUVMin = FaceUVMin;
	Cell.FaceUVMax = FaceUVMax;
	Cell.ApproxSurfaceArea = ApproxSurfaceArea * Scale * Scale;
	Cell.Neighbors = Neighbors;
	return Cell;
}

int32 USRDynamicMeshBaseDataAsset::GetClampedFaceResolution() const
{
	return FMath::Clamp(FaceResolution, 1, 512);
}

float USRDynamicMeshBaseDataAsset::GetSafeBaseRadius(float FallbackRadius) const
{
	return FMath::Max(1.0f, BaseRadius > KINDA_SMALL_NUMBER ? BaseRadius : FallbackRadius);
}

int32 USRDynamicMeshBaseDataAsset::GetExpectedCellCount() const
{
	const int32 Resolution = GetClampedFaceResolution();
	return 6 * Resolution * Resolution;
}

bool USRDynamicMeshBaseDataAsset::HasValidPrecomputedCells() const
{
	return BaseShape == ESRDynamicMeshBaseShape::CubeSphere
		&& PrecomputedFaceResolution == GetClampedFaceResolution()
		&& PrecomputedCells.Num() == GetExpectedCellCount()
		&& PrecomputedBaseRadius > KINDA_SMALL_NUMBER;
}

const TArray<FSRDynamicMeshBasePrecomputedCell>* USRDynamicMeshBaseDataAsset::GetValidPrecomputedCells() const
{
	if (!HasValidPrecomputedCells())
	{
		return nullptr;
	}

	return &PrecomputedCells;
}

float USRDynamicMeshBaseDataAsset::GetPrecomputedCellScale(float TargetRadius) const
{
	const float SafeTargetRadius = FMath::Max(1.0f, TargetRadius);
	const float SafePrecomputedRadius = FMath::Max(1.0f, PrecomputedBaseRadius);
	return SafeTargetRadius / SafePrecomputedRadius;
}

bool USRDynamicMeshBaseDataAsset::TryGetPrecomputedBaseCells(float TargetRadius, TArray<FSRPlanetSurfaceGridCell>& OutCells) const
{
	const TArray<FSRDynamicMeshBasePrecomputedCell>* ValidPrecomputedCells = GetValidPrecomputedCells();
	if (!ValidPrecomputedCells)
	{
		return false;
	}

	const float Scale = GetPrecomputedCellScale(TargetRadius);
	OutCells.Reset(PrecomputedCells.Num());
	OutCells.Reserve(PrecomputedCells.Num());
	for (const FSRDynamicMeshBasePrecomputedCell& PrecomputedCell : *ValidPrecomputedCells)
	{
		OutCells.Add(PrecomputedCell.ToSurfaceGridCell(Scale));
	}
	return true;
}

const TArray<FSRDynamicMeshBaseSourceMetadataCell>* USRDynamicMeshBaseDataAsset::GetValidPrecomputedSourceMetadata() const
{
	if (!HasValidPrecomputedCells()
		|| PrecomputedSourceMetadata.Num() != PrecomputedCells.Num())
	{
		return nullptr;
	}

	return &PrecomputedSourceMetadata;
}

void USRDynamicMeshBaseDataAsset::BakePrecomputedBaseData()
{
	if (BaseShape != ESRDynamicMeshBaseShape::CubeSphere)
	{
		UE_LOG(LogTemp, Warning, TEXT("DynamicMeshBase '%s' cannot bake unsupported shape."), *GetName());
		return;
	}

	Modify();

	const int32 Resolution = GetClampedFaceResolution();
	constexpr float BakeRadius = 1.0f;
	const TArray<FSRPlanetSurfaceGridCell> GeneratedCells =
		USRPlanetSurfaceGridLibrary::GenerateCubeSphereCells(Resolution, BakeRadius);

	PrecomputedFaceResolution = Resolution;
	PrecomputedBaseRadius = BakeRadius;
	PrecomputedCells.Reset(GeneratedCells.Num());
	PrecomputedCells.Reserve(GeneratedCells.Num());
	PrecomputedSourceMetadata.Reset(GeneratedCells.Num());
	PrecomputedSourceMetadata.Reserve(GeneratedCells.Num());

	for (const FSRPlanetSurfaceGridCell& GeneratedCell : GeneratedCells)
	{
		FSRDynamicMeshBasePrecomputedCell& PrecomputedCell = PrecomputedCells.AddDefaulted_GetRef();
		PrecomputedCell.SetFromSurfaceGridCell(GeneratedCell);
		PrecomputedSourceMetadata.Add(BuildPrecomputedSourceMetadataCell(GeneratedCell));
	}

	MarkPackageDirty();

	UE_LOG(
		LogTemp,
		Display,
		TEXT("DynamicMeshBase '%s' baked precomputed base data. Resolution=%d Cells=%d SourceMetadata=%d"),
		*GetName(),
		PrecomputedFaceResolution,
		PrecomputedCells.Num(),
		PrecomputedSourceMetadata.Num());
}

void USRDynamicMeshBaseDataAsset::ClearPrecomputedBaseData()
{
	Modify();

	PrecomputedFaceResolution = 0;
	PrecomputedBaseRadius = 1.0f;
	PrecomputedCells.Reset();
	PrecomputedSourceMetadata.Reset();

	MarkPackageDirty();

	UE_LOG(LogTemp, Display, TEXT("DynamicMeshBase '%s' cleared precomputed base data."), *GetName());
}
