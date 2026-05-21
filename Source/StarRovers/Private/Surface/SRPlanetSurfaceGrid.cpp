#include "Surface/SRPlanetSurfaceGrid.h"

#include "Celestial/SRCelestialBody.h"
#include "DrawDebugHelpers.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
#include "SceneManagement.h"
#include "Surface/SRPlanetSurfaceGridLibrary.h"
#include "Surface/SRPlanetTerrainGenerator.h"
#include "UDynamicMesh.h"
#include "Visual/SRLineThicknessUtils.h"

namespace
{
	uint32 HashGridDirection(const FVector& LocalDirection)
	{
		const FVector Direction = LocalDirection.GetSafeNormal();
		const int32 QuantizedX = FMath::RoundToInt((Direction.X + 1.0) * 100000.0);
		const int32 QuantizedY = FMath::RoundToInt((Direction.Y + 1.0) * 100000.0);
		const int32 QuantizedZ = FMath::RoundToInt((Direction.Z + 1.0) * 100000.0);
		return HashCombine(HashCombine(::GetTypeHash(QuantizedX), ::GetTypeHash(QuantizedY)), ::GetTypeHash(QuantizedZ));
	}

	uint64 BuildGridEdgeKey(const FVector& LocalDirectionA, const FVector& LocalDirectionB)
	{
		const uint32 EndpointA = HashGridDirection(LocalDirectionA);
		const uint32 EndpointB = HashGridDirection(LocalDirectionB);
		const uint32 MinEndpoint = FMath::Min(EndpointA, EndpointB);
		const uint32 MaxEndpoint = FMath::Max(EndpointA, EndpointB);
		return (static_cast<uint64>(MinEndpoint) << 32) | static_cast<uint64>(MaxEndpoint);
	}
}

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
	GridSurfaceOffset = 0.0f;
	GridMaterial = nullptr;
	TerrainSettings = FSRPlanetTerrainSettings();
	TerrainSettings.bUseProceduralTerrain = false;
	TerrainSettings.TerrainHeight = 0.0f;
	bHasHoveredCell = false;
	bHasSelectedCell = false;

	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCastShadow(false);
	SetVisibility(false);
	SetHiddenInGame(true);
}

void USRPlanetSurfaceGrid::OnRegister()
{
	Super::OnRegister();
	UpdateDebugTickState();
	ApplyGridMaterial();

	if (bRebuildGridOnRegister && !IsTemplate())
	{
		RebuildGrid();
	}
}

void USRPlanetSurfaceGrid::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void USRPlanetSurfaceGrid::RebuildGrid()
{
	Cells = USRPlanetSurfaceGridLibrary::GenerateCubeSphereCells(FMath::Max(1, FaceResolution), FMath::Max(1.0f, PlanetRadius));
	ClearHoveredCell();
	ClearSelectedCell();
	RebuildCellIndex();
	RebuildGridMesh();
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
	RebuildGridMesh();
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
	RebuildGridMesh();
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
	RebuildGridMesh();
	return true;
}

void USRPlanetSurfaceGrid::ClearHoveredCell()
{
	bHasHoveredCell = false;
	HoveredCellId = FSRPlanetSurfaceGridCellId();
	RebuildGridMesh();
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
	RebuildGridMesh();
	return true;
}

void USRPlanetSurfaceGrid::ClearSelectedCell()
{
	bHasSelectedCell = false;
	SelectedCellId = FSRPlanetSurfaceGridCellId();
	RebuildGridMesh();
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

	TSet<uint64> DrawnEdges;
	DrawnEdges.Reserve(Cells.Num() * 2);
	auto DrawUniqueDefaultEdge = [this, &DrawnEdges, &DefaultLineColor, Duration, &CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees](
		const FVector& CornerA,
		const FVector& CornerB)
	{
		const uint64 EdgeKey = BuildGridEdgeKey(CornerA, CornerB);
		if (DrawnEdges.Contains(EdgeKey))
		{
			return;
		}

		DrawnEdges.Add(EdgeKey);
		DrawDebugSurfaceLine(CornerA, CornerB, DefaultLineColor, Duration, DebugLineThickness, CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees);
	};

	for (const FSRPlanetSurfaceGridCell& Cell : Cells)
	{
		DrawUniqueDefaultEdge(Cell.Corner00, Cell.Corner10);
		DrawUniqueDefaultEdge(Cell.Corner10, Cell.Corner11);
		DrawUniqueDefaultEdge(Cell.Corner11, Cell.Corner01);
		DrawUniqueDefaultEdge(Cell.Corner01, Cell.Corner00);
		DrawUniqueDefaultEdge(Cell.Corner00, Cell.Corner11);
	}

	for (const FSRPlanetSurfaceGridCell& Cell : Cells)
	{
		const bool bIsHovered = bHasHoveredCell && (Cell.CellId == HoveredCellId);
		const bool bIsSelected = bHasSelectedCell && (Cell.CellId == SelectedCellId);
		const bool bShouldHighlightCell = bIsHovered || bIsSelected || Cell.bOccupied;
		if (!bShouldHighlightCell)
		{
			continue;
		}

		const FColor LineColor = bIsSelected ? SelectedLineColor : (bIsHovered ? HoverLineColor : OccupiedLineColor);
		const float LineThickness = bIsSelected ? DebugLineThickness * 2.5f : (bIsHovered ? DebugLineThickness * 2.0f : DebugLineThickness);
		DrawDebugSurfaceLine(Cell.Corner00, Cell.Corner10, LineColor, Duration, LineThickness, CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees);
		DrawDebugSurfaceLine(Cell.Corner10, Cell.Corner11, LineColor, Duration, LineThickness, CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees);
		DrawDebugSurfaceLine(Cell.Corner11, Cell.Corner01, LineColor, Duration, LineThickness, CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees);
		DrawDebugSurfaceLine(Cell.Corner01, Cell.Corner00, LineColor, Duration, LineThickness, CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees);
		DrawDebugSurfaceLine(Cell.Corner00, Cell.Corner11, LineColor, Duration, LineThickness, CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees);
	}
}

void USRPlanetSurfaceGrid::SetGridVisible(bool bNewGridVisible)
{
	bGridVisible = bNewGridVisible;
	SetVisibility(bGridVisible);
	SetHiddenInGame(!bGridVisible);
	if (!bGridVisible)
	{
		ClearHoveredCell();
		ClearSelectedCell();
	}
	else
	{
		RebuildGridMesh();
		ApplyGridMaterial();
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
	DebugLineThickness = FMath::Clamp(NewLineThickness, 0.0f, 2.0f);
	HoveredCellColor = NewHoveredCellColor;
	SelectedCellColor = NewSelectedCellColor;
	OccupiedCellColor = NewOccupiedCellColor;
	GridSurfaceOffset = FMath::Clamp(NewSurfaceOffset, 0.0f, 1.0f);
	RebuildGridMesh();
}

void USRPlanetSurfaceGrid::SetGridMaterial(UMaterialInterface* NewGridMaterial)
{
	GridMaterial = NewGridMaterial;
	ApplyGridMaterial();
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
	FSRPlanetTerrainSettings NewTerrainSettings = TerrainSettings;
	NewTerrainSettings.TerrainProfile = ESRPlanetTerrainProfile::EarthLike;
	NewTerrainSettings.bUseProceduralTerrain = bNewUseProceduralTerrain;
	NewTerrainSettings.TerrainSeed = NewTerrainSeed;
	NewTerrainSettings.TerrainHeight = FMath::Max(0.0f, NewTerrainHeight);
	NewTerrainSettings.DetailFrequency = FMath::Max(0.01f, NewTerrainFrequency);
	NewTerrainSettings.TerrainOctaves = FMath::Max(1, NewTerrainOctaves);
	NewTerrainSettings.TerrainPersistence = FMath::Clamp(NewTerrainPersistence, 0.0f, 1.0f);
	ConfigureTerrain(NewTerrainSettings);
}

void USRPlanetSurfaceGrid::ConfigureTerrain(const FSRPlanetTerrainSettings& NewTerrainSettings)
{
	TerrainSettings = NewTerrainSettings;
	TerrainSettings.TerrainHeight = FMath::Max(0.0f, TerrainSettings.TerrainHeight);
	TerrainSettings.ContinentFrequency = FMath::Max(0.01f, TerrainSettings.ContinentFrequency);
	TerrainSettings.MountainFrequency = FMath::Max(0.01f, TerrainSettings.MountainFrequency);
	TerrainSettings.DetailFrequency = FMath::Max(0.01f, TerrainSettings.DetailFrequency);
	TerrainSettings.ValleyStrength = FMath::Clamp(TerrainSettings.ValleyStrength, 0.0f, 1.0f);
	TerrainSettings.MountainSharpness = FMath::Clamp(TerrainSettings.MountainSharpness, 0.5f, 4.0f);
	TerrainSettings.DomainWarpStrength = FMath::Clamp(TerrainSettings.DomainWarpStrength, 0.0f, 1.0f);
	TerrainSettings.RiverStrength = FMath::Clamp(TerrainSettings.RiverStrength, 0.0f, 1.0f);
	TerrainSettings.LakeStrength = FMath::Clamp(TerrainSettings.LakeStrength, 0.0f, 1.0f);
	TerrainSettings.MicroDetailStrength = FMath::Clamp(TerrainSettings.MicroDetailStrength, 0.0f, 1.0f);
	TerrainSettings.MoistureFrequency = FMath::Max(0.01f, TerrainSettings.MoistureFrequency);
	TerrainSettings.TemperatureNoiseFrequency = FMath::Max(0.01f, TerrainSettings.TemperatureNoiseFrequency);
	TerrainSettings.TerrainOctaves = FMath::Max(1, TerrainSettings.TerrainOctaves);
	TerrainSettings.TerrainPersistence = FMath::Clamp(TerrainSettings.TerrainPersistence, 0.0f, 1.0f);
	TerrainSettings.SeaLevel = FMath::Clamp(TerrainSettings.SeaLevel, -1.0f, 1.0f);
	RebuildGridMesh();
}

FSRPlanetTerrainSample USRPlanetSurfaceGrid::GetTerrainSampleAtDirection(FVector LocalUnitDirection) const
{
	return FSRPlanetTerrainGenerator::SampleTerrain(LocalUnitDirection, TerrainSettings);
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
	SetComponentTickEnabled(false);
}

void USRPlanetSurfaceGrid::RebuildGridMesh()
{
	UE::Geometry::FDynamicMesh3 GridMesh;
	GridMesh.EnableAttributes();
	GridMesh.Attributes()->EnablePrimaryColors();

	if (!Cells.IsEmpty())
	{
		const FLinearColor DefaultLineColor(DebugLineColor.R, DebugLineColor.G, DebugLineColor.B, DebugLineOpacity);
		const FLinearColor HoverLineColor(HoveredCellColor.R, HoveredCellColor.G, HoveredCellColor.B, HoveredCellColor.A);
		const FLinearColor SelectedLineColor(SelectedCellColor.R, SelectedCellColor.G, SelectedCellColor.B, SelectedCellColor.A);
		const FLinearColor OccupiedLineColor(OccupiedCellColor.R, OccupiedCellColor.G, OccupiedCellColor.B, OccupiedCellColor.A);

		const bool bAppendedOwnerWire = AppendOwnerDynamicMeshWire(GridMesh, DefaultLineColor, DebugLineThickness);
		if (!bAppendedOwnerWire)
		{
			TSet<uint64> DrawnEdges;
			DrawnEdges.Reserve(Cells.Num() * 3);
			for (const FSRPlanetSurfaceGridCell& Cell : Cells)
			{
				AppendGridWireCell(GridMesh, Cell, DefaultLineColor, DebugLineThickness, true, &DrawnEdges);
			}
		}

		for (const FSRPlanetSurfaceGridCell& Cell : Cells)
		{
			const bool bIsHovered = bHasHoveredCell && (Cell.CellId == HoveredCellId);
			const bool bIsSelected = bHasSelectedCell && (Cell.CellId == SelectedCellId);
			const bool bShouldHighlightCell = bIsHovered || bIsSelected || Cell.bOccupied;
			if (!bShouldHighlightCell)
			{
				continue;
			}

			const FLinearColor LineColor = bIsSelected ? SelectedLineColor : (bIsHovered ? HoverLineColor : OccupiedLineColor);
			const float LineThickness = bIsSelected ? DebugLineThickness * 2.5f : (bIsHovered ? DebugLineThickness * 2.0f : DebugLineThickness);
			AppendGridWireCell(GridMesh, Cell, LineColor, LineThickness, false, nullptr);
		}
	}

	SetMesh(MoveTemp(GridMesh));
	SetVisibility(bGridVisible);
	SetHiddenInGame(!bGridVisible);
	ApplyGridMaterial();
}

bool USRPlanetSurfaceGrid::AppendOwnerDynamicMeshWire(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FLinearColor& LineColor,
	float LineThickness) const
{
	const ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(GetOwner());
	if (!IsValid(OwnerBody))
	{
		return false;
	}

	UDynamicMeshComponent* OwnerDynamicMeshComponent = OwnerBody->GetCelestialBodyDynamicMesh();
	UDynamicMesh* OwnerDynamicMeshObject = IsValid(OwnerDynamicMeshComponent)
		? OwnerDynamicMeshComponent->GetDynamicMesh()
		: nullptr;
	if (!IsValid(OwnerDynamicMeshObject))
	{
		return false;
	}

	bool bAppendedAnyEdge = false;
	const FTransform OwnerDynamicMeshRelativeTransform = OwnerDynamicMeshComponent->GetRelativeTransform();
	OwnerDynamicMeshObject->ProcessMesh([this, &GridMesh, &LineColor, LineThickness, &bAppendedAnyEdge, &OwnerDynamicMeshRelativeTransform](const UE::Geometry::FDynamicMesh3& OwnerMesh)
	{
		for (const int32 EdgeId : OwnerMesh.EdgeIndicesItr())
		{
			const auto EdgeTriangles = OwnerMesh.GetEdgeT(EdgeId);
			if (EdgeTriangles.A >= 0 && EdgeTriangles.B >= 0)
			{
				continue;
			}

			const auto EdgeVertices = OwnerMesh.GetEdgeV(EdgeId);
			const FVector LocalPointA = OwnerDynamicMeshRelativeTransform.TransformPosition(FVector(OwnerMesh.GetVertex(EdgeVertices.A)));
			const FVector LocalPointB = OwnerDynamicMeshRelativeTransform.TransformPosition(FVector(OwnerMesh.GetVertex(EdgeVertices.B)));
			AppendGridWireSegment(GridMesh, LocalPointA, LocalPointB, LineColor, LineThickness);
			bAppendedAnyEdge = true;
		}
	});

	return bAppendedAnyEdge;
}

void USRPlanetSurfaceGrid::AppendGridWireCell(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FSRPlanetSurfaceGridCell& Cell,
	const FLinearColor& LineColor,
	float LineThickness,
	bool bIncludeInEdgeSet,
	TSet<uint64>* DrawnEdges) const
{
	auto AppendEdge = [this, &GridMesh, &LineColor, LineThickness, bIncludeInEdgeSet, DrawnEdges](
		const FVector& CornerA,
		const FVector& CornerB)
	{
		if (bIncludeInEdgeSet && DrawnEdges)
		{
			const uint64 EdgeKey = BuildGridEdgeKey(CornerA, CornerB);
			if (DrawnEdges->Contains(EdgeKey))
			{
				return;
			}
			DrawnEdges->Add(EdgeKey);
		}

		AppendGridWireEdge(GridMesh, CornerA, CornerB, LineColor, LineThickness);
	};

	AppendEdge(Cell.Corner00, Cell.Corner10);
	AppendEdge(Cell.Corner10, Cell.Corner11);
	AppendEdge(Cell.Corner11, Cell.Corner01);
	AppendEdge(Cell.Corner01, Cell.Corner00);
}

void USRPlanetSurfaceGrid::AppendGridWireEdge(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FVector& LocalDirectionA,
	const FVector& LocalDirectionB,
	const FLinearColor& LineColor,
	float LineThickness) const
{
	const FVector DirectionA = LocalDirectionA.GetSafeNormal();
	const FVector DirectionB = LocalDirectionB.GetSafeNormal();
	if (DirectionA.IsNearlyZero() || DirectionB.IsNearlyZero())
	{
		return;
	}

	constexpr int32 SegmentCount = 8;
	FVector PreviousPoint = ResolveLocalSurfacePoint(DirectionA, GridSurfaceOffset);
	for (int32 SegmentIndex = 1; SegmentIndex <= SegmentCount; ++SegmentIndex)
	{
		const float Alpha = static_cast<float>(SegmentIndex) / static_cast<float>(SegmentCount);
		const FVector SampleDirection = FMath::Lerp(DirectionA, DirectionB, Alpha).GetSafeNormal();
		if (SampleDirection.IsNearlyZero())
		{
			continue;
		}

		const FVector CurrentPoint = ResolveLocalSurfacePoint(SampleDirection, GridSurfaceOffset);
		AppendGridWireSegment(GridMesh, PreviousPoint, CurrentPoint, LineColor, LineThickness);
		PreviousPoint = CurrentPoint;
	}
}

void USRPlanetSurfaceGrid::AppendGridWireSegment(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FVector& LocalPointA,
	const FVector& LocalPointB,
	const FLinearColor& LineColor,
	float LineThickness) const
{
	const FVector SegmentDirection = (LocalPointB - LocalPointA).GetSafeNormal();
	if (SegmentDirection.IsNearlyZero())
	{
		return;
	}

	const FVector Midpoint = (LocalPointA + LocalPointB) * 0.5f;
	FVector SurfaceNormal = Midpoint.GetSafeNormal();
	if (SurfaceNormal.IsNearlyZero())
	{
		SurfaceNormal = FVector::UpVector;
	}

	FVector SideDirection = FVector::CrossProduct(SurfaceNormal, SegmentDirection).GetSafeNormal();
	if (SideDirection.IsNearlyZero())
	{
		SideDirection = FVector::CrossProduct(FVector::UpVector, SegmentDirection).GetSafeNormal();
	}
	if (SideDirection.IsNearlyZero())
	{
		return;
	}

	const float HalfThickness = FMath::Max(0.01f, LineThickness) * 0.5f;
	const FVector Offset = SideDirection * HalfThickness;
	const int32 Vertex0 = GridMesh.AppendVertex(FVector3d(LocalPointA - Offset));
	const int32 Vertex1 = GridMesh.AppendVertex(FVector3d(LocalPointA + Offset));
	const int32 Vertex2 = GridMesh.AppendVertex(FVector3d(LocalPointB + Offset));
	const int32 Vertex3 = GridMesh.AppendVertex(FVector3d(LocalPointB - Offset));

	const int32 Triangle0 = GridMesh.AppendTriangle(Vertex0, Vertex1, Vertex2);
	const int32 Triangle1 = GridMesh.AppendTriangle(Vertex0, Vertex2, Vertex3);

	UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = GridMesh.Attributes()->PrimaryNormals();
	auto* ColorOverlay = GridMesh.Attributes()->PrimaryColors();
	if (!NormalOverlay || !ColorOverlay)
	{
		return;
	}

	const int32 Normal0 = NormalOverlay->AppendElement(FVector3f(SurfaceNormal));
	const int32 Normal1 = NormalOverlay->AppendElement(FVector3f(SurfaceNormal));
	const int32 Normal2 = NormalOverlay->AppendElement(FVector3f(SurfaceNormal));
	const int32 Normal3 = NormalOverlay->AppendElement(FVector3f(SurfaceNormal));
	const int32 Color0 = ColorOverlay->AppendElement(FVector4f(LineColor.R, LineColor.G, LineColor.B, LineColor.A));
	const int32 Color1 = ColorOverlay->AppendElement(FVector4f(LineColor.R, LineColor.G, LineColor.B, LineColor.A));
	const int32 Color2 = ColorOverlay->AppendElement(FVector4f(LineColor.R, LineColor.G, LineColor.B, LineColor.A));
	const int32 Color3 = ColorOverlay->AppendElement(FVector4f(LineColor.R, LineColor.G, LineColor.B, LineColor.A));

	if (Triangle0 >= 0)
	{
		NormalOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Normal0, Normal1, Normal2));
		ColorOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Color0, Color1, Color2));
	}
	if (Triangle1 >= 0)
	{
		NormalOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Normal0, Normal2, Normal3));
		ColorOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Color0, Color2, Color3));
	}
}

void USRPlanetSurfaceGrid::ApplyGridMaterial()
{
	if (IsValid(GridMaterial))
	{
		SetMaterial(0, GridMaterial);
		return;
	}

	if (UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface))
	{
		SetMaterial(0, DefaultMaterial);
	}
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
		DrawDebugLine(World, PreviousPoint, CurrentPoint, LineColor, false, Duration, SDPG_Foreground, FMath::Max(0.0f, ScreenSpaceThickness));
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
	const FVector LocalSurfaceNormal = TerrainSettings.bUseProceduralTerrain
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
	return GetTerrainSampleAtDirection(LocalUnitDirection).HeightOffset;
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
		+ (TerrainSettings.bUseProceduralTerrain ? FMath::Max(0.0f, TerrainSettings.TerrainHeight) * GetComponentTransform().GetScale3D().GetAbsMax() : 0.0f);
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
