#include "Conveyor/SRConveyorItemLabelUpdater.h"

#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Surface/SRPlanetSurfaceGrid.h"

void StarRovers::Conveyor::FSRConveyorItemLabelUpdater::Refresh(
	AActor* OwnerActor,
	USceneComponent* AttachParent,
	UWorld* World,
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	const TArray<FSRConveyorBeltPath>& BeltPaths,
	const FSRConveyorItemLabelSettings& LabelSettings,
	FSRConveyorTransportRuntimeState& TransportState)
{
	if (!IsValid(SurfaceGrid) || !TransportState.HasItems())
	{
		Destroy(TransportState);
		return;
	}

	TSet<FSRConveyorLaneKey> ActiveLaneKeys;
	ActiveLaneKeys.Reserve(TransportState.ItemsByLane.Num());

	const float TimeSeconds = World ? World->GetTimeSeconds() : 0.0f;
	for (const TPair<FSRConveyorLaneKey, FSRConveyorItem>& ItemPair : TransportState.ItemsByLane)
	{
		const FSRConveyorLaneKey& LaneKey = ItemPair.Key;
		const FSRConveyorItem& Item = ItemPair.Value;

		FVector WorldLocation = FVector::ZeroVector;
		FVector WorldNormal = FVector::UpVector;
		if (!FSRConveyorItemLabelResolver::ResolveWorldLocation(
			SurfaceGrid,
			Segments,
			BeltPaths,
			Item,
			LabelSettings,
			WorldLocation,
			WorldNormal))
		{
			continue;
		}

		UTextRenderComponent* LabelComponent = EnsureLabelComponent(OwnerActor, AttachParent, LaneKey, LabelSettings, TransportState.ItemLabelsByLane);
		if (!IsValid(LabelComponent))
		{
			continue;
		}

		ActiveLaneKeys.Add(LaneKey);

		FRotator LabelRotation = WorldNormal.Rotation();
		if (World)
		{
			if (APlayerController* PlayerController = World->GetFirstPlayerController())
			{
				FRotator CameraRotation = FRotator::ZeroRotator;
				FVector UnusedCameraLocation = FVector::ZeroVector;
				PlayerController->GetPlayerViewPoint(UnusedCameraLocation, CameraRotation);
				const FVector CameraFacingNormal = -CameraRotation.Vector();
				const FVector CameraUp = CameraRotation.RotateVector(FVector::UpVector).GetSafeNormal();
				if (!CameraFacingNormal.IsNearlyZero() && !CameraUp.IsNearlyZero())
				{
					LabelRotation = FRotationMatrix::MakeFromXZ(CameraFacingNormal, CameraUp).Rotator();
				}
			}
		}

		LabelComponent->SetText(FSRConveyorItemLabelResolver::BuildLabelText(Item.ResourceInstance));
		LabelComponent->SetTextRenderColor(FSRConveyorItemLabelResolver::ResolveLabelColor(Item.ResourceInstance, LabelSettings));
		LabelComponent->SetWorldSize(FSRConveyorItemLabelResolver::ResolveLabelWorldSize(LaneKey, Item.ResourceInstance, TimeSeconds, LabelSettings));
		LabelComponent->SetWorldLocationAndRotation(WorldLocation, LabelRotation);
		LabelComponent->SetVisibility(true);
		LabelComponent->SetHiddenInGame(false);
	}

	TArray<FSRConveyorLaneKey> ExistingLabelLaneKeys;
	TransportState.ItemLabelsByLane.GetKeys(ExistingLabelLaneKeys);
	for (const FSRConveyorLaneKey& ExistingLaneKey : ExistingLabelLaneKeys)
	{
		if (ActiveLaneKeys.Contains(ExistingLaneKey))
		{
			continue;
		}

		if (TObjectPtr<UTextRenderComponent>* LabelComponentPtr = TransportState.ItemLabelsByLane.Find(ExistingLaneKey))
		{
			if (IsValid(*LabelComponentPtr))
			{
				(*LabelComponentPtr)->DestroyComponent();
			}
		}
		TransportState.ItemLabelsByLane.Remove(ExistingLaneKey);
	}
}

void StarRovers::Conveyor::FSRConveyorItemLabelUpdater::Destroy(FSRConveyorTransportRuntimeState& TransportState)
{
	for (const TPair<FSRConveyorLaneKey, TObjectPtr<UTextRenderComponent>>& LabelPair : TransportState.ItemLabelsByLane)
	{
		if (IsValid(LabelPair.Value))
		{
			LabelPair.Value->DestroyComponent();
		}
	}
	TransportState.ResetLabels();
}

UTextRenderComponent* StarRovers::Conveyor::FSRConveyorItemLabelUpdater::EnsureLabelComponent(
	AActor* OwnerActor,
	USceneComponent* AttachParent,
	const FSRConveyorLaneKey& LaneKey,
	const FSRConveyorItemLabelSettings& LabelSettings,
	TMap<FSRConveyorLaneKey, TObjectPtr<UTextRenderComponent>>& ItemLabelsByLane)
{
	if (TObjectPtr<UTextRenderComponent>* ExistingComponent = ItemLabelsByLane.Find(LaneKey))
	{
		if (IsValid(*ExistingComponent))
		{
			return *ExistingComponent;
		}
		ItemLabelsByLane.Remove(LaneKey);
	}

	if (!IsValid(OwnerActor) || !IsValid(AttachParent))
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

	LabelComponent->SetupAttachment(AttachParent);
	LabelComponent->SetMobility(EComponentMobility::Movable);
	LabelComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LabelComponent->SetGenerateOverlapEvents(false);
	LabelComponent->SetCastShadow(false);
	LabelComponent->SetHorizontalAlignment(EHTA_Center);
	LabelComponent->SetVerticalAlignment(EVRTA_TextCenter);
	LabelComponent->SetWorldSize(FMath::Max(1.0f, LabelSettings.ItemEnergyLabelWorldSize));
	LabelComponent->SetTextRenderColor(LabelSettings.ItemEnergyLowColor.ToFColor(true));
	LabelComponent->SetText(FText::FromString(TEXT("E 0.00")));
	LabelComponent->SetVisibility(true);
	LabelComponent->SetHiddenInGame(false);
	LabelComponent->SetTranslucentSortPriority(100);
	LabelComponent->ComponentTags.AddUnique(TEXT("StarRovers.ConveyorItemEnergyLabel"));

	OwnerActor->AddInstanceComponent(LabelComponent);
	LabelComponent->RegisterComponent();
	ItemLabelsByLane.Add(LaneKey, LabelComponent);
	return LabelComponent;
}
