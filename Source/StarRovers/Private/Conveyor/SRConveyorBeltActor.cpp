#include "Conveyor/SRConveyorBeltActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "PCGComponent.h"
#include "Helpers/PCGHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	const FName ConveyorBeltSplineNameBase(TEXT("ConveyorVisualSpline"));
	constexpr float ConveyorPCGBoundsDefaultExtent = 100.0f;
	constexpr float ConveyorPCGBoundsPadding = 500.0f;
}

ASRConveyorBeltActor::ASRConveyorBeltActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PCGComponent = CreateDefaultSubobject<UPCGComponent>(TEXT("PCGComponent"));

	PCGBoundsComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("PCGBoundsComponent"));
	PCGBoundsComponent->SetupAttachment(SceneRoot);
	PCGBoundsComponent->InitBoxExtent(FVector(ConveyorPCGBoundsDefaultExtent));
	PCGBoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PCGBoundsComponent->SetGenerateOverlapEvents(false);
	PCGBoundsComponent->SetCanEverAffectNavigation(false);
	PCGBoundsComponent->SetVisibility(false);
	PCGBoundsComponent->SetHiddenInGame(true);
	PCGBoundsComponent->ComponentTags.AddUnique(TEXT("StarRovers.ConveyorPCGBounds"));

	bAutoGeneratePCG = true;
	bRebaseGeneratedSplineMeshes = true;
	ConveyorSplineComponentTag = TEXT("ConveyorVisualSpline");
	ConveyorSurfaceOffset = 0.0f;
	bConveyorGhostMode = false;
	ConveyorGhostMaterial = nullptr;
	bConveyorGhostGenerationPending = false;
}

void ASRConveyorBeltActor::BeginPlay()
{
	Super::BeginPlay();

	BindPCGGenerationDelegate();
}

bool ASRConveyorBeltActor::InitializeConveyorPath(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorVisualPath& VisualPath,
	FName SplineComponentTag,
	float SurfaceOffset)
{
	TArray<FSRConveyorVisualPath> SingleVisualPath;
	SingleVisualPath.Add(VisualPath);
	return InitializeConveyorPaths(SurfaceGrid, SingleVisualPath, SplineComponentTag, SurfaceOffset);
}

bool ASRConveyorBeltActor::InitializeConveyorPaths(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRConveyorVisualPath>& VisualPaths,
	FName SplineComponentTag,
	float SurfaceOffset)
{
	if (!IsValid(SurfaceGrid) || VisualPaths.IsEmpty())
	{
		return false;
	}

	ConveyorVisualPath = FSRConveyorVisualPath();
	ConveyorVisualPaths.Reset();
	for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
	{
		if (!VisualPath.CellIds.IsEmpty())
		{
			ConveyorVisualPaths.Add(VisualPath);
		}
	}

	if (ConveyorVisualPaths.IsEmpty())
	{
		return false;
	}

	ConveyorVisualPath = ConveyorVisualPaths[0];
	ConveyorSplineComponentTag = SplineComponentTag.IsNone() ? FName(TEXT("ConveyorVisualSpline")) : SplineComponentTag;
	ConveyorSurfaceOffset = FMath::Max(0.0f, SurfaceOffset);

	TArray<FVector> WorldPoints;
	TArray<FVector> WorldNormals;
	FBox ConveyorWorldBounds(EForceInit::ForceInit);
	int32 UsedSplineCount = 0;
	for (const FSRConveyorVisualPath& VisualPath : ConveyorVisualPaths)
	{
		if (!BuildConveyorPathPoints(SurfaceGrid, VisualPath, WorldPoints, WorldNormals))
		{
			continue;
		}

		for (const FVector& WorldPoint : WorldPoints)
		{
			ConveyorWorldBounds += WorldPoint;
		}

		for (int32 SegmentIndex = 0; SegmentIndex + 1 < WorldPoints.Num(); ++SegmentIndex)
		{
			const FVector SegmentStart = WorldPoints[SegmentIndex];
			const FVector SegmentEnd = WorldPoints[SegmentIndex + 1];
			const FVector SegmentVector = SegmentEnd - SegmentStart;
			if (SegmentVector.SizeSquared() <= FMath::Square(KINDA_SMALL_NUMBER))
			{
				continue;
			}

			USplineComponent* SplineComponent = EnsureConveyorSplineComponent(UsedSplineCount);
			if (!IsValid(SplineComponent))
			{
				continue;
			}

			const FTransform SplineTransform = SplineComponent->GetComponentTransform();
			const FVector LocalSegmentStart = SplineTransform.InverseTransformPosition(SegmentStart);
			const FVector LocalSegmentEnd = SplineTransform.InverseTransformPosition(SegmentEnd);
			const FVector LocalSegmentVector = SplineTransform.InverseTransformVectorNoScale(SegmentVector);
			const FVector LocalStartNormal = SplineTransform.InverseTransformVectorNoScale(WorldNormals[SegmentIndex].GetSafeNormal()).GetSafeNormal();
			const FVector LocalEndNormal = SplineTransform.InverseTransformVectorNoScale(WorldNormals[SegmentIndex + 1].GetSafeNormal()).GetSafeNormal();

			SplineComponent->ClearSplinePoints(false);
			SplineComponent->ComponentTags.Reset();
			SplineComponent->ComponentTags.AddUnique(ConveyorSplineComponentTag);
			SplineComponent->ComponentTags.AddUnique(TEXT("StarRovers.ConveyorBeltSpline"));
			SplineComponent->AddSplinePoint(LocalSegmentStart, ESplineCoordinateSpace::Local, false);
			SplineComponent->AddSplinePoint(LocalSegmentEnd, ESplineCoordinateSpace::Local, false);
			SplineComponent->SetSplinePointType(0, ESplinePointType::Linear, false);
			SplineComponent->SetSplinePointType(1, ESplinePointType::Linear, false);
			SplineComponent->SetTangentAtSplinePoint(0, LocalSegmentVector, ESplineCoordinateSpace::Local, false);
			SplineComponent->SetTangentAtSplinePoint(1, LocalSegmentVector, ESplineCoordinateSpace::Local, false);
			SplineComponent->SetUpVectorAtSplinePoint(0, LocalStartNormal, ESplineCoordinateSpace::Local, false);
			SplineComponent->SetUpVectorAtSplinePoint(1, LocalEndNormal, ESplineCoordinateSpace::Local, false);
			SplineComponent->SetClosedLoop(false, false);
			SplineComponent->SetVisibility(false);
			SplineComponent->SetHiddenInGame(true);
			SplineComponent->UpdateSpline();
			++UsedSplineCount;
		}
	}

	ClearUnusedConveyorSplineComponents(UsedSplineCount);
	if (UsedSplineCount <= 0)
	{
		return false;
	}

	UpdatePCGBoundsFromWorldBounds(ConveyorWorldBounds);
	BindPCGGenerationDelegate();
	if (bConveyorGhostMode && HasReusableGeneratedSplineMeshes(UsedSplineCount))
	{
		bConveyorGhostGenerationPending = false;
		RebaseGeneratedSplineMeshes();
		return true;
	}

	if (bConveyorGhostMode)
	{
		bConveyorGhostGenerationPending = true;
		SetActorHiddenInGame(true);
		HideGeneratedSplineMeshes();
	}

	RequestPCGGeneration();
	return true;
}

void ASRConveyorBeltActor::SetConveyorGhostMode(bool bNewGhostMode, UMaterialInterface* InGhostMaterial)
{
	bConveyorGhostMode = bNewGhostMode;
	ConveyorGhostMaterial = InGhostMaterial;
	SetActorEnableCollision(!bConveyorGhostMode);
	if (!bConveyorGhostMode)
	{
		bConveyorGhostGenerationPending = false;
	}
	ApplyConveyorGhostModeToGeneratedMeshes();
}

bool ASRConveyorBeltActor::IsConveyorGhostGenerationPending() const
{
	return bConveyorGhostGenerationPending;
}

USplineComponent* ASRConveyorBeltActor::EnsureConveyorSplineComponent(int32 SplineIndex)
{
	if (SplineIndex < 0)
	{
		return nullptr;
	}

	while (ConveyorSplineComponents.Num() <= SplineIndex)
	{
		ConveyorSplineComponents.Add(nullptr);
	}

	if (IsValid(ConveyorSplineComponents[SplineIndex]))
	{
		return ConveyorSplineComponents[SplineIndex];
	}

	const FName RequestedName(*FString::Printf(TEXT("%s_%d"), *ConveyorBeltSplineNameBase.ToString(), SplineIndex));
	const FName ComponentName = MakeUniqueObjectName(this, USplineComponent::StaticClass(), RequestedName);
	USplineComponent* SplineComponent = NewObject<USplineComponent>(this, ComponentName);
	if (!IsValid(SplineComponent))
	{
		return nullptr;
	}

	SplineComponent->SetupAttachment(SceneRoot);
	SplineComponent->SetRelativeTransform(FTransform::Identity);
	SplineComponent->SetMobility(EComponentMobility::Movable);
	SplineComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SplineComponent->SetGenerateOverlapEvents(false);
	SplineComponent->SetVisibility(false);
	SplineComponent->SetHiddenInGame(true);
	SplineComponent->ComponentTags.AddUnique(ConveyorSplineComponentTag);
	SplineComponent->ComponentTags.AddUnique(TEXT("StarRovers.ConveyorBeltSpline"));
	AddInstanceComponent(SplineComponent);
	SplineComponent->RegisterComponent();
	ConveyorSplineComponents[SplineIndex] = SplineComponent;
	return SplineComponent;
}

void ASRConveyorBeltActor::ClearUnusedConveyorSplineComponents(int32 FirstUnusedSplineIndex)
{
	for (int32 SplineIndex = FMath::Max(0, FirstUnusedSplineIndex); SplineIndex < ConveyorSplineComponents.Num(); ++SplineIndex)
	{
		USplineComponent* SplineComponent = ConveyorSplineComponents[SplineIndex];
		if (!IsValid(SplineComponent))
		{
			continue;
		}

		SplineComponent->ClearSplinePoints(false);
		SplineComponent->SetVisibility(false);
		SplineComponent->SetHiddenInGame(true);
		SplineComponent->UpdateSpline();
	}
}

bool ASRConveyorBeltActor::BuildConveyorPathPoints(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorVisualPath& VisualPath,
	TArray<FVector>& OutWorldPoints,
	TArray<FVector>& OutWorldNormals) const
{
	OutWorldPoints.Reset();
	OutWorldNormals.Reset();
	if (!IsValid(SurfaceGrid) || VisualPath.CellIds.IsEmpty())
	{
		return false;
	}

	const FVector PlanetCenter = SurfaceGrid->GetComponentTransform().GetLocation();
	const float LayerOffset = static_cast<float>(FMath::Max(0, VisualPath.Layer)) * FMath::Max(0.0f, VisualPath.LayerHeight);
	const float HeightOffset = LayerOffset + ConveyorSurfaceOffset;
	OutWorldPoints.Reserve(VisualPath.CellIds.Num());
	OutWorldNormals.Reserve(VisualPath.CellIds.Num());

	for (const FSRPlanetSurfaceGridCellId& CellId : VisualPath.CellIds)
	{
		FSRPlanetSurfaceGridCellInfo CellInfo;
		if (!SurfaceGrid->GetCellInfoById(CellId, CellInfo))
		{
			continue;
		}

		FVector OutwardNormal = CellInfo.WorldNormal.GetSafeNormal();
		if (OutwardNormal.IsNearlyZero())
		{
			OutwardNormal = (CellInfo.WorldCenter - PlanetCenter).GetSafeNormal();
		}
		else if (FVector::DotProduct(OutwardNormal, CellInfo.WorldCenter - PlanetCenter) < 0.0f)
		{
			OutwardNormal *= -1.0f;
		}

		OutWorldPoints.Add(CellInfo.WorldCenter + OutwardNormal * HeightOffset);
		OutWorldNormals.Add(OutwardNormal);
	}

	if (OutWorldPoints.Num() == 1)
	{
		const FSRPlanetSurfaceGridCellId& CellId = VisualPath.CellIds[0];
		FSRPlanetSurfaceGridCell Cell;
		if (SurfaceGrid->GetCellById(CellId, Cell))
		{
			const FTransform SurfaceGridTransform = SurfaceGrid->GetComponentTransform();
			FVector SingleTangent = SurfaceGridTransform.TransformPosition(Cell.Corner10) - SurfaceGridTransform.TransformPosition(Cell.Corner00);
			SingleTangent = SingleTangent - OutWorldNormals[0] * FVector::DotProduct(SingleTangent, OutWorldNormals[0]);
			if (SingleTangent.Normalize())
			{
				const FVector CenterPoint = OutWorldPoints[0];
				const FVector CenterNormal = OutWorldNormals[0];
				const float CellEdgeLength = FVector::Distance(
					SurfaceGridTransform.TransformPosition(Cell.Corner00),
					SurfaceGridTransform.TransformPosition(Cell.Corner10));
				const float HalfLength = FMath::Max(1.0f, CellEdgeLength * 0.35f);
				OutWorldPoints.Reset();
				OutWorldNormals.Reset();
				OutWorldPoints.Add(CenterPoint - SingleTangent * HalfLength);
				OutWorldPoints.Add(CenterPoint + SingleTangent * HalfLength);
				OutWorldNormals.Add(CenterNormal);
				OutWorldNormals.Add(CenterNormal);
			}
		}
	}

	return OutWorldPoints.Num() >= 2 && OutWorldPoints.Num() == OutWorldNormals.Num();
}

void ASRConveyorBeltActor::UpdatePCGBoundsFromWorldBounds(const FBox& WorldBounds)
{
	if (!IsValid(PCGBoundsComponent))
	{
		return;
	}

	FBox LocalBounds(EForceInit::ForceInit);
	if (WorldBounds.IsValid)
	{
		const FTransform RootTransform = IsValid(SceneRoot)
			? SceneRoot->GetComponentTransform()
			: GetActorTransform();

		const FVector Min = WorldBounds.Min;
		const FVector Max = WorldBounds.Max;
		const FVector Corners[] =
		{
			FVector(Min.X, Min.Y, Min.Z),
			FVector(Min.X, Min.Y, Max.Z),
			FVector(Min.X, Max.Y, Min.Z),
			FVector(Min.X, Max.Y, Max.Z),
			FVector(Max.X, Min.Y, Min.Z),
			FVector(Max.X, Min.Y, Max.Z),
			FVector(Max.X, Max.Y, Min.Z),
			FVector(Max.X, Max.Y, Max.Z),
		};

		for (const FVector& Corner : Corners)
		{
			LocalBounds += RootTransform.InverseTransformPosition(Corner);
		}
	}

	if (!LocalBounds.IsValid)
	{
		LocalBounds = FBox::BuildAABB(FVector::ZeroVector, FVector(ConveyorPCGBoundsDefaultExtent));
	}

	FVector Extent = LocalBounds.GetExtent() + FVector(ConveyorPCGBoundsPadding);
	Extent.X = FMath::Max(ConveyorPCGBoundsDefaultExtent, Extent.X);
	Extent.Y = FMath::Max(ConveyorPCGBoundsDefaultExtent, Extent.Y);
	Extent.Z = FMath::Max(ConveyorPCGBoundsDefaultExtent, Extent.Z);

	PCGBoundsComponent->SetRelativeLocation(LocalBounds.GetCenter());
	PCGBoundsComponent->SetBoxExtent(Extent, false);
	PCGBoundsComponent->UpdateBounds();
}

void ASRConveyorBeltActor::BindPCGGenerationDelegate()
{
	if (!IsValid(PCGComponent))
	{
		return;
	}

	PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
	PCGComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
	PCGComponent->OnPCGGraphGeneratedDelegate.AddUObject(this, &ASRConveyorBeltActor::HandlePCGGraphGenerated);
}

void ASRConveyorBeltActor::RequestPCGGeneration()
{
	if (!bAutoGeneratePCG || !IsValid(PCGComponent))
	{
		return;
	}

	PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
	PCGComponent->DirtyGenerated(EPCGComponentDirtyFlag::All, /*bDispatchToLocalComponents=*/false);
	PCGComponent->Generate(true);
}

void ASRConveyorBeltActor::HandlePCGGraphGenerated(UPCGComponent* InPCGComponent)
{
	if (InPCGComponent == PCGComponent)
	{
		RebaseGeneratedSplineMeshes();
		if (bConveyorGhostGenerationPending)
		{
			bConveyorGhostGenerationPending = false;
			if (bConveyorGhostMode)
			{
				SetActorHiddenInGame(false);
			}
		}
	}
}

void ASRConveyorBeltActor::CollectGeneratedSplineMeshes(TArray<USplineMeshComponent*>& OutGeneratedSplineMeshes) const
{
	OutGeneratedSplineMeshes.Reset();
	if (!IsValid(PCGComponent))
	{
		return;
	}

	GetComponents<USplineMeshComponent>(OutGeneratedSplineMeshes);
	OutGeneratedSplineMeshes.RemoveAll([this](const USplineMeshComponent* SplineMeshComponent)
	{
		return !IsValid(SplineMeshComponent)
			|| SplineMeshComponent->ComponentTags.Contains(PCGHelpers::MarkedForCleanupPCGTag)
			|| (!SplineMeshComponent->ComponentTags.Contains(PCGComponent->GetFName())
				&& !SplineMeshComponent->ComponentTags.Contains(PCGHelpers::DefaultPCGTag));
	});
	OutGeneratedSplineMeshes.Sort([](const USplineMeshComponent& Left, const USplineMeshComponent& Right)
	{
		return Left.GetFName().LexicalLess(Right.GetFName());
	});
}

void ASRConveyorBeltActor::CollectAllGeneratedSplineMeshes(TArray<USplineMeshComponent*>& OutGeneratedSplineMeshes) const
{
	OutGeneratedSplineMeshes.Reset();
	if (!IsValid(PCGComponent))
	{
		return;
	}

	GetComponents<USplineMeshComponent>(OutGeneratedSplineMeshes);
	OutGeneratedSplineMeshes.RemoveAll([this](const USplineMeshComponent* SplineMeshComponent)
	{
		return !IsValid(SplineMeshComponent)
			|| (!SplineMeshComponent->ComponentTags.Contains(PCGComponent->GetFName())
				&& !SplineMeshComponent->ComponentTags.Contains(PCGHelpers::DefaultPCGTag));
	});
	OutGeneratedSplineMeshes.Sort([](const USplineMeshComponent& Left, const USplineMeshComponent& Right)
	{
		return Left.GetFName().LexicalLess(Right.GetFName());
	});
}

bool ASRConveyorBeltActor::HasReusableGeneratedSplineMeshes(int32 RequiredSplineMeshCount) const
{
	if (RequiredSplineMeshCount <= 0 || !IsValid(PCGComponent))
	{
		return false;
	}

	TArray<USplineMeshComponent*> GeneratedSplineMeshes;
	CollectGeneratedSplineMeshes(GeneratedSplineMeshes);
	return GeneratedSplineMeshes.Num() >= RequiredSplineMeshCount;
}

void ASRConveyorBeltActor::HideGeneratedSplineMeshes() const
{
	if (!IsValid(PCGComponent))
	{
		return;
	}

	TArray<USplineMeshComponent*> GeneratedSplineMeshes;
	CollectAllGeneratedSplineMeshes(GeneratedSplineMeshes);
	for (USplineMeshComponent* SplineMeshComponent : GeneratedSplineMeshes)
	{
		SplineMeshComponent->SetVisibility(false);
		SplineMeshComponent->SetHiddenInGame(true);
		SplineMeshComponent->UpdateMesh();
	}
}

void ASRConveyorBeltActor::RebaseGeneratedSplineMeshes()
{
	if (!bRebaseGeneratedSplineMeshes || !IsValid(PCGComponent))
	{
		return;
	}

	HideGeneratedSplineMeshes();

	TArray<USplineMeshComponent*> GeneratedSplineMeshes;
	CollectGeneratedSplineMeshes(GeneratedSplineMeshes);

	const int32 SegmentCount = FMath::Min(GeneratedSplineMeshes.Num(), ConveyorSplineComponents.Num());
	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		USplineMeshComponent* SplineMeshComponent = GeneratedSplineMeshes[SegmentIndex];
		const USplineComponent* SplineComponent = ConveyorSplineComponents[SegmentIndex];
		if (!IsValid(SplineMeshComponent) || !IsValid(SplineComponent) || SplineComponent->GetNumberOfSplinePoints() < 2)
		{
			continue;
		}

		const FVector Start = SplineComponent->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local);
		const FVector End = SplineComponent->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::Local);
		const FVector StartTangent = SplineComponent->GetTangentAtSplinePoint(0, ESplineCoordinateSpace::Local);
		const FVector EndTangent = SplineComponent->GetTangentAtSplinePoint(1, ESplineCoordinateSpace::Local);
		const FVector UpDirection = SplineComponent->GetUpVectorAtSplinePoint(0, ESplineCoordinateSpace::Local).GetSafeNormal();

		SplineMeshComponent->SetMobility(EComponentMobility::Movable);
		SplineMeshComponent->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepRelativeTransform);
		SplineMeshComponent->SetRelativeTransform(FTransform::Identity);
		SplineMeshComponent->SetSplineUpDir(UpDirection.IsNearlyZero() ? FVector::UpVector : UpDirection, false);
		SplineMeshComponent->SetStartAndEnd(Start, StartTangent, End, EndTangent, false);
		SplineMeshComponent->SetStartRollDegrees(0.0f, false);
		SplineMeshComponent->SetEndRollDegrees(0.0f, false);
		SplineMeshComponent->SetVisibility(true);
		SplineMeshComponent->SetHiddenInGame(false);
		SplineMeshComponent->ComponentTags.AddUnique(PCGComponent->GetFName());
		SplineMeshComponent->ComponentTags.AddUnique(PCGHelpers::DefaultPCGTag);
		ApplyConveyorGhostModeToSplineMesh(SplineMeshComponent);
		SplineMeshComponent->UpdateMesh();
	}

	for (int32 SegmentIndex = SegmentCount; SegmentIndex < GeneratedSplineMeshes.Num(); ++SegmentIndex)
	{
		USplineMeshComponent* SplineMeshComponent = GeneratedSplineMeshes[SegmentIndex];
		if (!IsValid(SplineMeshComponent))
		{
			continue;
		}

		SplineMeshComponent->SetVisibility(false);
		SplineMeshComponent->SetHiddenInGame(true);
	}
}

void ASRConveyorBeltActor::ApplyConveyorGhostModeToSplineMesh(USplineMeshComponent* SplineMeshComponent) const
{
	if (!IsValid(SplineMeshComponent) || !bConveyorGhostMode)
	{
		return;
	}

	SplineMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SplineMeshComponent->SetGenerateOverlapEvents(false);
	SplineMeshComponent->SetCastShadow(false);

	if (!IsValid(ConveyorGhostMaterial))
	{
		return;
	}

	const int32 MaterialSlotCount = FMath::Max(1, SplineMeshComponent->GetNumMaterials());
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
	{
		SplineMeshComponent->SetMaterial(MaterialIndex, ConveyorGhostMaterial);
	}
}

void ASRConveyorBeltActor::ApplyConveyorGhostModeToGeneratedMeshes() const
{
	if (!bConveyorGhostMode)
	{
		return;
	}

	TArray<USplineMeshComponent*> GeneratedSplineMeshes;
	CollectAllGeneratedSplineMeshes(GeneratedSplineMeshes);
	for (USplineMeshComponent* SplineMeshComponent : GeneratedSplineMeshes)
	{
		ApplyConveyorGhostModeToSplineMesh(SplineMeshComponent);
	}
}
