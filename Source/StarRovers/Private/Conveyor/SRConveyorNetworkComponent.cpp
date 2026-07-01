#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Components/DynamicMeshComponent.h"
#include "Components/LineBatchComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "GameFramework/Actor.h"
#include "PCGComponent.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Surface/SRPlanetSurfaceGrid.h"

USRConveyorNetworkComponent::USRConveyorNetworkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	DefaultLayerHeight = 160.0f;
	BeltWidth = 260.0f;
	BeltThickness = 80.0f;
	BeltSurfaceOffset = 0.0f;
	bBuildDynamicMeshVisuals = false;
	bSpawnConveyorBeltActors = true;
	MaxConveyorActorGroupsRefreshedPerFrame = 1;
	bBuildPCGSplineInputs = false;
	PCGSplineComponentTag = TEXT("ConveyorVisualSpline");
	PCGSplineHeightOffset = 0.0f;
	bAutoGeneratePCG = false;
	bShowPathDebugLine = false;
	PathDebugLineColor = FLinearColor(1.0f, 0.1f, 0.0f, 1.0f);
	PathDebugLineThickness = 8.0f;
	bShowConnectionDebugLine = false;
	ConnectionDebugLineColor = FLinearColor(0.1f, 1.0f, 0.25f, 1.0f);
	BrokenConnectionDebugLineColor = FLinearColor(1.0f, 0.15f, 0.05f, 1.0f);
	EndpointDebugLineColor = FLinearColor(1.0f, 0.9f, 0.1f, 1.0f);
	ConnectionDebugLineThickness = 6.0f;
	ConnectionDebugLineHeightOffset = 110.0f;
	bAutoTransportItems = true;
	ItemSpeedCellsPerSecond = 1.0f;
	MaxItemTransfersPerTick = 128;
	bShowTransportItemVisuals = true;
	ItemVisualHeightOffset = 180.0f;
	ItemEnergyLabelWorldSize = 120.0f;
	ItemEnergyLabelMaxScale = 2.5f;
	ItemEnergyLowColor = FLinearColor(0.1f, 0.75f, 1.0f, 1.0f);
	ItemEnergyHighColor = FLinearColor(1.0f, 0.55f, 0.05f, 1.0f);
	ItemEnergyNegativeColor = FLinearColor(0.85f, 0.1f, 1.0f, 1.0f);
	BeltMeshComponent = nullptr;
	PathDebugLineBatchComponent = nullptr;
}

void USRConveyorNetworkComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	TArray<UPCGComponent*> PCGComponents;
	OwnerActor->GetComponents<UPCGComponent>(PCGComponents);
	for (UPCGComponent* PCGComponent : PCGComponents)
	{
		if (IsValid(PCGComponent))
		{
			PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
		}
	}
	BindPCGGenerationDelegates();

	if (bShowPathDebugLine || bShowConnectionDebugLine)
	{
		USRPlanetSurfaceGrid* SurfaceGrid = OwnerActor->FindComponentByClass<USRPlanetSurfaceGrid>();
		RefreshPathDebugLines(SurfaceGrid);
		SetComponentTickEnabled(true);
	}
}

void USRConveyorNetworkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USRPlanetSurfaceGrid* SurfaceGrid = PendingConveyorActorRefreshSurfaceGrid.Get();
	if (!IsValid(SurfaceGrid))
	{
		if (AActor* OwnerActor = GetOwner())
		{
			SurfaceGrid = OwnerActor->FindComponentByClass<USRPlanetSurfaceGrid>();
			PendingConveyorActorRefreshSurfaceGrid = SurfaceGrid;
		}
	}

	float TransportDeltaTime = FMath::Max(0.0f, DeltaTime);
	if (const UWorld* World = GetWorld())
	{
		if (const USRTimeControlSubsystem* TimeControlSubsystem = World->GetSubsystem<USRTimeControlSubsystem>())
		{
			TransportDeltaTime *= FMath::Max(0.0f, TimeControlSubsystem->GetEffectiveTimeScale());
		}
	}

	if (bAutoTransportItems && IsValid(SurfaceGrid) && TransportDeltaTime > 0.0f)
	{
		ProcessConveyorTransport(SurfaceGrid, TransportDeltaTime);
	}

	if (bShowTransportItemVisuals && IsValid(SurfaceGrid))
	{
		RefreshConveyorItemVisuals(SurfaceGrid, DeltaTime);
	}
	else
	{
		DestroyConveyorItemVisuals();
	}

	if ((bShowPathDebugLine || bShowConnectionDebugLine) && IsValid(SurfaceGrid))
	{
		RefreshPathDebugLines(SurfaceGrid);
	}

	RefreshDirtyConveyorActorGroups(SurfaceGrid, FMath::Max(1, MaxConveyorActorGroupsRefreshedPerFrame));
	if (!HasDirtyConveyorActorGroups() && !ShouldKeepTransportTickEnabled() && !bShowPathDebugLine && !bShowConnectionDebugLine)
	{
		PendingConveyorActorRefreshSurfaceGrid.Reset();
		SetComponentTickEnabled(false);
	}
}

#if WITH_EDITOR
void USRConveyorNetworkComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (AActor* OwnerActor = GetOwner(); IsValid(OwnerActor) && OwnerActor->HasActorBegunPlay())
	{
		USRPlanetSurfaceGrid* SurfaceGrid = OwnerActor->FindComponentByClass<USRPlanetSurfaceGrid>();
		if (bSpawnConveyorBeltActors)
		{
			RebuildPlacedConveyorActors(SurfaceGrid);
		}
		else
		{
			DestroyPlacedConveyorActors();
		}
		RefreshConveyorVisuals(SurfaceGrid);
		RefreshPathDebugLines(SurfaceGrid);
		SetComponentTickEnabled(HasDirtyConveyorActorGroups() || ShouldKeepTransportTickEnabled() || bShowPathDebugLine || bShowConnectionDebugLine);
	}
}
#endif

bool USRConveyorNetworkComponent::HasConveyorSegment(const FSRConveyorLaneKey& LaneKey) const
{
	return Segments.Contains(LaneKey);
}

bool USRConveyorNetworkComponent::GetConveyorSegment(const FSRConveyorLaneKey& LaneKey, FSRConveyorSegment& OutSegment) const
{
	if (const FSRConveyorSegment* Segment = Segments.Find(LaneKey))
	{
		OutSegment = *Segment;
		return true;
	}

	OutSegment = FSRConveyorSegment();
	return false;
}

bool USRConveyorNetworkComponent::HasConveyorSegmentAtCell(const FSRPlanetSurfaceGridCellId& CellId) const
{
	for (const TPair<FSRConveyorLaneKey, FSRConveyorSegment>& SegmentPair : Segments)
	{
		if (SegmentPair.Key.CellId == CellId)
		{
			return true;
		}
	}
	return false;
}

void USRConveyorNetworkComponent::ClearConveyors()
{
	if (AActor* OwnerActor = GetOwner())
	{
		if (USRPlanetSurfaceGrid* SurfaceGrid = OwnerActor->FindComponentByClass<USRPlanetSurfaceGrid>())
		{
			TArray<FSRPlanetSurfaceGridCellId> SurfaceLayerCellIds;
			for (const TPair<FSRConveyorLaneKey, FSRConveyorSegment>& SegmentPair : Segments)
			{
				if (SegmentPair.Key.Layer == 0)
				{
					SurfaceLayerCellIds.Add(SegmentPair.Key.CellId);
				}
			}
			if (!SurfaceLayerCellIds.IsEmpty())
			{
				SurfaceGrid->SetCellsOccupied(SurfaceLayerCellIds, false, NAME_None);
			}
		}
	}

	Segments.Reset();
	VisualPaths.Reset();
	TransportState.ResetItems();
	DestroyConveyorItemVisuals();
	DestroyPlacedConveyorActors();
	PendingConveyorActorRefreshSurfaceGrid.Reset();
	SetComponentTickEnabled(false);
	if (IsValid(BeltMeshComponent))
	{
		UE::Geometry::FDynamicMesh3 EmptyMesh;
		EmptyMesh.EnableAttributes();
		EmptyMesh.Attributes()->EnablePrimaryColors();
		EmptyMesh.Attributes()->SetNumUVLayers(1);
		BeltMeshComponent->SetMesh(MoveTemp(EmptyMesh));
	}
	if (IsValid(PathDebugLineBatchComponent))
	{
		PathDebugLineBatchComponent->Flush();
	}
	ClearUnusedPCGSplineComponents(0);
}

void USRConveyorNetworkComponent::SetPathDebugLineVisible(bool bNewPathDebugLineVisible)
{
	if (bShowPathDebugLine == bNewPathDebugLineVisible)
	{
		return;
	}

	bShowPathDebugLine = bNewPathDebugLineVisible;
	if (AActor* OwnerActor = GetOwner())
	{
		RefreshPathDebugLines(OwnerActor->FindComponentByClass<USRPlanetSurfaceGrid>());
	}
	SetComponentTickEnabled(HasDirtyConveyorActorGroups() || ShouldKeepTransportTickEnabled() || bShowPathDebugLine || bShowConnectionDebugLine);
}

bool USRConveyorNetworkComponent::IsPathDebugLineVisible() const
{
	return bShowPathDebugLine;
}

void USRConveyorNetworkComponent::SetConnectionDebugLineVisible(bool bNewConnectionDebugLineVisible)
{
	if (bShowConnectionDebugLine == bNewConnectionDebugLineVisible)
	{
		return;
	}

	bShowConnectionDebugLine = bNewConnectionDebugLineVisible;
	if (AActor* OwnerActor = GetOwner())
	{
		RefreshPathDebugLines(OwnerActor->FindComponentByClass<USRPlanetSurfaceGrid>());
	}
	SetComponentTickEnabled(HasDirtyConveyorActorGroups() || ShouldKeepTransportTickEnabled() || bShowPathDebugLine || bShowConnectionDebugLine);
}

bool USRConveyorNetworkComponent::IsConnectionDebugLineVisible() const
{
	return bShowConnectionDebugLine;
}
