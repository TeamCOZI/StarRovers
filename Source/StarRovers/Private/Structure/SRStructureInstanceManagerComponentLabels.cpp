#include "Structure/SRStructureInstanceManagerComponent.h"

#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Surface/SRPlanetSurfaceGrid.h"

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

void USRStructureInstanceManagerComponent::RefreshStructureNameLabel(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlacedStructureInstance& PlacedStructure)
{
	if (PlacedStructure.OccupantId.IsNone()
		|| !bShowStructureNameLabels
		|| (PlacedStructure.bNaturalStructure && !bShowNaturalStructureNameLabels)
		|| !IsValid(SurfaceGrid))
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

	LabelComponent->SetText(BuildStructureNameLabelText(PlacedStructure));
	LabelComponent->SetTextRenderColor(StructureNameLabelColor.ToFColor(true));
	LabelComponent->SetWorldSize(FMath::Max(1.0f, StructureNameLabelWorldSize));
	LabelComponent->SetWorldLocation(LabelLocation);
	LabelComponent->SetVisibility(true);
	LabelComponent->SetHiddenInGame(false);
	UpdateNameLabelTickEnabled();
}

void USRStructureInstanceManagerComponent::RefreshAllStructureNameLabels(USRPlanetSurfaceGrid* SurfaceGrid)
{
	if (!bShowStructureNameLabels)
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
	if (!bShowStructureNameLabels || !IsValid(SurfaceGrid))
	{
		DestroyAllStructureNameLabels();
		return;
	}

	FVector CameraLocation = FVector::ZeroVector;
	FVector CameraFacingNormal = FVector::ForwardVector;
	FVector CameraUp = FVector::UpVector;
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
			bHasCameraLocation = true;
		}
	}

	TArray<FName> LabelOccupantIds;
	StructureNameLabelsByOccupantId.GetKeys(LabelOccupantIds);
	for (const FName OccupantId : LabelOccupantIds)
	{
		const FSRPlacedStructureInstance* PlacedStructure = PlacedStructuresByOccupantId.Find(OccupantId);
		TObjectPtr<UTextRenderComponent>* LabelComponentPtr = StructureNameLabelsByOccupantId.Find(OccupantId);
		if (!PlacedStructure || !LabelComponentPtr || !IsValid(*LabelComponentPtr))
		{
			DestroyStructureNameLabel(OccupantId);
			continue;
		}

		if (PlacedStructure->bNaturalStructure && !bShowNaturalStructureNameLabels)
		{
			DestroyStructureNameLabel(OccupantId);
			continue;
		}

		FVector LabelLocation = FVector::ZeroVector;
		if (!ResolveStructureNameLabelLocation(SurfaceGrid, *PlacedStructure, LabelLocation))
		{
			DestroyStructureNameLabel(OccupantId);
			continue;
		}

		UTextRenderComponent* LabelComponent = LabelComponentPtr->Get();
		LabelComponent->SetWorldLocation(LabelLocation);
		if (bHasCameraLocation)
		{
			LabelComponent->SetWorldRotation(FRotationMatrix::MakeFromXZ(CameraFacingNormal, CameraUp).Rotator());

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
	if (const USRStructureDataAsset* StructureDataAsset = PlacedStructure.StructureDataAsset.Get())
	{
		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		if (!StructureData.DisplayName.IsEmpty())
		{
			return StructureData.DisplayName;
		}
	}

	if (!PlacedStructure.StructureId.IsNone())
	{
		return FText::FromName(PlacedStructure.StructureId);
	}
	return FText::FromName(PlacedStructure.OccupantId);
}

void USRStructureInstanceManagerComponent::UpdateNameLabelTickEnabled()
{
	SetComponentTickEnabled(bShowStructureNameLabels && !StructureNameLabelsByOccupantId.IsEmpty());
}
