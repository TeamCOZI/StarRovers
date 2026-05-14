#include "Surface/SRPlanetSurfaceGrid.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"
#include "Surface/SRPlanetSurfaceGridLibrary.h"
#include "Visual/SRLineThicknessUtils.h"

USRPlanetSurfaceGrid::USRPlanetSurfaceGrid()
{
	PrimaryComponentTick.bCanEverTick = true;

	FaceResolution = 8;
	PlanetRadius = 1000.0f;
	bRebuildGridOnRegister = false;
	ConstructionHeightOffset = 15.0f;
	bDrawDebugGrid = true;
	bGridVisible = false;
	DebugLineColor = FLinearColor(0.15f, 0.85f, 1.0f, 1.0f);
	DebugLineOpacity = 1.0f;
	HoveredCellColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);
	SelectedCellColor = FLinearColor(0.25f, 1.0f, 0.35f, 1.0f);
	OccupiedCellColor = FLinearColor(1.0f, 0.35f, 0.35f, 1.0f);
	DebugLineThickness = 1.0f;
	GridSurfaceOffset = 8.0f;
	bUseProceduralTerrain = false;
	TerrainSeed = 1337;
	TerrainHeight = 0.0f;
	TerrainFrequency = 3.0f;
	TerrainOctaves = 4;
	TerrainPersistence = 0.5f;
	bHasHoveredCell = false;
	bHasSelectedCell = false;
}

void USRPlanetSurfaceGrid::OnRegister()
{
	Super::OnRegister();
	UpdateDebugTickState();

	if (bRebuildGridOnRegister && !IsTemplate())
	{
		RebuildGrid();
	}
}

void USRPlanetSurfaceGrid::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bDrawDebugGrid && bGridVisible)
	{
		DrawDebugGrid();
	}
}

void USRPlanetSurfaceGrid::RebuildGrid()
{
	Cells = USRPlanetSurfaceGridLibrary::GenerateCubeSphereCells(FMath::Max(1, FaceResolution), FMath::Max(1.0f, PlanetRadius));
	ClearHoveredCell();
	ClearSelectedCell();
	RebuildCellIndex();
}

void USRPlanetSurfaceGrid::SetPlanetRadius(float NewPlanetRadius)
{
	PlanetRadius = FMath::Max(1.0f, NewPlanetRadius);
}

void USRPlanetSurfaceGrid::SetFaceResolution(int32 NewFaceResolution)
{
	FaceResolution = FMath::Max(1, NewFaceResolution);
}

void USRPlanetSurfaceGrid::ClearOccupancy()
{
	for (FSRPlanetSurfaceGridCell& Cell : Cells)
	{
		Cell.bOccupied = false;
		Cell.OccupantId = NAME_None;
	}
}

int32 USRPlanetSurfaceGrid::GetFaceResolution() const
{
	return FaceResolution;
}

float USRPlanetSurfaceGrid::GetPlanetRadius() const
{
	return PlanetRadius;
}

int32 USRPlanetSurfaceGrid::GetCellCount() const
{
	return Cells.Num();
}

TArray<FSRPlanetSurfaceGridCell> USRPlanetSurfaceGrid::GetCells() const
{
	return Cells;
}

bool USRPlanetSurfaceGrid::GetCellById(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCell& OutCell) const
{
	int32 CellIndex = INDEX_NONE;
	if (!GetCellIndex(CellId, CellIndex))
	{
		OutCell = FSRPlanetSurfaceGridCell();
		return false;
	}

	OutCell = Cells[CellIndex];
	return true;
}

bool USRPlanetSurfaceGrid::GetCellNeighbors(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCellNeighbors& OutNeighbors) const
{
	FSRPlanetSurfaceGridCell Cell;
	if (!GetCellById(CellId, Cell))
	{
		OutNeighbors = FSRPlanetSurfaceGridCellNeighbors();
		return false;
	}

	OutNeighbors = Cell.Neighbors;
	return true;
}

bool USRPlanetSurfaceGrid::GetCellWorldTransform(const FSRPlanetSurfaceGridCellId& CellId, float HeightOffset, FTransform& OutTransform) const
{
	FSRPlanetSurfaceGridCell Cell;
	if (!GetCellById(CellId, Cell))
	{
		OutTransform = FTransform::Identity;
		return false;
	}

	FVector LocalTangent = ((Cell.Corner10 + Cell.Corner11) - (Cell.Corner00 + Cell.Corner01)) * 0.5f;
	if (LocalTangent.IsNearlyZero())
	{
		LocalTangent = FVector::CrossProduct(FVector::UpVector, Cell.LocalNormal);
		if (LocalTangent.IsNearlyZero())
		{
			LocalTangent = FVector::ForwardVector;
		}
	}

	const FVector WorldPosition = ResolveWorldSurfacePoint(Cell.LocalNormal, ConstructionHeightOffset + HeightOffset);
	const FVector WorldCorner00 = ResolveWorldSurfacePoint(Cell.Corner00.GetSafeNormal(), GridSurfaceOffset);
	const FVector WorldCorner10 = ResolveWorldSurfacePoint(Cell.Corner10.GetSafeNormal(), GridSurfaceOffset);
	const FVector WorldCorner01 = ResolveWorldSurfacePoint(Cell.Corner01.GetSafeNormal(), GridSurfaceOffset);
	const FVector DerivedWorldNormal = FVector::CrossProduct(WorldCorner10 - WorldCorner00, WorldCorner01 - WorldCorner00).GetSafeNormal();
	const FVector WorldNormal = DerivedWorldNormal.IsNearlyZero()
		? GetComponentTransform().TransformVectorNoScale(Cell.LocalNormal).GetSafeNormal()
		: DerivedWorldNormal;
	FVector WorldTangent = (WorldCorner10 - WorldCorner00).GetSafeNormal();
	if (WorldTangent.IsNearlyZero())
	{
		WorldTangent = GetComponentTransform().TransformVectorNoScale(LocalTangent).GetSafeNormal();
	}
	const FQuat WorldRotation = FRotationMatrix::MakeFromXZ(WorldTangent, WorldNormal).ToQuat();

	OutTransform = FTransform(WorldRotation, WorldPosition, FVector::OneVector);
	return true;
}

bool USRPlanetSurfaceGrid::GetCellWorldCorners(const FSRPlanetSurfaceGridCellId& CellId, FVector& OutCorner00, FVector& OutCorner10, FVector& OutCorner11, FVector& OutCorner01) const
{
	FSRPlanetSurfaceGridCell Cell;
	if (!GetCellById(CellId, Cell))
	{
		OutCorner00 = FVector::ZeroVector;
		OutCorner10 = FVector::ZeroVector;
		OutCorner11 = FVector::ZeroVector;
		OutCorner01 = FVector::ZeroVector;
		return false;
	}

	OutCorner00 = ResolveWorldSurfacePoint(Cell.Corner00.GetSafeNormal(), GridSurfaceOffset);
	OutCorner10 = ResolveWorldSurfacePoint(Cell.Corner10.GetSafeNormal(), GridSurfaceOffset);
	OutCorner11 = ResolveWorldSurfacePoint(Cell.Corner11.GetSafeNormal(), GridSurfaceOffset);
	OutCorner01 = ResolveWorldSurfacePoint(Cell.Corner01.GetSafeNormal(), GridSurfaceOffset);
	return true;
}

bool USRPlanetSurfaceGrid::ProjectWorldLocationToCell(const FVector& WorldLocation, FSRPlanetSurfaceGridCell& OutCell) const
{
	if (Cells.IsEmpty())
	{
		OutCell = FSRPlanetSurfaceGridCell();
		return false;
	}

	const FVector LocalDirection = GetComponentTransform().InverseTransformPosition(WorldLocation).GetSafeNormal();
	if (LocalDirection.IsNearlyZero())
	{
		OutCell = FSRPlanetSurfaceGridCell();
		return false;
	}

	FSRPlanetSurfaceGridCellId CellId;
	FVector2D UnusedFaceCoordinates = FVector2D::ZeroVector;
	if (!USRPlanetSurfaceGridLibrary::ProjectDirectionToCubeSphereCellId(LocalDirection, FaceResolution, CellId, UnusedFaceCoordinates))
	{
		OutCell = FSRPlanetSurfaceGridCell();
		return false;
	}

	return GetCellById(CellId, OutCell);
}

bool USRPlanetSurfaceGrid::RaycastCell(const FVector& RayOrigin, const FVector& RayDirection, FSRPlanetSurfaceGridCell& OutCell, FVector& OutHitLocation) const
{
	OutHitLocation = FVector::ZeroVector;
	if (!IntersectRayWithSurfaceSphere(RayOrigin, RayDirection, OutHitLocation))
	{
		OutCell = FSRPlanetSurfaceGridCell();
		return false;
	}

	if (!ProjectWorldLocationToCell(OutHitLocation, OutCell))
	{
		return false;
	}

	OutHitLocation = ResolveWorldSurfacePoint(OutCell.LocalNormal, 0.0f);
	return true;
}

bool USRPlanetSurfaceGrid::SetCellOccupied(const FSRPlanetSurfaceGridCellId& CellId, bool bOccupied, FName OccupantId)
{
	int32 CellIndex = INDEX_NONE;
	if (!GetCellIndex(CellId, CellIndex))
	{
		return false;
	}

	FSRPlanetSurfaceGridCell& Cell = Cells[CellIndex];
	Cell.bOccupied = bOccupied;
	Cell.OccupantId = bOccupied ? OccupantId : NAME_None;
	return true;
}

bool USRPlanetSurfaceGrid::SetHoveredCell(const FSRPlanetSurfaceGridCellId& CellId)
{
	int32 CellIndex = INDEX_NONE;
	if (!GetCellIndex(CellId, CellIndex))
	{
		return false;
	}

	bHasHoveredCell = true;
	HoveredCellId = CellId;
	return true;
}

void USRPlanetSurfaceGrid::ClearHoveredCell()
{
	bHasHoveredCell = false;
	HoveredCellId = FSRPlanetSurfaceGridCellId();
}

bool USRPlanetSurfaceGrid::HasHoveredCell() const
{
	return bHasHoveredCell;
}

bool USRPlanetSurfaceGrid::GetHoveredCell(FSRPlanetSurfaceGridCell& OutCell) const
{
	return bHasHoveredCell && GetCellById(HoveredCellId, OutCell);
}

bool USRPlanetSurfaceGrid::SetSelectedCell(const FSRPlanetSurfaceGridCellId& CellId)
{
	int32 CellIndex = INDEX_NONE;
	if (!GetCellIndex(CellId, CellIndex))
	{
		return false;
	}

	bHasSelectedCell = true;
	SelectedCellId = CellId;
	return true;
}

void USRPlanetSurfaceGrid::ClearSelectedCell()
{
	bHasSelectedCell = false;
	SelectedCellId = FSRPlanetSurfaceGridCellId();
}

bool USRPlanetSurfaceGrid::HasSelectedCell() const
{
	return bHasSelectedCell;
}

bool USRPlanetSurfaceGrid::GetSelectedCell(FSRPlanetSurfaceGridCell& OutCell) const
{
	return bHasSelectedCell && GetCellById(SelectedCellId, OutCell);
}

void USRPlanetSurfaceGrid::DrawDebugGrid(float Duration) const
{
	if (!GetWorld() || Cells.IsEmpty() || !bGridVisible)
	{
		return;
	}

	const FLinearColor DefaultDebugLineColor(DebugLineColor.R, DebugLineColor.G, DebugLineColor.B, DebugLineOpacity);
	const FColor DefaultLineColor = DefaultDebugLineColor.ToFColor(true);
	const FColor HoverLineColor = HoveredCellColor.ToFColor(true);
	const FColor SelectedLineColor = SelectedCellColor.ToFColor(true);
	const FColor OccupiedLineColor = OccupiedCellColor.ToFColor(true);

	FSRCameraInfo CameraInfo;
	FSRLineThicknessUtils::TryBuildPrimaryCameraInfo(GetWorld(), CameraInfo);

	float ReferenceViewDepth = FSRLineThicknessUtils::DefaultReferenceViewDepth;
	float ReferenceFieldOfViewDegrees = FSRLineThicknessUtils::DefaultReferenceFieldOfViewDegrees;
	FSRLineThicknessUtils::ResolveReferenceView(GetWorld(), ReferenceViewDepth, ReferenceFieldOfViewDegrees);

	for (const FSRPlanetSurfaceGridCell& Cell : Cells)
	{
		const bool bIsHovered = bHasHoveredCell && (Cell.CellId == HoveredCellId);
		const bool bIsSelected = bHasSelectedCell && (Cell.CellId == SelectedCellId);
		const FColor LineColor = bIsSelected
			? SelectedLineColor
			: (bIsHovered ? HoverLineColor : (Cell.bOccupied ? OccupiedLineColor : DefaultLineColor));
		const float LineThickness = bIsSelected
			? DebugLineThickness * 2.5f
			: (bIsHovered ? DebugLineThickness * 2.0f : DebugLineThickness);

		DrawDebugSurfaceLine(Cell.Corner00, Cell.Corner10, LineColor, Duration, LineThickness, CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees);
		DrawDebugSurfaceLine(Cell.Corner10, Cell.Corner11, LineColor, Duration, LineThickness, CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees);
		DrawDebugSurfaceLine(Cell.Corner11, Cell.Corner01, LineColor, Duration, LineThickness, CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees);
		DrawDebugSurfaceLine(Cell.Corner01, Cell.Corner00, LineColor, Duration, LineThickness, CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees);
	}
}

void USRPlanetSurfaceGrid::SetGridVisible(bool bNewGridVisible)
{
	if (bGridVisible == bNewGridVisible)
	{
		return;
	}

	bGridVisible = bNewGridVisible;
	if (!bGridVisible)
	{
		ClearHoveredCell();
		ClearSelectedCell();
	}
	UpdateDebugTickState();
}

bool USRPlanetSurfaceGrid::IsGridVisible() const
{
	return bGridVisible;
}

void USRPlanetSurfaceGrid::ConfigureDebugGrid(
	FLinearColor NewGridLineColor,
	float NewGridLineOpacity,
	float NewLineThickness,
	FLinearColor NewHoveredCellColor,
	FLinearColor NewSelectedCellColor,
	FLinearColor NewOccupiedCellColor,
	float NewSurfaceOffset)
{
	DebugLineColor = NewGridLineColor;
	DebugLineOpacity = FMath::Clamp(NewGridLineOpacity, 0.0f, 1.0f);
	DebugLineThickness = FMath::Max(0.0f, NewLineThickness);
	HoveredCellColor = NewHoveredCellColor;
	SelectedCellColor = NewSelectedCellColor;
	OccupiedCellColor = NewOccupiedCellColor;
	GridSurfaceOffset = FMath::Max(0.0f, NewSurfaceOffset);
}

void USRPlanetSurfaceGrid::ConfigureConstructionHeightOffset(float NewConstructionHeightOffset)
{
	ConstructionHeightOffset = NewConstructionHeightOffset;
}

float USRPlanetSurfaceGrid::GetSurfaceHeightOffsetAtDirection_Implementation(FVector LocalUnitDirection) const
{
	return ComputeProceduralTerrainHeight(LocalUnitDirection);
}

void USRPlanetSurfaceGrid::ConfigureProceduralTerrain(
	bool bNewUseProceduralTerrain,
	int32 NewTerrainSeed,
	float NewTerrainHeight,
	float NewTerrainFrequency,
	int32 NewTerrainOctaves,
	float NewTerrainPersistence)
{
	bUseProceduralTerrain = bNewUseProceduralTerrain;
	TerrainSeed = NewTerrainSeed;
	TerrainHeight = FMath::Max(0.0f, NewTerrainHeight);
	TerrainFrequency = FMath::Max(0.01f, NewTerrainFrequency);
	TerrainOctaves = FMath::Max(1, NewTerrainOctaves);
	TerrainPersistence = FMath::Clamp(NewTerrainPersistence, 0.0f, 1.0f);
}

bool USRPlanetSurfaceGrid::GetCellIndex(const FSRPlanetSurfaceGridCellId& CellId, int32& OutIndex) const
{
	if (const int32* FoundIndex = CellIndexById.Find(CellId))
	{
		OutIndex = *FoundIndex;
		return Cells.IsValidIndex(OutIndex);
	}

	OutIndex = INDEX_NONE;
	return false;
}

void USRPlanetSurfaceGrid::RebuildCellIndex()
{
	CellIndexById.Reset();
	CellIndexById.Reserve(Cells.Num());

	for (int32 CellIndex = 0; CellIndex < Cells.Num(); ++CellIndex)
	{
		CellIndexById.Add(Cells[CellIndex].CellId, CellIndex);
	}
}

void USRPlanetSurfaceGrid::UpdateDebugTickState()
{
	SetComponentTickEnabled(bDrawDebugGrid && bGridVisible);
}

float USRPlanetSurfaceGrid::GetEffectiveWorldRadius() const
{
	const FVector Scale3D = GetComponentTransform().GetScale3D().GetAbs();
	return PlanetRadius * Scale3D.GetMax();
}

void USRPlanetSurfaceGrid::DrawDebugSurfaceLine(
	const FVector& LocalDirectionA,
	const FVector& LocalDirectionB,
	const FColor& LineColor,
	float Duration,
	float LineThickness,
	const FSRCameraInfo& CameraInfo,
	float ReferenceViewDepth,
	float ReferenceFieldOfViewDegrees) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector DirectionA = LocalDirectionA.GetSafeNormal();
	const FVector DirectionB = LocalDirectionB.GetSafeNormal();
	if (DirectionA.IsNearlyZero() || DirectionB.IsNearlyZero())
	{
		return;
	}

	constexpr int32 SegmentCount = 8;
	FVector PreviousPoint = ResolveWorldSurfacePoint(DirectionA, GridSurfaceOffset);

	for (int32 SegmentIndex = 1; SegmentIndex <= SegmentCount; ++SegmentIndex)
	{
		const float Alpha = static_cast<float>(SegmentIndex) / static_cast<float>(SegmentCount);
		const FVector SampleDirection = FMath::Lerp(DirectionA, DirectionB, Alpha).GetSafeNormal();
		if (SampleDirection.IsNearlyZero())
		{
			continue;
		}

		const FVector CurrentPoint = ResolveWorldSurfacePoint(SampleDirection, GridSurfaceOffset);
		const FVector SegmentMidpoint = (PreviousPoint + CurrentPoint) * 0.5f;
		const float ScreenSpaceThickness = FSRLineThicknessUtils::ComputeWorldThicknessAtLocation(
			CameraInfo,
			SegmentMidpoint,
			LineThickness,
			ReferenceViewDepth,
			ReferenceFieldOfViewDegrees);
		DrawDebugLine(World, PreviousPoint, CurrentPoint, LineColor, false, Duration, 0, FMath::Max(0.0f, ScreenSpaceThickness));
		PreviousPoint = CurrentPoint;
	}
}

FVector USRPlanetSurfaceGrid::ResolveLocalSurfacePoint(const FVector& LocalUnitDirection, float HeightOffset) const
{
	const FVector LocalDirection = LocalUnitDirection.GetSafeNormal();
	if (LocalDirection.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	const float SurfaceHeightOffset = GetSurfaceHeightOffsetAtDirection(LocalDirection);
	const FVector LocalBasePoint = LocalDirection * FMath::Max(1.0f, PlanetRadius + SurfaceHeightOffset);
	const FVector LocalSurfaceNormal = bUseProceduralTerrain
		? ComputeProceduralSurfaceNormal(LocalDirection)
		: LocalDirection;
	return LocalBasePoint + (LocalSurfaceNormal.GetSafeNormal() * HeightOffset);
}

FVector USRPlanetSurfaceGrid::ResolveWorldSurfacePoint(const FVector& LocalUnitDirection, float HeightOffset) const
{
	return GetComponentTransform().TransformPosition(ResolveLocalSurfacePoint(LocalUnitDirection, HeightOffset));
}

float USRPlanetSurfaceGrid::ComputeProceduralTerrainHeight(FVector LocalUnitDirection) const
{
	if (!bUseProceduralTerrain || TerrainHeight <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const FVector Direction = LocalUnitDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return 0.0f;
	}

	const int64 Seed64 = static_cast<int64>(TerrainSeed);
	const FVector SeedOffset(
		static_cast<float>((Seed64 * 15731LL) % 10007LL),
		static_cast<float>((Seed64 * 789221LL) % 10009LL),
		static_cast<float>((Seed64 * 1376312589LL) % 10037LL));

	float Frequency = TerrainFrequency;
	float Amplitude = 1.0f;
	float NoiseSum = 0.0f;
	float AmplitudeSum = 0.0f;
	const int32 SafeOctaves = FMath::Clamp(TerrainOctaves, 1, 8);

	for (int32 OctaveIndex = 0; OctaveIndex < SafeOctaves; ++OctaveIndex)
	{
		const float NoiseValue = FMath::PerlinNoise3D((Direction * Frequency) + SeedOffset);
		NoiseSum += NoiseValue * Amplitude;
		AmplitudeSum += Amplitude;
		Frequency *= 2.0f;
		Amplitude *= TerrainPersistence;
	}

	const float NormalizedNoise = AmplitudeSum > KINDA_SMALL_NUMBER
		? NoiseSum / AmplitudeSum
		: 0.0f;
	return NormalizedNoise * TerrainHeight;
}

FVector USRPlanetSurfaceGrid::ComputeProceduralSurfaceNormal(FVector LocalUnitDirection) const
{
	const FVector Direction = LocalUnitDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return FVector::UpVector;
	}

	FVector TangentA = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
	if (TangentA.IsNearlyZero())
	{
		TangentA = FVector::CrossProduct(Direction, FVector::RightVector).GetSafeNormal();
	}
	const FVector TangentB = FVector::CrossProduct(Direction, TangentA).GetSafeNormal();
	if (TangentA.IsNearlyZero() || TangentB.IsNearlyZero())
	{
		return Direction;
	}

	auto ResolveBasePoint = [this](const FVector& SampleDirection)
	{
		const FVector SafeDirection = SampleDirection.GetSafeNormal();
		const float SurfaceHeightOffset = GetSurfaceHeightOffsetAtDirection(SafeDirection);
		return SafeDirection * FMath::Max(1.0f, PlanetRadius + SurfaceHeightOffset);
	};

	constexpr float NormalSampleStep = 0.003f;
	const FVector PointA0 = ResolveBasePoint(Direction - TangentA * NormalSampleStep);
	const FVector PointA1 = ResolveBasePoint(Direction + TangentA * NormalSampleStep);
	const FVector PointB0 = ResolveBasePoint(Direction - TangentB * NormalSampleStep);
	const FVector PointB1 = ResolveBasePoint(Direction + TangentB * NormalSampleStep);

	FVector Normal = FVector::CrossProduct(PointA1 - PointA0, PointB1 - PointB0).GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		return Direction;
	}

	if (FVector::DotProduct(Normal, Direction) < 0.0f)
	{
		Normal *= -1.0f;
	}
	return Normal;
}

bool USRPlanetSurfaceGrid::IntersectRayWithSurfaceSphere(const FVector& RayOrigin, const FVector& RayDirection, FVector& OutHitLocation) const
{
	OutHitLocation = FVector::ZeroVector;

	const FVector Direction = RayDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return false;
	}

	const FVector SphereCenter = GetComponentLocation();
	const float SphereRadius = GetEffectiveWorldRadius()
		+ (bUseProceduralTerrain ? FMath::Max(0.0f, TerrainHeight) * GetComponentTransform().GetScale3D().GetAbsMax() : 0.0f);
	if (SphereRadius <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector OriginToCenter = RayOrigin - SphereCenter;
	const double HalfB = FVector::DotProduct(OriginToCenter, Direction);
	const double C = OriginToCenter.SizeSquared() - FMath::Square(static_cast<double>(SphereRadius));
	const double Discriminant = (HalfB * HalfB) - C;
	if (Discriminant < 0.0)
	{
		return false;
	}

	const double SqrtDiscriminant = FMath::Sqrt(Discriminant);
	double HitDistance = -HalfB - SqrtDiscriminant;
	if (HitDistance < 0.0)
	{
		HitDistance = -HalfB + SqrtDiscriminant;
	}

	if (HitDistance < 0.0)
	{
		return false;
	}

	OutHitLocation = RayOrigin + (Direction * static_cast<float>(HitDistance));
	return true;
}
