#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	FString FormatEnergyValue(double Value)
	{
		const double AbsValue = FMath::Abs(Value);
		if (AbsValue >= 1000.0)
		{
			return FString::Printf(TEXT("%.0f"), Value);
		}
		if (AbsValue >= 100.0)
		{
			return FString::Printf(TEXT("%.1f"), Value);
		}
		return FString::Printf(TEXT("%.2f"), Value);
	}

	FString GetCatalystOperatorText(ESRResourceCatalystOperator CatalystOperator)
	{
		switch (CatalystOperator)
		{
		case ESRResourceCatalystOperator::Add:
			return TEXT("+");
		case ESRResourceCatalystOperator::Multiply:
			return TEXT("*");
		case ESRResourceCatalystOperator::Subtract:
			return TEXT("-");
		case ESRResourceCatalystOperator::Divide:
			return TEXT("/");
		default:
			return TEXT("?");
		}
	}

	bool ResolveOutwardNormal(
		const USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellInfo& CellInfo,
		FVector& OutNormal)
	{
		if (!IsValid(SurfaceGrid))
		{
			OutNormal = FVector::UpVector;
			return false;
		}

		const FVector SurfaceCenter = SurfaceGrid->GetComponentTransform().GetLocation();
		OutNormal = CellInfo.WorldNormal.GetSafeNormal();
		if (OutNormal.IsNearlyZero())
		{
			OutNormal = (CellInfo.WorldCenter - SurfaceCenter).GetSafeNormal();
		}
		else if (FVector::DotProduct(OutNormal, CellInfo.WorldCenter - SurfaceCenter) < 0.0f)
		{
			OutNormal *= -1.0f;
		}

		if (OutNormal.IsNearlyZero())
		{
			OutNormal = FVector::UpVector;
		}
		return true;
	}
}

void USRConveyorNetworkComponent::RefreshConveyorItemVisuals(USRPlanetSurfaceGrid* SurfaceGrid, float /*DeltaTime*/)
{
	if (!IsValid(SurfaceGrid) || ConveyorItemsByLane.IsEmpty())
	{
		DestroyConveyorItemVisuals();
		return;
	}

	TSet<FSRConveyorLaneKey> ActiveLaneKeys;
	ActiveLaneKeys.Reserve(ConveyorItemsByLane.Num());

	const float TimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	for (const TPair<FSRConveyorLaneKey, FSRConveyorItem>& ItemPair : ConveyorItemsByLane)
	{
		const FSRConveyorLaneKey& LaneKey = ItemPair.Key;
		const FSRConveyorItem& Item = ItemPair.Value;

		FVector WorldLocation = FVector::ZeroVector;
		FVector WorldNormal = FVector::UpVector;
		if (!ResolveConveyorItemWorldLocation(SurfaceGrid, Item, WorldLocation, WorldNormal))
		{
			continue;
		}

		UTextRenderComponent* LabelComponent = EnsureConveyorItemLabelComponent(LaneKey);
		if (!IsValid(LabelComponent))
		{
			continue;
		}

		ActiveLaneKeys.Add(LaneKey);
		const double EnergyMagnitude = Item.ResourceInstance.ResourceKind == ESRResourceKind::Energy
			? FMath::Abs(Item.ResourceInstance.EnergyValue)
			: 0.0;
		const float EnergyScale = FMath::Clamp(
			1.0f + static_cast<float>(FMath::Loge(1.0 + EnergyMagnitude)) * 0.18f,
			1.0f,
			FMath::Max(1.0f, ItemEnergyLabelMaxScale));
		const float PulseStrength = FMath::Clamp((EnergyScale - 1.0f) / FMath::Max(0.01f, ItemEnergyLabelMaxScale - 1.0f), 0.0f, 1.0f);
		const float LanePhase = static_cast<float>(GetTypeHash(LaneKey) % 97) * 0.17f;
		const float PulseScale = 1.0f + FMath::Sin(TimeSeconds * 6.0f + LanePhase) * 0.08f * PulseStrength;

		FRotator LabelRotation = WorldNormal.Rotation();
		if (UWorld* World = GetWorld())
		{
			if (APlayerController* PlayerController = World->GetFirstPlayerController())
			{
				FRotator CameraRotation = FRotator::ZeroRotator;
				FVector UnusedCameraLocation = FVector::ZeroVector;
				PlayerController->GetPlayerViewPoint(UnusedCameraLocation, CameraRotation);
				FVector CameraFacingNormal = -CameraRotation.Vector();
				FVector CameraUp = CameraRotation.RotateVector(FVector::UpVector).GetSafeNormal();
				if (!CameraFacingNormal.IsNearlyZero() && !CameraUp.IsNearlyZero())
				{
					LabelRotation = FRotationMatrix::MakeFromXZ(CameraFacingNormal, CameraUp).Rotator();
				}
			}
		}

		LabelComponent->SetText(BuildConveyorItemLabelText(Item.ResourceInstance));
		LabelComponent->SetTextRenderColor(ResolveConveyorItemLabelColor(Item.ResourceInstance));
		LabelComponent->SetWorldSize(FMath::Max(1.0f, ItemEnergyLabelWorldSize * EnergyScale * PulseScale));
		LabelComponent->SetWorldLocationAndRotation(WorldLocation, LabelRotation);
		LabelComponent->SetVisibility(true);
		LabelComponent->SetHiddenInGame(false);
	}

	TArray<FSRConveyorLaneKey> ExistingLabelLaneKeys;
	ConveyorItemLabelsByLane.GetKeys(ExistingLabelLaneKeys);
	for (const FSRConveyorLaneKey& ExistingLaneKey : ExistingLabelLaneKeys)
	{
		if (ActiveLaneKeys.Contains(ExistingLaneKey))
		{
			continue;
		}

		if (TObjectPtr<UTextRenderComponent>* LabelComponentPtr = ConveyorItemLabelsByLane.Find(ExistingLaneKey))
		{
			if (IsValid(*LabelComponentPtr))
			{
				(*LabelComponentPtr)->DestroyComponent();
			}
		}
		ConveyorItemLabelsByLane.Remove(ExistingLaneKey);
	}
}

void USRConveyorNetworkComponent::DestroyConveyorItemVisuals()
{
	for (const TPair<FSRConveyorLaneKey, TObjectPtr<UTextRenderComponent>>& LabelPair : ConveyorItemLabelsByLane)
	{
		if (IsValid(LabelPair.Value))
		{
			LabelPair.Value->DestroyComponent();
		}
	}
	ConveyorItemLabelsByLane.Reset();
}

UTextRenderComponent* USRConveyorNetworkComponent::EnsureConveyorItemLabelComponent(const FSRConveyorLaneKey& LaneKey)
{
	if (TObjectPtr<UTextRenderComponent>* ExistingComponent = ConveyorItemLabelsByLane.Find(LaneKey))
	{
		if (IsValid(*ExistingComponent))
		{
			return *ExistingComponent;
		}
		ConveyorItemLabelsByLane.Remove(LaneKey);
	}

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return nullptr;
	}

	const FName ComponentName = MakeUniqueObjectName(
		OwnerActor,
		UTextRenderComponent::StaticClass(),
		FName(TEXT("ConveyorItemEnergyLabel")));
	UTextRenderComponent* LabelComponent = NewObject<UTextRenderComponent>(OwnerActor, ComponentName);
	if (!IsValid(LabelComponent))
	{
		return nullptr;
	}

	LabelComponent->SetupAttachment(this);
	LabelComponent->SetMobility(EComponentMobility::Movable);
	LabelComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LabelComponent->SetGenerateOverlapEvents(false);
	LabelComponent->SetCastShadow(false);
	LabelComponent->SetHorizontalAlignment(EHTA_Center);
	LabelComponent->SetVerticalAlignment(EVRTA_TextCenter);
	LabelComponent->SetWorldSize(FMath::Max(1.0f, ItemEnergyLabelWorldSize));
	LabelComponent->SetTextRenderColor(ItemEnergyLowColor.ToFColor(true));
	LabelComponent->SetText(FText::FromString(TEXT("E 0.00")));
	LabelComponent->SetVisibility(true);
	LabelComponent->SetHiddenInGame(false);
	LabelComponent->SetTranslucentSortPriority(100);
	LabelComponent->ComponentTags.AddUnique(TEXT("StarRovers.ConveyorItemEnergyLabel"));

	OwnerActor->AddInstanceComponent(LabelComponent);
	LabelComponent->RegisterComponent();
	ConveyorItemLabelsByLane.Add(LaneKey, LabelComponent);
	return LabelComponent;
}

bool USRConveyorNetworkComponent::ResolveConveyorItemWorldLocation(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorItem& Item,
	FVector& OutWorldLocation,
	FVector& OutWorldNormal) const
{
	OutWorldLocation = FVector::ZeroVector;
	OutWorldNormal = FVector::UpVector;
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	const FSRConveyorLaneKey LaneKey = Item.CurrentLane;
	FSRPlanetSurfaceGridCellInfo CurrentCellInfo;
	if (!SurfaceGrid->GetCellInfoById(LaneKey.CellId, CurrentCellInfo))
	{
		return false;
	}

	float LayerHeight = DefaultLayerHeight;
	for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
	{
		if (VisualPath.Layer == LaneKey.Layer && VisualPath.CellIds.Contains(LaneKey.CellId))
		{
			LayerHeight = VisualPath.LayerHeight;
			break;
		}
	}

	FVector CurrentNormal = FVector::UpVector;
	ResolveOutwardNormal(SurfaceGrid, CurrentCellInfo, CurrentNormal);
	const float HeightOffset = static_cast<float>(FMath::Max(0, LaneKey.Layer)) * FMath::Max(0.0f, LayerHeight)
		+ FMath::Max(0.0f, BeltSurfaceOffset)
		+ FMath::Max(0.0f, ItemVisualHeightOffset);
	const FVector CurrentPoint = CurrentCellInfo.WorldCenter + CurrentNormal * HeightOffset;

	FVector StartPoint = CurrentPoint;
	FVector EndPoint = CurrentPoint;
	FVector EndNormal = CurrentNormal;
	if (const FSRConveyorSegment* Segment = Segments.Find(LaneKey))
	{
		FSRConveyorLaneKey NextLaneKey;
		TArray<ESRConveyorGridDirection> OutputDirections;
		CollectConveyorOutputDirections(*Segment, OutputDirections);
		const int32 OutputDirectionIndex = OutputDirections.IsValidIndex(Segment->NextOutputDirectionIndex)
			? Segment->NextOutputDirectionIndex
			: 0;
		const ESRConveyorGridDirection VisualOutputDirection = OutputDirections.IsValidIndex(OutputDirectionIndex)
			? OutputDirections[OutputDirectionIndex]
			: ESRConveyorGridDirection::None;
		if (TryResolveNextLaneByDirection(SurfaceGrid, *Segment, VisualOutputDirection, NextLaneKey) && Segments.Contains(NextLaneKey))
		{
			FSRPlanetSurfaceGridCellInfo NextCellInfo;
			if (SurfaceGrid->GetCellInfoById(NextLaneKey.CellId, NextCellInfo))
			{
				FVector NextNormal = FVector::UpVector;
				ResolveOutwardNormal(SurfaceGrid, NextCellInfo, NextNormal);
				StartPoint = CurrentPoint;
				EndPoint = NextCellInfo.WorldCenter + NextNormal * HeightOffset;
				EndNormal = NextNormal;
			}
		}
		else if (Segment->InputDirection != ESRConveyorGridDirection::None)
		{
			FSRPlanetSurfaceGridCellNeighbors Neighbors;
			FSRPlanetSurfaceGridCellId PreviousCellId;
			if (SurfaceGrid->GetCellNeighbors(LaneKey.CellId, Neighbors)
				&& GetNeighborCellIdByDirection(Neighbors, Segment->InputDirection, PreviousCellId))
			{
				FSRPlanetSurfaceGridCellInfo PreviousCellInfo;
				if (SurfaceGrid->GetCellInfoById(PreviousCellId, PreviousCellInfo))
				{
					FVector PreviousNormal = FVector::UpVector;
					ResolveOutwardNormal(SurfaceGrid, PreviousCellInfo, PreviousNormal);
					const FVector PreviousPoint = PreviousCellInfo.WorldCenter + PreviousNormal * HeightOffset;
					const FVector TravelDirection = (CurrentPoint - PreviousPoint).GetSafeNormal();
					if (!TravelDirection.IsNearlyZero())
					{
						const float HalfCellTravelDistance = FVector::Distance(CurrentPoint, PreviousPoint) * 0.35f;
						StartPoint = CurrentPoint - TravelDirection * HalfCellTravelDistance;
						EndPoint = CurrentPoint + TravelDirection * HalfCellTravelDistance;
					}
				}
			}
		}
	}

	const float Alpha = FMath::Clamp(Item.Progress, 0.0f, 1.0f);
	OutWorldLocation = FMath::Lerp(StartPoint, EndPoint, Alpha);
	OutWorldNormal = FMath::Lerp(CurrentNormal, EndNormal, Alpha).GetSafeNormal();
	if (OutWorldNormal.IsNearlyZero())
	{
		OutWorldNormal = CurrentNormal;
	}
	return true;
}

FText USRConveyorNetworkComponent::BuildConveyorItemLabelText(const FSRResourceInstance& ResourceInstance) const
{
	if (ResourceInstance.ResourceKind == ESRResourceKind::Energy)
	{
		return FText::FromString(FString::Printf(TEXT("E %s"), *FormatEnergyValue(ResourceInstance.EnergyValue)));
	}

	return FText::FromString(FString::Printf(TEXT("CAT %s"), *GetCatalystOperatorText(ResourceInstance.CatalystOperator)));
}

FColor USRConveyorNetworkComponent::ResolveConveyorItemLabelColor(const FSRResourceInstance& ResourceInstance) const
{
	if (ResourceInstance.ResourceKind != ESRResourceKind::Energy)
	{
		return FLinearColor(0.25f, 1.0f, 0.8f, 1.0f).ToFColor(true);
	}

	if (ResourceInstance.EnergyValue < 0.0)
	{
		return ItemEnergyNegativeColor.ToFColor(true);
	}

	const double EnergyMagnitude = FMath::Abs(ResourceInstance.EnergyValue);
	const float Alpha = FMath::Clamp(static_cast<float>(FMath::Loge(1.0 + EnergyMagnitude) / FMath::Loge(101.0)), 0.0f, 1.0f);
	return FLinearColor::LerpUsingHSV(ItemEnergyLowColor, ItemEnergyHighColor, Alpha).ToFColor(true);
}
