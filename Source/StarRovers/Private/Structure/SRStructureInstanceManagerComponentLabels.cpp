#include "Structure/SRStructureInstanceManagerComponent.h"

#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	const FLinearColor MiningDepositLabelColor(0.18f, 0.88f, 1.0f, 1.0f);
	const FLinearColor MiningTargetLabelColor(0.3f, 1.0f, 0.42f, 1.0f);
}

void USRStructureInstanceManagerComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	if (AActor* OwnerActor = GetOwner())
	{
		SurfaceGrid = OwnerActor->FindComponentByClass<USRPlanetSurfaceGrid>();
	}

	UpdateStructureNameLabelTransforms(SurfaceGrid);
	UpdateNameLabelTickEnabled();
}

bool USRStructureInstanceManagerComponent::ShouldShowStructureNameLabel(
	const FSRPlacedStructureInstance& PlacedStructure) const
{
	if (PlacedStructure.OccupantId.IsNone())
	{
		return false;
	}

	if (IsMiningResourceDepositHighlighted(PlacedStructure.OccupantId)
		|| IsMiningResourceDepositTarget(PlacedStructure.OccupantId))
	{
		return true;
	}

	return bShowStructureNameLabels
		&& (!PlacedStructure.bNaturalStructure || bShowNaturalStructureNameLabels);
}

void USRStructureInstanceManagerComponent::RefreshStructureNameLabel(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlacedStructureInstance& PlacedStructure)
{
	if (!ShouldShowStructureNameLabel(PlacedStructure) || !IsValid(SurfaceGrid))
	{
		DestroyStructureNameLabel(PlacedStructure.OccupantId);
		UpdateNameLabelTickEnabled();
		return;
	}

	FVector LabelLocation = FVector::ZeroVector;
	if (!ResolveStructureNameLabelLocation(SurfaceGrid, PlacedStructure, LabelLocation))
	{
		DestroyStructureNameLabel(PlacedStructure.OccupantId);
		UpdateNameLabelTickEnabled();
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	UTextRenderComponent* LabelComponent = nullptr;
	if (TObjectPtr<UTextRenderComponent>* ExistingLabel = StructureNameLabelsByOccupantId.Find(PlacedStructure.OccupantId))
	{
		LabelComponent = ExistingLabel->Get();
	}

	if (!IsValid(LabelComponent))
	{
		const FName ComponentName = MakeUniqueObjectName(
			OwnerActor,
			UTextRenderComponent::StaticClass(),
			FName(TEXT("StructureNameLabel")));
		LabelComponent = NewObject<UTextRenderComponent>(OwnerActor, ComponentName);
		if (!IsValid(LabelComponent))
		{
			return;
		}

		LabelComponent->SetupAttachment(this);
		LabelComponent->SetMobility(EComponentMobility::Movable);
		LabelComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		LabelComponent->SetGenerateOverlapEvents(false);
		LabelComponent->SetCastShadow(false);
		LabelComponent->SetHorizontalAlignment(EHTA_Center);
		LabelComponent->SetVerticalAlignment(EVRTA_TextCenter);
		LabelComponent->SetTranslucentSortPriority(90);
		LabelComponent->ComponentTags.AddUnique(TEXT("StarRovers.StructureNameLabel"));

		OwnerActor->AddInstanceComponent(LabelComponent);
		LabelComponent->RegisterComponent();
		StructureNameLabelsByOccupantId.Add(PlacedStructure.OccupantId, LabelComponent);
	}

	const bool bMiningTarget = IsMiningResourceDepositTarget(PlacedStructure.OccupantId);
	const bool bMiningDeposit = IsMiningResourceDepositHighlighted(PlacedStructure.OccupantId);
	const FLinearColor LabelColor = bMiningTarget
		? MiningTargetLabelColor
		: (bMiningDeposit ? MiningDepositLabelColor : StructureNameLabelColor);
	const float LabelSizeScale = bMiningTarget ? 1.2f : (bMiningDeposit ? 1.05f : 1.0f);
	LabelComponent->SetText(BuildStructureNameLabelText(PlacedStructure));
	LabelComponent->SetTextRenderColor(LabelColor.ToFColor(true));
	LabelComponent->SetWorldSize(FMath::Max(1.0f, StructureNameLabelWorldSize * LabelSizeScale));
	LabelComponent->SetWorldLocation(LabelLocation);
	LabelComponent->SetVisibility(true);
	LabelComponent->SetHiddenInGame(false);
	UpdateNameLabelTickEnabled();
}

void USRStructureInstanceManagerComponent::RefreshAllStructureNameLabels(USRPlanetSurfaceGrid* SurfaceGrid)
{
	if (!bShowStructureNameLabels && MiningHighlightedResourceDepositOccupantIds.IsEmpty())
	{
		DestroyAllStructureNameLabels();
		UpdateNameLabelTickEnabled();
		return;
	}

	for (const TPair<FName, FSRPlacedStructureInstance>& PlacedStructurePair : PlacedStructuresByOccupantId)
	{
		RefreshStructureNameLabel(SurfaceGrid, PlacedStructurePair.Value);
	}
	UpdateNameLabelTickEnabled();
}

void USRStructureInstanceManagerComponent::DestroyStructureNameLabel(FName OccupantId)
{
	if (TObjectPtr<UTextRenderComponent>* LabelComponentPtr = StructureNameLabelsByOccupantId.Find(OccupantId))
	{
		if (IsValid(*LabelComponentPtr))
		{
			(*LabelComponentPtr)->DestroyComponent();
		}
		StructureNameLabelsByOccupantId.Remove(OccupantId);
	}
	UpdateNameLabelTickEnabled();
}

void USRStructureInstanceManagerComponent::DestroyAllStructureNameLabels()
{
	for (const TPair<FName, TObjectPtr<UTextRenderComponent>>& LabelPair : StructureNameLabelsByOccupantId)
	{
		if (IsValid(LabelPair.Value))
		{
			LabelPair.Value->DestroyComponent();
		}
	}
	StructureNameLabelsByOccupantId.Reset();
	UpdateNameLabelTickEnabled();
}

void USRStructureInstanceManagerComponent::UpdateStructureNameLabelTransforms(USRPlanetSurfaceGrid* SurfaceGrid)
{
	if ((!bShowStructureNameLabels && MiningHighlightedResourceDepositOccupantIds.IsEmpty())
		|| !IsValid(SurfaceGrid))
	{
		DestroyAllStructureNameLabels();
		return;
	}

	FVector CameraLocation = FVector::ZeroVector;
	FVector CameraFacingNormal = FVector::ForwardVector;
	FVector CameraUp = FVector::UpVector;
	FRotator CameraFacingRotation = FRotator::ZeroRotator;
	bool bHasCameraLocation = false;
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			FRotator CameraRotation = FRotator::ZeroRotator;
			PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);
			CameraFacingNormal = -CameraRotation.Vector();
			CameraUp = CameraRotation.RotateVector(FVector::UpVector).GetSafeNormal();
			if (CameraFacingNormal.IsNearlyZero())
			{
				CameraFacingNormal = FVector::ForwardVector;
			}
			if (CameraUp.IsNearlyZero())
			{
				CameraUp = FVector::UpVector;
			}
			CameraFacingRotation = FRotationMatrix::MakeFromXZ(CameraFacingNormal, CameraUp).Rotator();
			bHasCameraLocation = true;
		}
	}

	bool bRemovedLabel = false;
	for (auto LabelIterator = StructureNameLabelsByOccupantId.CreateIterator(); LabelIterator; ++LabelIterator)
	{
		const FName OccupantId = LabelIterator.Key();
		const FSRPlacedStructureInstance* PlacedStructure = PlacedStructuresByOccupantId.Find(OccupantId);
		UTextRenderComponent* LabelComponent = LabelIterator.Value().Get();
		if (!PlacedStructure || !IsValid(LabelComponent))
		{
			if (IsValid(LabelComponent))
			{
				LabelComponent->DestroyComponent();
			}
			LabelIterator.RemoveCurrent();
			bRemovedLabel = true;
			continue;
		}

		if (!ShouldShowStructureNameLabel(*PlacedStructure))
		{
			LabelComponent->DestroyComponent();
			LabelIterator.RemoveCurrent();
			bRemovedLabel = true;
			continue;
		}

		FVector LabelLocation = FVector::ZeroVector;
		if (!ResolveStructureNameLabelLocation(SurfaceGrid, *PlacedStructure, LabelLocation))
		{
			LabelComponent->DestroyComponent();
			LabelIterator.RemoveCurrent();
			bRemovedLabel = true;
			continue;
		}

		LabelComponent->SetWorldLocation(LabelLocation);
		if (bHasCameraLocation)
		{
			LabelComponent->SetWorldRotation(CameraFacingRotation);

			if (StructureNameLabelMaxDrawDistance > KINDA_SMALL_NUMBER)
			{
				const bool bWithinDrawDistance = FVector::DistSquared(CameraLocation, LabelLocation)
					<= FMath::Square(StructureNameLabelMaxDrawDistance);
				LabelComponent->SetVisibility(bWithinDrawDistance);
			}
			else
			{
				LabelComponent->SetVisibility(true);
			}
		}
	}

	if (bRemovedLabel)
	{
		UpdateNameLabelTickEnabled();
	}
}

bool USRStructureInstanceManagerComponent::ResolveStructureNameLabelLocation(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlacedStructureInstance& PlacedStructure,
	FVector& OutWorldLocation) const
{
	OutWorldLocation = FVector::ZeroVector;
	if (!IsValid(SurfaceGrid) || PlacedStructure.FootprintCellIds.IsEmpty())
	{
		return false;
	}

	FVector CenterSum = FVector::ZeroVector;
	FVector NormalSum = FVector::ZeroVector;
	int32 ValidCellCount = 0;
	const FVector SurfaceCenter = SurfaceGrid->GetComponentTransform().GetLocation();
	for (const FSRPlanetSurfaceGridCellId& CellId : PlacedStructure.FootprintCellIds)
	{
		FSRPlanetSurfaceGridCellInfo CellInfo;
		if (!SurfaceGrid->GetCellInfoById(CellId, CellInfo))
		{
			continue;
		}

		FVector OutwardNormal = CellInfo.WorldNormal.GetSafeNormal();
		if (OutwardNormal.IsNearlyZero())
		{
			OutwardNormal = (CellInfo.WorldCenter - SurfaceCenter).GetSafeNormal();
		}
		else if (FVector::DotProduct(OutwardNormal, CellInfo.WorldCenter - SurfaceCenter) < 0.0f)
		{
			OutwardNormal *= -1.0f;
		}

		if (OutwardNormal.IsNearlyZero())
		{
			OutwardNormal = FVector::UpVector;
		}

		CenterSum += CellInfo.WorldCenter;
		NormalSum += OutwardNormal;
		++ValidCellCount;
	}

	if (ValidCellCount <= 0)
	{
		return false;
	}

	FVector LabelNormal = NormalSum.GetSafeNormal();
	if (LabelNormal.IsNearlyZero())
	{
		LabelNormal = FVector::UpVector;
	}

	const FVector FootprintCenter = CenterSum / static_cast<float>(ValidCellCount);
	OutWorldLocation = FootprintCenter + LabelNormal * FMath::Max(0.0f, StructureNameLabelHeightOffset);
	return true;
}

FText USRStructureInstanceManagerComponent::BuildStructureNameLabelText(const FSRPlacedStructureInstance& PlacedStructure) const
{
	FText BaseLabel;
	if (const USRStructureDataAsset* StructureDataAsset = PlacedStructure.StructureDataAsset.Get())
	{
		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		if (!StructureData.DisplayName.IsEmpty())
		{
			BaseLabel = StructureData.DisplayName;
		}
	}

	if (BaseLabel.IsEmpty() && !PlacedStructure.StructureId.IsNone())
	{
		BaseLabel = FText::FromName(PlacedStructure.StructureId);
	}
	if (BaseLabel.IsEmpty())
	{
		BaseLabel = FText::FromName(PlacedStructure.OccupantId);
	}

	if (IsMiningResourceDepositTarget(PlacedStructure.OccupantId))
	{
		return FText::Format(
			NSLOCTEXT("StarRoversStructureLabels", "MiningTargetLabel", "TARGET  ·  {0}"),
			BaseLabel);
	}
	if (IsMiningResourceDepositHighlighted(PlacedStructure.OccupantId))
	{
		return FText::Format(
			NSLOCTEXT("StarRoversStructureLabels", "MiningDepositLabel", "RESOURCE  ·  {0}"),
			BaseLabel);
	}
	return BaseLabel;
}

void USRStructureInstanceManagerComponent::UpdateNameLabelTickEnabled()
{
	SetComponentTickEnabled(!StructureNameLabelsByOccupantId.IsEmpty());
}
