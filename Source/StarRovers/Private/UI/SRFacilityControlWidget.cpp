#include "UI/SRFacilityControlWidget.h"

#include "Utility/SRLog.h"
#include "Automation/SRFacilityNetworkComponent.h"
#include "Automation/SRFacilityResourceOperations.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/SRPlayerController.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Actor.h"
#include "Logistics/SRSpaceLogisticsSubsystem.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"

namespace
{
	const TCHAR* GetFacilityTemperatureLabel(ESRFacilityTemperatureState TemperatureState)
	{
		switch (TemperatureState)
		{
		case ESRFacilityTemperatureState::Frozen:
			return TEXT("Frozen");
		case ESRFacilityTemperatureState::Cold:
			return TEXT("Cold");
		case ESRFacilityTemperatureState::Normal:
			return TEXT("Normal");
		case ESRFacilityTemperatureState::Hot:
			return TEXT("Hot");
		case ESRFacilityTemperatureState::Overheated:
			return TEXT("Overheated");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* GetResourceKindLabel(ESRResourceKind ResourceKind)
	{
		switch (ResourceKind)
		{
		case ESRResourceKind::Energy:
			return TEXT("Energy");
		case ESRResourceKind::Catalyst:
			return TEXT("Catalyst");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* GetCatalystOperatorLabel(ESRResourceCatalystOperator CatalystOperator)
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
		case ESRResourceCatalystOperator::None:
		default:
			return TEXT("None");
		}
	}

	const TCHAR* GetResourceProcessTagLabel(ESRResourceProcessTag ResourceTag)
	{
		switch (ResourceTag)
		{
		case ESRResourceProcessTag::Responsive:
			return TEXT("Responsive");
		case ESRResourceProcessTag::HalfLife:
			return TEXT("HalfLife");
		case ESRResourceProcessTag::Volatile:
			return TEXT("Volatile");
		case ESRResourceProcessTag::Singularity:
			return TEXT("Singularity");
		case ESRResourceProcessTag::Waste:
			return TEXT("Waste");
		default:
			return TEXT("Tag");
		}
	}

	const TCHAR* GetEffectKindLabel(ESRFacilityEffectKind EffectKind)
	{
		switch (EffectKind)
		{
		case ESRFacilityEffectKind::AddEnergy:
			return TEXT("Energy +");
		case ESRFacilityEffectKind::MultiplyEnergy:
			return TEXT("Energy *");
		case ESRFacilityEffectKind::AddProcessLimit:
			return TEXT("Limit +");
		case ESRFacilityEffectKind::AddTag:
			return TEXT("Add Tag");
		case ESRFacilityEffectKind::RemoveTag:
			return TEXT("Remove Tag");
		case ESRFacilityEffectKind::ProduceResource:
			return TEXT("Produce");
		case ESRFacilityEffectKind::SubtractEnergy:
			return TEXT("Energy -");
		case ESRFacilityEffectKind::DivideEnergy:
			return TEXT("Energy /");
		case ESRFacilityEffectKind::SubtractProcessLimit:
			return TEXT("Limit -");
		case ESRFacilityEffectKind::MultiplyProcessLimit:
			return TEXT("Limit *");
		case ESRFacilityEffectKind::DivideProcessLimit:
			return TEXT("Limit /");
		case ESRFacilityEffectKind::AddCellTemperature:
			return TEXT("Cell Temp +");
		case ESRFacilityEffectKind::SubtractCellTemperature:
			return TEXT("Cell Temp -");
		default:
			return TEXT("Effect");
		}
	}

	const TCHAR* GetHubRoutePhaseLabel(ESRSpaceLogisticsHubRoutePhase Phase)
	{
		switch (Phase)
		{
		case ESRSpaceLogisticsHubRoutePhase::Idle:
			return TEXT("Idle");
		case ESRSpaceLogisticsHubRoutePhase::WaitingForCargo:
			return TEXT("Waiting");
		case ESRSpaceLogisticsHubRoutePhase::TravelingToDestination:
			return TEXT("Outbound");
		case ESRSpaceLogisticsHubRoutePhase::UnloadingAtDestination:
			return TEXT("Unload Dest");
		case ESRSpaceLogisticsHubRoutePhase::TravelingToSource:
			return TEXT("Return");
		case ESRSpaceLogisticsHubRoutePhase::UnloadingAtSource:
			return TEXT("Unload Source");
		case ESRSpaceLogisticsHubRoutePhase::Blocked:
			return TEXT("Blocked");
		default:
			return TEXT("Unknown");
		}
	}

	bool AreHubEndpointKeysEqual(const FSRSpaceLogisticsHubEndpoint& Left, const FSRSpaceLogisticsHubEndpoint& Right)
	{
		return Left.BodyActor == Right.BodyActor && Left.HubOccupantId == Right.HubOccupantId;
	}

	bool DoesHubRouteConnectEndpoints(
		const FSRSpaceLogisticsHubRoute& Route,
		const FSRSpaceLogisticsHubEndpoint& SourceHub,
		const FSRSpaceLogisticsHubEndpoint& DestinationHub)
	{
		return (AreHubEndpointKeysEqual(Route.SourceHub, SourceHub) && AreHubEndpointKeysEqual(Route.DestinationHub, DestinationHub))
			|| (AreHubEndpointKeysEqual(Route.SourceHub, DestinationHub) && AreHubEndpointKeysEqual(Route.DestinationHub, SourceHub));
	}

	FString BuildCelestialBodyDisplayName(const AActor* BodyActor)
	{
		if (!IsValid(BodyActor))
		{
			return TEXT("Unknown Body");
		}

		const FText BodyName = USRCelestialBodyRuntimeLibrary::GetCelestialVariableName(BodyActor);
		return BodyName.IsEmpty()
			? GetNameSafe(BodyActor)
			: BodyName.ToString();
	}

	AActor* ResolvePrimaryStarOrbitBody(const AActor* BodyActor, const AActor* PrimaryStarActor)
	{
		if (!IsValid(BodyActor) || !IsValid(PrimaryStarActor) || BodyActor == PrimaryStarActor)
		{
			return nullptr;
		}

		AActor* CurrentBody = const_cast<AActor*>(BodyActor);
		for (int32 ParentDepth = 0; ParentDepth < 16 && IsValid(CurrentBody); ++ParentDepth)
		{
			AActor* ParentBody = nullptr;
			if (!USRCelestialBodyRuntimeLibrary::GetCelestialParentBody(CurrentBody, ParentBody) || !IsValid(ParentBody))
			{
				return nullptr;
			}

			if (ParentBody == PrimaryStarActor)
			{
				return CurrentBody;
			}

			CurrentBody = ParentBody;
		}

		return nullptr;
	}

	float ResolveStarOrbitSortValue(const AActor* BodyActor, const AActor* PrimaryStarActor)
	{
		float OrbitRadius = 0.0f;
		if (USRCelestialBodyRuntimeLibrary::GetCelestialOrbitRadius(BodyActor, OrbitRadius) && OrbitRadius > KINDA_SMALL_NUMBER)
		{
			return OrbitRadius;
		}

		return IsValid(BodyActor) && IsValid(PrimaryStarActor)
			? FVector::DistSquared(BodyActor->GetActorLocation(), PrimaryStarActor->GetActorLocation())
			: BIG_NUMBER;
	}

	int32 ResolveStarOrbitOrdinal(const AActor* BodyActor, UWorld* World, bool& bOutIsSatellite)
	{
		bOutIsSatellite = false;
		if (!IsValid(BodyActor) || !IsValid(World) || USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(BodyActor))
		{
			return INDEX_NONE;
		}

		const USRCelestialBodyRegistrySubsystem* CelestialRegistry = World->GetSubsystem<USRCelestialBodyRegistrySubsystem>();
		if (!IsValid(CelestialRegistry))
		{
			return INDEX_NONE;
		}

		AActor* PrimaryStarActor = CelestialRegistry->GetPrimaryStarActor();
		if (!IsValid(PrimaryStarActor))
		{
			return INDEX_NONE;
		}

		AActor* TargetStarOrbitBody = ResolvePrimaryStarOrbitBody(BodyActor, PrimaryStarActor);
		if (!IsValid(TargetStarOrbitBody))
		{
			return INDEX_NONE;
		}
		bOutIsSatellite = TargetStarOrbitBody != BodyActor;

		TArray<AActor*> CelestialBodies;
		CelestialRegistry->GetCelestialBodies(CelestialBodies);

		TArray<AActor*> StarOrbitBodies;
		TSet<AActor*> UniqueStarOrbitBodies;
		StarOrbitBodies.Reserve(CelestialBodies.Num());
		UniqueStarOrbitBodies.Reserve(CelestialBodies.Num());
		for (AActor* CelestialBody : CelestialBodies)
		{
			if (!IsValid(CelestialBody) || CelestialBody == PrimaryStarActor)
			{
				continue;
			}

			AActor* StarOrbitBody = ResolvePrimaryStarOrbitBody(CelestialBody, PrimaryStarActor);
			if (IsValid(StarOrbitBody))
			{
				bool bAlreadyAdded = false;
				UniqueStarOrbitBodies.Add(StarOrbitBody, &bAlreadyAdded);
				if (!bAlreadyAdded)
				{
					StarOrbitBodies.Add(StarOrbitBody);
				}
			}
		}

		StarOrbitBodies.Sort(
			[PrimaryStarActor](const AActor& LeftBody, const AActor& RightBody)
			{
				const float LeftSortValue = ResolveStarOrbitSortValue(&LeftBody, PrimaryStarActor);
				const float RightSortValue = ResolveStarOrbitSortValue(&RightBody, PrimaryStarActor);
				if (!FMath::IsNearlyEqual(LeftSortValue, RightSortValue))
				{
					return LeftSortValue < RightSortValue;
				}

				return LeftBody.GetFName().LexicalLess(RightBody.GetFName());
			});

		for (int32 BodyIndex = 0; BodyIndex < StarOrbitBodies.Num(); ++BodyIndex)
		{
			if (StarOrbitBodies[BodyIndex] == TargetStarOrbitBody)
			{
				return BodyIndex + 1;
			}
		}

		return INDEX_NONE;
	}

	FString BuildHubEndpointUILabel(const FSRSpaceLogisticsHubEndpoint& HubEndpoint, UWorld* World)
	{
		const FString HubName = HubEndpoint.DisplayName.IsEmpty()
			? HubEndpoint.HubOccupantId.ToString()
			: HubEndpoint.DisplayName.ToString();
		const FString BodyName = BuildCelestialBodyDisplayName(HubEndpoint.BodyActor.Get());
		FString OrbitOrderLabel = TEXT("Orbit order unknown");
		if (USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(HubEndpoint.BodyActor.Get()))
		{
			OrbitOrderLabel = TEXT("Primary Star");
		}
		else
		{
			bool bIsSatellite = false;
			const int32 StarOrbitOrdinal = ResolveStarOrbitOrdinal(HubEndpoint.BodyActor.Get(), World, bIsSatellite);
			if (StarOrbitOrdinal != INDEX_NONE)
			{
				OrbitOrderLabel = bIsSatellite
					? FString::Printf(TEXT("#%d from star / satellite"), StarOrbitOrdinal)
					: FString::Printf(TEXT("#%d from star"), StarOrbitOrdinal);
			}
		}

		return FString::Printf(
			TEXT("%s\n%s\n%s"),
			*BodyName,
			*OrbitOrderLabel,
			*HubName);
	}

	FString BuildCompactResourceIdLabel(FName ResourceId)
	{
		if (ResourceId.IsNone())
		{
			return TEXT("Any");
		}

		FString Label = ResourceId.ToString();
		if (Label.Len() > 10)
		{
			Label = Label.Left(9) + TEXT(".");
		}
		return Label;
	}

	FString BuildResourceDisplayName(const FSRResourceInstance& ResourceInstance)
	{
		if (const USRResourceDataAsset* ResourceDataAsset = ResourceInstance.ResourceDataAsset.Get())
		{
			if (!ResourceDataAsset->DisplayName.IsEmpty())
			{
				return ResourceDataAsset->DisplayName.ToString();
			}
			if (!ResourceDataAsset->ResourceId.IsNone())
			{
				return ResourceDataAsset->ResourceId.ToString();
			}
		}

		return ResourceInstance.ResourceId.IsNone()
			? TEXT("UnknownResource")
			: ResourceInstance.ResourceId.ToString();
	}

	FString BuildResourceDataAssetDisplayName(const USRResourceDataAsset* ResourceDataAsset)
	{
		if (!IsValid(ResourceDataAsset))
		{
			return TEXT("UnknownResource");
		}

		if (!ResourceDataAsset->DisplayName.IsEmpty())
		{
			return ResourceDataAsset->DisplayName.ToString();
		}

		return ResourceDataAsset->ResourceId.IsNone()
			? GetNameSafe(ResourceDataAsset)
			: ResourceDataAsset->ResourceId.ToString();
	}

	FString BuildResourceSummary(const FSRResourceInstance& ResourceInstance)
	{
		if (ResourceInstance.ResourceId.IsNone())
		{
			return TEXT("None");
		}

		FString Summary = FString::Printf(
			TEXT("%s [%s] x%d"),
			*BuildResourceDisplayName(ResourceInstance),
			GetResourceKindLabel(ResourceInstance.ResourceKind),
			FMath::Max(0, ResourceInstance.StackCount));

		if (ResourceInstance.ResourceKind == ESRResourceKind::Catalyst)
		{
			Summary += FString::Printf(TEXT("\nOp: %s"), GetCatalystOperatorLabel(ResourceInstance.CatalystOperator));
		}
		else
		{
			Summary += FString::Printf(
				TEXT("\nEnergy Total: %.2f\nLimit: %d\nUsed: %d"),
				ResourceInstance.EnergyValue,
				ResourceInstance.RemainingProcessLimit,
				ResourceInstance.ProcessCount);
		}

		if (!ResourceInstance.Tags.IsEmpty())
		{
			Summary += FString::Printf(TEXT("\nTags: %d"), ResourceInstance.Tags.Num());
		}
		return Summary;
	}

	FString BuildCompactResourceSummary(const FSRResourceInstance* ResourceInstance)
	{
		if (!ResourceInstance || ResourceInstance->ResourceId.IsNone())
		{
			return TEXT("Empty");
		}

		FString Summary = BuildResourceDisplayName(*ResourceInstance);
		if (ResourceInstance->ResourceKind == ESRResourceKind::Catalyst)
		{
			Summary += FString::Printf(TEXT("\nCatalyst  Op: %s"), GetCatalystOperatorLabel(ResourceInstance->CatalystOperator));
			return Summary;
		}

		Summary += FString::Printf(
			TEXT("\nEnergy Total: %.2f  Limit: %d"),
			ResourceInstance->EnergyValue,
			ResourceInstance->RemainingProcessLimit);
		if (!ResourceInstance->Tags.IsEmpty())
		{
			Summary += FString::Printf(TEXT("\nTags: %d"), ResourceInstance->Tags.Num());
		}
		return Summary;
	}

	FString BuildResourceSlotText(
		const TCHAR* Label,
		int32 SlotIndex,
		FName PortId,
		const FSRResourceInstance* ResourceInstance,
		const TCHAR* EmptyText)
	{
		const FString PortLabel = PortId.IsNone()
			? FString::Printf(TEXT("%s %d"), Label, SlotIndex + 1)
			: PortId.ToString();
		const FString ResourceText = ResourceInstance
			? BuildCompactResourceSummary(ResourceInstance)
			: FString(EmptyText);
		return FString::Printf(TEXT("%s %d\n%s\n%s"), Label, SlotIndex + 1, *PortLabel, *ResourceText);
	}

	FString BuildResourceTagCardLabel(const FSRResourceInstance& ResourceInstance)
	{
		int32 VisibleTagCount = 0;
		FString Label;
		for (const FSRResourceTagStack& TagStack : ResourceInstance.Tags)
		{
			if (TagStack.StackCount <= 0)
			{
				continue;
			}

			if (VisibleTagCount == 0)
			{
				Label = GetResourceProcessTagLabel(TagStack.Tag);
			}
			++VisibleTagCount;
		}

		if (VisibleTagCount <= 0)
		{
			return TEXT("No Tag");
		}
		if (VisibleTagCount > 1)
		{
			Label += FString::Printf(TEXT(" +%d"), VisibleTagCount - 1);
		}
		return Label;
	}

	FString BuildInventoryCardPortLabel(const FSRFacilityPortInventory& PortInventory, int32 SlotIndex, const TCHAR* FallbackLabel)
	{
		return PortInventory.PortId.IsNone()
			? FString::Printf(TEXT("%s_%02d"), FallbackLabel, SlotIndex + 1)
			: PortInventory.PortId.ToString();
	}

	FString BuildInventorySlotSignature(const FSRFacilityPortInventory& PortInventory)
	{
		FString Signature = FString::Printf(
			TEXT("%s:%d:%d"),
			*PortInventory.PortId.ToString(),
			PortInventory.Capacity,
			StarRovers::FacilityResources::GetInventorySlotStackCount(PortInventory));
		for (const FSRResourceInstance& ResourceInstance : PortInventory.Inventory)
		{
			Signature += FString::Printf(
				TEXT("|%s:%d:%.3f:%d:%d:%d:%d"),
				*ResourceInstance.ResourceId.ToString(),
				static_cast<int32>(ResourceInstance.ResourceKind),
				ResourceInstance.EnergyValue,
				static_cast<int32>(ResourceInstance.CatalystOperator),
				ResourceInstance.RemainingProcessLimit,
				ResourceInstance.ProcessCount,
				ResourceInstance.StackCount);
			for (const FSRResourceTagStack& TagStack : ResourceInstance.Tags)
			{
				Signature += FString::Printf(
					TEXT(":T%d/%d/%d"),
					static_cast<int32>(TagStack.Tag),
					TagStack.StackCount,
					TagStack.RemainingCycles);
			}
		}
		return Signature;
	}

	FString BuildInventoryPanelSignature(const TCHAR* Label, const TArray<FSRFacilityPortInventory>& PortInventories, const FString& EmptyText)
	{
		FString Signature = FString::Printf(TEXT("%s:%d:%s"), Label, PortInventories.Num(), *EmptyText);
		for (const FSRFacilityPortInventory& PortInventory : PortInventories)
		{
			Signature += TEXT("|");
			Signature += BuildInventorySlotSignature(PortInventory);
		}
		return Signature;
	}

	void BuildNextInputPreviewResources(const FSRFacilityInstance& FacilityInstance, TArray<FSRResourceInstance>& OutPreviewInputs)
	{
		OutPreviewInputs.Reset();
		if (!FacilityInstance.ProcessingInventory.IsEmpty())
		{
			for (const FSRResourceInstance& ResourceInstance : FacilityInstance.ProcessingInventory)
			{
				if (ResourceInstance.ResourceId.IsNone() || ResourceInstance.StackCount <= 0)
				{
					continue;
				}

				FSRResourceInstance PreviewResource = ResourceInstance;
				PreviewResource.StackCount = 1;
				OutPreviewInputs.Add(PreviewResource);
			}
			return;
		}

		for (const FSRFacilityPortInventory& PortInventory : FacilityInstance.InputPortInventories)
		{
			FSRResourceInstance PreviewResource = StarRovers::FacilityResources::PeekSingleResourceFromInventorySlot(PortInventory);
			if (!PreviewResource.ResourceId.IsNone() && PreviewResource.StackCount > 0)
			{
				OutPreviewInputs.Add(PreviewResource);
			}
		}
	}

	FString BuildInlineResourceSummary(const FSRResourceInstance& ResourceInstance)
	{
		FString Summary = BuildResourceDisplayName(ResourceInstance);
		if (ResourceInstance.ResourceKind == ESRResourceKind::Energy)
		{
			Summary += FString::Printf(TEXT("  Energy Total: %.2f"), ResourceInstance.EnergyValue);
		}
		else if (ResourceInstance.ResourceKind == ESRResourceKind::Catalyst)
		{
			Summary += FString::Printf(TEXT("  Op: %s"), GetCatalystOperatorLabel(ResourceInstance.CatalystOperator));
		}
		return Summary;
	}

	FString BuildInventorySummary(const TCHAR* Label, const TArray<FSRResourceInstance>& Inventory)
	{
		FString Summary = FString::Printf(TEXT("%s (%d)"), Label, Inventory.Num());
		for (int32 ResourceIndex = 0; ResourceIndex < Inventory.Num(); ++ResourceIndex)
		{
			Summary += FString::Printf(
				TEXT("\n%d. %s"),
				ResourceIndex + 1,
				*BuildInlineResourceSummary(Inventory[ResourceIndex]));
		}
		return Summary;
	}

	FString BuildPortInventorySummary(const TCHAR* Label, const TArray<FSRFacilityPortInventory>& PortInventories)
	{
		FString Summary = FString::Printf(TEXT("%s (%d ports)"), Label, PortInventories.Num());
		if (PortInventories.IsEmpty())
		{
			Summary += TEXT("\nNone");
			return Summary;
		}

		for (int32 PortIndex = 0; PortIndex < PortInventories.Num(); ++PortIndex)
		{
			const FSRFacilityPortInventory& PortInventory = PortInventories[PortIndex];
			Summary += FString::Printf(
				TEXT("\n%s (%d/%d)"),
				*PortInventory.PortId.ToString(),
				StarRovers::FacilityResources::GetInventorySlotStackCount(PortInventory),
				FMath::Max(1, PortInventory.Capacity));

			if (PortInventory.Inventory.IsEmpty())
			{
				Summary += TEXT("\n  Empty");
				continue;
			}

			for (int32 ResourceIndex = 0; ResourceIndex < PortInventory.Inventory.Num(); ++ResourceIndex)
			{
				Summary += FString::Printf(
					TEXT("\n  %d. %s"),
					ResourceIndex + 1,
					*BuildInlineResourceSummary(PortInventory.Inventory[ResourceIndex]));
			}
		}
		return Summary;
	}

	FString BuildMiningTargetSummary(USRFacilityNetworkComponent* FacilityNetwork, FName OccupantId)
	{
		if (!IsValid(FacilityNetwork) || OccupantId.IsNone())
		{
			return TEXT("Mining Target\nNo adjacent deposit");
		}

		FSRResourceDepositInstance ResourceDeposit;
		if (!FacilityNetwork->GetFacilityMiningTarget(OccupantId, ResourceDeposit))
		{
			return TEXT("Mining Target\nNo adjacent deposit");
		}

		return FString::Printf(
			TEXT("Mining Target\nDeposit: %s\nResource: %s\nRemaining: %d / %d"),
			ResourceDeposit.StructureId.IsNone() ? *ResourceDeposit.OccupantId.ToString() : *ResourceDeposit.StructureId.ToString(),
			*BuildResourceDataAssetDisplayName(ResourceDeposit.ResourceDataAsset.Get()),
			FMath::Max(0, ResourceDeposit.RemainingAmount),
			FMath::Max(0, ResourceDeposit.TotalAmount));
	}

	bool HasAvailableInputPortCapacity(const FSRFacilityInstance& FacilityInstance)
	{
		for (const FSRFacilityPortInventory& InputPortInventory : FacilityInstance.InputPortInventories)
		{
			if (StarRovers::FacilityResources::GetInventorySlotStackCount(InputPortInventory) < FMath::Max(1, InputPortInventory.Capacity))
			{
				return true;
			}
		}
		return false;
	}

	FString BuildResourceListSummary(const TCHAR* Label, const TArray<FSRResourceInstance>& ResourceInstances)
	{
		FString Summary = Label;
		if (ResourceInstances.IsEmpty())
		{
			Summary += TEXT("\nNone");
			return Summary;
		}

		const int32 VisibleCount = FMath::Min(ResourceInstances.Num(), 3);
		for (int32 ResourceIndex = 0; ResourceIndex < VisibleCount; ++ResourceIndex)
		{
			Summary += FString::Printf(
				TEXT("\n%d. %s"),
				ResourceIndex + 1,
				*BuildResourceSummary(ResourceInstances[ResourceIndex]));
		}
		if (ResourceInstances.Num() > VisibleCount)
		{
			Summary += FString::Printf(TEXT("\n... +%d"), ResourceInstances.Num() - VisibleCount);
		}
		return Summary;
	}

	FString BuildEffectsSummary(const USRFacilityDataAsset* FacilityDataAsset)
	{
		if (!IsValid(FacilityDataAsset))
		{
			return TEXT("Effects\nNone");
		}

		FString Summary = TEXT("Effects / Tags");
		if (FacilityDataAsset->Effects.IsEmpty())
		{
			Summary += TEXT("\nNone");
			return Summary;
		}

		const int32 VisibleCount = FMath::Min(FacilityDataAsset->Effects.Num(), 5);
		for (int32 EffectIndex = 0; EffectIndex < VisibleCount; ++EffectIndex)
		{
			const FSRFacilityEffectSpec& EffectSpec = FacilityDataAsset->Effects[EffectIndex];
			Summary += FString::Printf(
				TEXT("\n- %s %.2f"),
				GetEffectKindLabel(EffectSpec.EffectKind),
				EffectSpec.Value);
		}
		if (FacilityDataAsset->Effects.Num() > VisibleCount)
		{
			Summary += FString::Printf(TEXT("\n... +%d"), FacilityDataAsset->Effects.Num() - VisibleCount);
		}
		return Summary;
	}

	float ResolveProcessSeconds(const FSRFacilityInstance& FacilityInstance)
	{
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		float ProcessSeconds = IsValid(FacilityDataAsset) ? FMath::Max(0.01f, FacilityDataAsset->BaseProcessSeconds) : 1.0f;
		if (FacilityInstance.TemperatureState == ESRFacilityTemperatureState::Cold)
		{
			ProcessSeconds *= 2.0f;
		}
		return ProcessSeconds;
	}

	bool CanToggleProcess(const FSRFacilityInstance& FacilityInstance, FString& OutReason)
	{
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		if (!IsValid(FacilityDataAsset))
		{
			OutReason = TEXT("Invalid facility");
			return false;
		}
		if (FacilityInstance.TemperatureState == ESRFacilityTemperatureState::Frozen
			|| FacilityInstance.TemperatureState == ESRFacilityTemperatureState::Overheated)
		{
			OutReason = FString::Printf(TEXT("Blocked by %s"), GetFacilityTemperatureLabel(FacilityInstance.TemperatureState));
			return false;
		}
		if (FacilityDataAsset->bRequiresColdTemperature && FacilityInstance.TemperatureState != ESRFacilityTemperatureState::Cold)
		{
			OutReason = TEXT("Requires Cold");
			return false;
		}
		if (FacilityDataAsset->bRequiresHotTemperature && FacilityInstance.TemperatureState != ESRFacilityTemperatureState::Hot)
		{
			OutReason = TEXT("Requires Hot");
			return false;
		}

		OutReason = TEXT("Ready");
		return true;
	}

	UTextBlock* ConstructTextBlock(UWidgetTree* WidgetTree, const FName& Name, int32 FontSize, const FLinearColor& Color)
	{
		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo FontInfo = TextBlock->GetFont();
		FontInfo.Size = FontSize;
		TextBlock->SetFont(FontInfo);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetAutoWrapText(true);
		return TextBlock;
	}

	void AddWidgetToCanvas(UCanvasPanel* CanvasPanel, UWidget* Widget, const FVector2D& Position, const FVector2D& Size)
	{
		if (!CanvasPanel || !Widget)
		{
			return;
		}

		if (UCanvasPanelSlot* CanvasSlot = CanvasPanel->AddChildToCanvas(Widget))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
			CanvasSlot->SetAlignment(FVector2D::ZeroVector);
			CanvasSlot->SetPosition(Position);
			CanvasSlot->SetSize(Size);
		}
	}

	UBorder* ConstructSectionBorder(
		UWidgetTree* WidgetTree,
		const FName& Name,
		UWidget* Content,
		const FLinearColor& Color = FLinearColor(0.075f, 0.095f, 0.115f, 0.96f),
		const FMargin& Padding = FMargin(10.0f))
	{
		UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Border->SetBrushColor(Color);
		Border->SetPadding(Padding);
		if (Content)
		{
			Border->SetContent(Content);
		}
		return Border;
	}

	void AddResourceSlotCard(
		UWidgetTree* WidgetTree,
		UVerticalBox* SlotBox,
		const FString& Text,
		const FLinearColor& TextColor,
		const FLinearColor& CardColor)
	{
		if (!WidgetTree || !SlotBox)
		{
			return;
		}

		UTextBlock* SlotTextBlock = ConstructTextBlock(WidgetTree, NAME_None, 11, TextColor);
		SlotTextBlock->SetText(FText::FromString(Text));
		SlotTextBlock->SetJustification(ETextJustify::Left);

		UBorder* SlotBorder = ConstructSectionBorder(WidgetTree, NAME_None, SlotTextBlock, CardColor, FMargin(7.0f, 5.0f));
		if (UVerticalBoxSlot* Slot = SlotBox->AddChildToVerticalBox(SlotBorder))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}

	void AddInventoryCardText(
		UWidgetTree* WidgetTree,
		UCanvasPanel* CardCanvas,
		const FString& Text,
		const FVector2D& Position,
		const FVector2D& Size,
		int32 FontSize,
		const FLinearColor& Color,
		ETextJustify::Type Justification = ETextJustify::Left)
	{
		if (!WidgetTree || !CardCanvas)
		{
			return;
		}

		UTextBlock* TextBlock = ConstructTextBlock(WidgetTree, NAME_None, FontSize, Color);
		TextBlock->SetText(FText::FromString(Text));
		TextBlock->SetJustification(Justification);
		TextBlock->SetAutoWrapText(false);
		AddWidgetToCanvas(CardCanvas, TextBlock, Position, Size);
	}

	void AddInventorySlotCard(
		UWidgetTree* WidgetTree,
		UHorizontalBox* SlotBox,
		const FSRFacilityPortInventory& PortInventory,
		int32 SlotIndex,
		const TCHAR* FallbackLabel,
		const FLinearColor& AccentColor)
	{
		if (!WidgetTree || !SlotBox)
		{
			return;
		}

		const int32 Capacity = FMath::Max(1, PortInventory.Capacity);
		const int32 SlotStackCount = StarRovers::FacilityResources::GetInventorySlotStackCount(PortInventory);
		const FSRResourceInstance* ResourceInstance = PortInventory.Inventory.IsEmpty()
			? nullptr
			: &PortInventory.Inventory[0];

		FString TopLeft = BuildInventoryCardPortLabel(PortInventory, SlotIndex, FallbackLabel);
		FString TopRight = FString::Printf(TEXT("%d/%d"), SlotStackCount, Capacity);
		FString Center = TEXT("Empty");
		FString BottomLeft;
		FString BottomRight;
		FLinearColor CardColor = FLinearColor(0.145f, 0.170f, 0.190f, 0.98f);
		FLinearColor MainTextColor = FLinearColor(0.90f, 0.94f, 0.96f, 1.0f);

		if (ResourceInstance && !ResourceInstance->ResourceId.IsNone())
		{
			Center = BuildResourceDisplayName(*ResourceInstance);
			CardColor = ResourceInstance->ResourceKind == ESRResourceKind::Catalyst
				? FLinearColor(0.165f, 0.155f, 0.120f, 0.98f)
				: FLinearColor(0.125f, 0.175f, 0.160f, 0.98f);

			if (ResourceInstance->ResourceKind == ESRResourceKind::Catalyst)
			{
				TopLeft = GetCatalystOperatorLabel(ResourceInstance->CatalystOperator);
				BottomLeft = BuildInventoryCardPortLabel(PortInventory, SlotIndex, FallbackLabel);
				BottomRight = FString::Printf(TEXT("x%d"), SlotStackCount);
			}
			else
			{
				TopLeft = BuildResourceTagCardLabel(*ResourceInstance);
				TopRight = FString::Printf(TEXT("E:%.0f"), ResourceInstance->EnergyValue);
				BottomLeft = FString::Printf(TEXT("L:%d"), ResourceInstance->RemainingProcessLimit);
				BottomRight = FString::Printf(TEXT("%d/%d"), SlotStackCount, Capacity);
			}
		}

		UCanvasPanel* CardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		AddInventoryCardText(WidgetTree, CardCanvas, TopLeft, FVector2D(7.0f, 5.0f), FVector2D(60.0f, 18.0f), 9, AccentColor);
		AddInventoryCardText(WidgetTree, CardCanvas, TopRight, FVector2D(58.0f, 5.0f), FVector2D(46.0f, 18.0f), 9, AccentColor, ETextJustify::Right);
		AddInventoryCardText(WidgetTree, CardCanvas, Center, FVector2D(8.0f, 34.0f), FVector2D(96.0f, 24.0f), 12, MainTextColor, ETextJustify::Center);
		AddInventoryCardText(WidgetTree, CardCanvas, BottomLeft, FVector2D(7.0f, 68.0f), FVector2D(54.0f, 18.0f), 9, MainTextColor);
		AddInventoryCardText(WidgetTree, CardCanvas, BottomRight, FVector2D(55.0f, 68.0f), FVector2D(49.0f, 18.0f), 9, MainTextColor, ETextJustify::Right);

		USizeBox* CardSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CardSizeBox->SetWidthOverride(112.0f);
		CardSizeBox->SetHeightOverride(90.0f);
		CardSizeBox->AddChild(CardCanvas);

		UBorder* InnerBorder = ConstructSectionBorder(WidgetTree, NAME_None, CardSizeBox, CardColor, FMargin(0.0f));
		UBorder* OuterBorder = ConstructSectionBorder(WidgetTree, NAME_None, InnerBorder, FLinearColor(0.005f, 0.006f, 0.007f, 1.0f), FMargin(3.0f));
		if (UHorizontalBoxSlot* Slot = SlotBox->AddChildToHorizontalBox(OuterBorder))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddPreviewResourceCard(
		UWidgetTree* WidgetTree,
		UHorizontalBox* SlotBox,
		const FSRResourceInstance& ResourceInstance,
		const FLinearColor& AccentColor)
	{
		if (!WidgetTree || !SlotBox)
		{
			return;
		}

		const bool bHasResource = !ResourceInstance.ResourceId.IsNone();
		const bool bIsCatalyst = bHasResource && ResourceInstance.ResourceKind == ESRResourceKind::Catalyst;
		const FString TopLeft = bHasResource ? BuildResourceTagCardLabel(ResourceInstance) : TEXT("-");
		const FString TopRight = bHasResource
			? FString::Printf(TEXT("E:%.0f"), ResourceInstance.EnergyValue)
			: TEXT("-");
		const FString Center = bHasResource ? BuildResourceDisplayName(ResourceInstance) : TEXT("Empty");
		const FString BottomLeft = bHasResource
			? FString::Printf(TEXT("L:%d"), ResourceInstance.RemainingProcessLimit)
			: FString();
		const FString BottomRight = bHasResource ? GetResourceKindLabel(ResourceInstance.ResourceKind) : FString();
		const FLinearColor CardColor = !bHasResource
			? FLinearColor(0.145f, 0.170f, 0.190f, 0.98f)
			: (bIsCatalyst
				? FLinearColor(0.165f, 0.155f, 0.120f, 0.98f)
				: FLinearColor(0.125f, 0.175f, 0.160f, 0.98f));
		const FLinearColor MainTextColor = FLinearColor(0.90f, 0.94f, 0.96f, 1.0f);

		UCanvasPanel* CardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		AddInventoryCardText(WidgetTree, CardCanvas, TopLeft, FVector2D(7.0f, 5.0f), FVector2D(60.0f, 18.0f), 9, AccentColor);
		AddInventoryCardText(WidgetTree, CardCanvas, TopRight, FVector2D(58.0f, 5.0f), FVector2D(46.0f, 18.0f), 9, AccentColor, ETextJustify::Right);
		AddInventoryCardText(WidgetTree, CardCanvas, Center, FVector2D(8.0f, 34.0f), FVector2D(96.0f, 24.0f), 12, MainTextColor, ETextJustify::Center);
		AddInventoryCardText(WidgetTree, CardCanvas, BottomLeft, FVector2D(7.0f, 68.0f), FVector2D(54.0f, 18.0f), 9, MainTextColor);
		AddInventoryCardText(WidgetTree, CardCanvas, BottomRight, FVector2D(43.0f, 68.0f), FVector2D(61.0f, 18.0f), 9, MainTextColor, ETextJustify::Right);

		USizeBox* CardSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CardSizeBox->SetWidthOverride(112.0f);
		CardSizeBox->SetHeightOverride(90.0f);
		CardSizeBox->AddChild(CardCanvas);

		UBorder* InnerBorder = ConstructSectionBorder(WidgetTree, NAME_None, CardSizeBox, CardColor, FMargin(0.0f));
		UBorder* OuterBorder = ConstructSectionBorder(WidgetTree, NAME_None, InnerBorder, FLinearColor(0.005f, 0.006f, 0.007f, 1.0f), FMargin(3.0f));
		if (UHorizontalBoxSlot* Slot = SlotBox->AddChildToHorizontalBox(OuterBorder))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddInventoryInfoCard(
		UWidgetTree* WidgetTree,
		UHorizontalBox* SlotBox,
		const FString& Text,
		const FLinearColor& AccentColor)
	{
		if (!WidgetTree || !SlotBox)
		{
			return;
		}

		UCanvasPanel* CardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		AddInventoryCardText(WidgetTree, CardCanvas, TEXT("Info"), FVector2D(7.0f, 5.0f), FVector2D(98.0f, 18.0f), 9, AccentColor);

		UTextBlock* CenterTextBlock = ConstructTextBlock(WidgetTree, NAME_None, 11, FLinearColor(0.90f, 0.94f, 0.96f, 1.0f));
		CenterTextBlock->SetText(FText::FromString(Text));
		CenterTextBlock->SetJustification(ETextJustify::Center);
		AddWidgetToCanvas(CardCanvas, CenterTextBlock, FVector2D(8.0f, 30.0f), FVector2D(96.0f, 46.0f));

		USizeBox* CardSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CardSizeBox->SetWidthOverride(112.0f);
		CardSizeBox->SetHeightOverride(90.0f);
		CardSizeBox->AddChild(CardCanvas);

		UBorder* InnerBorder = ConstructSectionBorder(WidgetTree, NAME_None, CardSizeBox, FLinearColor(0.145f, 0.170f, 0.190f, 0.98f), FMargin(0.0f));
		UBorder* OuterBorder = ConstructSectionBorder(WidgetTree, NAME_None, InnerBorder, FLinearColor(0.005f, 0.006f, 0.007f, 1.0f), FMargin(3.0f));
		if (UHorizontalBoxSlot* Slot = SlotBox->AddChildToHorizontalBox(OuterBorder))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	UButton* ConstructDebugInputButton(UWidgetTree* WidgetTree, const FName& ButtonName, const FText& Label)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		Button->SetBackgroundColor(FLinearColor(0.16f, 0.22f, 0.28f, 0.95f));

		UTextBlock* LabelTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		LabelTextBlock->SetText(Label);
		LabelTextBlock->SetJustification(ETextJustify::Center);
		LabelTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		FSlateFontInfo FontInfo = LabelTextBlock->GetFont();
		FontInfo.Size = 12;
		LabelTextBlock->SetFont(FontInfo);
		Button->AddChild(LabelTextBlock);
		return Button;
	}

	bool IsWidgetUnderScreenPosition(const UWidget* Widget, const FVector2D& ScreenPosition)
	{
		return IsValid(Widget)
			&& Widget->IsVisible()
			&& Widget->GetCachedGeometry().IsUnderLocation(ScreenPosition);
	}

	void ClearHubRouteButtonsAndActions(
		UHorizontalBox* ButtonBox,
		TArray<TObjectPtr<USRHubRouteDestinationAction>>& DestinationActions,
		TArray<TObjectPtr<USRHubRouteLaunchAction>>& LaunchActions,
		TArray<TObjectPtr<USRHubRouteRemovalAction>>& RemovalActions,
		TArray<TObjectPtr<USRHubRouteDebugOrbitAction>>& DebugOrbitActions,
		TArray<TObjectPtr<USRHubRouteSettingAction>>& SettingActions)
	{
		if (ButtonBox)
		{
			ButtonBox->ClearChildren();
		}

		DestinationActions.Reset();
		LaunchActions.Reset();
		RemovalActions.Reset();
		DebugOrbitActions.Reset();
		SettingActions.Reset();
	}

	void BindInputSlotDebugButton(
		UButton* Button,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRFacilityInputSlotDebugAction>>& OutActions,
		int32 InputPortIndex,
		FName ResourceId)
	{
		if (!Button || !OwnerWidget || InputPortIndex == INDEX_NONE || ResourceId.IsNone())
		{
			return;
		}

		USRFacilityInputSlotDebugAction* Action = NewObject<USRFacilityInputSlotDebugAction>(OwnerWidget);
		Action->Initialize(OwnerWidget, InputPortIndex, ResourceId);
		OutActions.Add(Action);
		Button->OnClicked.AddDynamic(Action, &USRFacilityInputSlotDebugAction::HandleClicked);
	}

	void AddHubDestinationButton(
		UWidgetTree* WidgetTree,
		UHorizontalBox* ButtonBox,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRHubRouteDestinationAction>>& OutActions,
		const FSRSpaceLogisticsHubEndpoint& DestinationHub,
		const FString& Label,
		bool bSelected,
		bool bEnabled)
	{
		if (!WidgetTree || !ButtonBox || !OwnerWidget)
		{
			return;
		}

		UButton* Button = ConstructDebugInputButton(WidgetTree, NAME_None, FText::FromString(Label));
		Button->SetBackgroundColor(bSelected
			? FLinearColor(0.095f, 0.220f, 0.180f, 0.95f)
			: bEnabled
			? FLinearColor(0.105f, 0.165f, 0.210f, 0.95f)
			: FLinearColor(0.060f, 0.066f, 0.072f, 0.95f));
		Button->SetIsEnabled(bEnabled);

		USRHubRouteDestinationAction* Action = NewObject<USRHubRouteDestinationAction>(OwnerWidget);
		Action->Initialize(OwnerWidget, DestinationHub, Button);
		OutActions.Add(Action);
		Button->OnClicked.AddDynamic(Action, &USRHubRouteDestinationAction::HandleClicked);

		USizeBox* ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		ButtonSizeBox->SetWidthOverride(176.0f);
		ButtonSizeBox->SetHeightOverride(64.0f);
		ButtonSizeBox->AddChild(Button);

		if (UHorizontalBoxSlot* ButtonSlot = ButtonBox->AddChildToHorizontalBox(ButtonSizeBox))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddHubRouteLaunchButton(
		UWidgetTree* WidgetTree,
		UHorizontalBox* ButtonBox,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRHubRouteLaunchAction>>& OutActions,
		const FSRSpaceLogisticsHubEndpoint& DestinationHub,
		bool bEnabled)
	{
		if (!WidgetTree || !ButtonBox || !OwnerWidget)
		{
			return;
		}

		UButton* Button = ConstructDebugInputButton(
			WidgetTree,
			NAME_None,
			NSLOCTEXT("StarRoversFacilityControl", "HubRouteLaunchButton", "Launch\nRoute"));
		Button->SetBackgroundColor(bEnabled
			? FLinearColor(0.070f, 0.180f, 0.115f, 0.95f)
			: FLinearColor(0.060f, 0.066f, 0.072f, 0.95f));
		Button->SetIsEnabled(bEnabled);

		USRHubRouteLaunchAction* Action = NewObject<USRHubRouteLaunchAction>(OwnerWidget);
		Action->Initialize(OwnerWidget, DestinationHub, Button);
		OutActions.Add(Action);
		Button->OnClicked.AddDynamic(Action, &USRHubRouteLaunchAction::HandleClicked);

		USizeBox* ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		ButtonSizeBox->SetWidthOverride(104.0f);
		ButtonSizeBox->SetHeightOverride(64.0f);
		ButtonSizeBox->AddChild(Button);

		if (UHorizontalBoxSlot* ButtonSlot = ButtonBox->AddChildToHorizontalBox(ButtonSizeBox))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddHubRouteRemoveButton(
		UWidgetTree* WidgetTree,
		UHorizontalBox* ButtonBox,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRHubRouteRemovalAction>>& OutActions,
		FName RouteId)
	{
		if (!WidgetTree || !ButtonBox || !OwnerWidget || RouteId.IsNone())
		{
			return;
		}

		UButton* Button = ConstructDebugInputButton(
			WidgetTree,
			NAME_None,
			NSLOCTEXT("StarRoversFacilityControl", "HubRouteRemoveButton", "Remove\nRoute"));
		Button->SetBackgroundColor(FLinearColor(0.240f, 0.075f, 0.070f, 0.95f));

		USRHubRouteRemovalAction* Action = NewObject<USRHubRouteRemovalAction>(OwnerWidget);
		Action->Initialize(OwnerWidget, RouteId, Button);
		OutActions.Add(Action);
		Button->OnClicked.AddDynamic(Action, &USRHubRouteRemovalAction::HandleClicked);

		USizeBox* ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		ButtonSizeBox->SetWidthOverride(88.0f);
		ButtonSizeBox->SetHeightOverride(54.0f);
		ButtonSizeBox->AddChild(Button);

		if (UHorizontalBoxSlot* ButtonSlot = ButtonBox->AddChildToHorizontalBox(ButtonSizeBox))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddHubRouteDebugOrbitButton(
		UWidgetTree* WidgetTree,
		UHorizontalBox* ButtonBox,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRHubRouteDebugOrbitAction>>& OutActions,
		bool bEnabled)
	{
		if (!WidgetTree || !ButtonBox || !OwnerWidget)
		{
			return;
		}

		UButton* Button = ConstructDebugInputButton(
			WidgetTree,
			NAME_None,
			NSLOCTEXT("StarRoversFacilityControl", "HubRouteDebugOrbitButton", "Debug\nOrbit"));
		Button->SetBackgroundColor(bEnabled
			? FLinearColor(0.120f, 0.090f, 0.190f, 0.95f)
			: FLinearColor(0.060f, 0.066f, 0.072f, 0.95f));
		Button->SetIsEnabled(bEnabled);

		USRHubRouteDebugOrbitAction* Action = NewObject<USRHubRouteDebugOrbitAction>(OwnerWidget);
		Action->Initialize(OwnerWidget, Button);
		OutActions.Add(Action);
		Button->OnClicked.AddDynamic(Action, &USRHubRouteDebugOrbitAction::HandleClicked);

		USizeBox* ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		ButtonSizeBox->SetWidthOverride(104.0f);
		ButtonSizeBox->SetHeightOverride(54.0f);
		ButtonSizeBox->AddChild(Button);

		if (UHorizontalBoxSlot* ButtonSlot = ButtonBox->AddChildToHorizontalBox(ButtonSizeBox))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddHubRouteMaxCargoStackCountButton(
		UWidgetTree* WidgetTree,
		UHorizontalBox* ButtonBox,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRHubRouteSettingAction>>& OutActions,
		FName RouteId,
		int32 NewMaxCargoStackCount)
	{
		if (!WidgetTree || !ButtonBox || !OwnerWidget || RouteId.IsNone())
		{
			return;
		}

		UButton* Button = ConstructDebugInputButton(
			WidgetTree,
			NAME_None,
			FText::FromString(FString::Printf(TEXT("Set\nx%d"), FMath::Max(1, NewMaxCargoStackCount))));
		Button->SetBackgroundColor(FLinearColor(0.080f, 0.130f, 0.165f, 0.95f));

		USRHubRouteSettingAction* Action = NewObject<USRHubRouteSettingAction>(OwnerWidget);
		Action->InitializeMaxCargoStackCount(OwnerWidget, RouteId, NewMaxCargoStackCount, Button);
		OutActions.Add(Action);
		Button->OnClicked.AddDynamic(Action, &USRHubRouteSettingAction::HandleClicked);

		USizeBox* ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		ButtonSizeBox->SetWidthOverride(74.0f);
		ButtonSizeBox->SetHeightOverride(54.0f);
		ButtonSizeBox->AddChild(Button);

		if (UHorizontalBoxSlot* ButtonSlot = ButtonBox->AddChildToHorizontalBox(ButtonSizeBox))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddHubRouteReturnEmptyButton(
		UWidgetTree* WidgetTree,
		UHorizontalBox* ButtonBox,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRHubRouteSettingAction>>& OutActions,
		FName RouteId,
		bool bNewReturnEmptyWhenNoCargo)
	{
		if (!WidgetTree || !ButtonBox || !OwnerWidget || RouteId.IsNone())
		{
			return;
		}

		UButton* Button = ConstructDebugInputButton(
			WidgetTree,
			NAME_None,
			bNewReturnEmptyWhenNoCargo
				? NSLOCTEXT("StarRoversFacilityControl", "HubRouteReturnEmptyOnButton", "Set\nON")
				: NSLOCTEXT("StarRoversFacilityControl", "HubRouteReturnEmptyOffButton", "Set\nOFF"));
		Button->SetBackgroundColor(bNewReturnEmptyWhenNoCargo
			? FLinearColor(0.070f, 0.145f, 0.105f, 0.95f)
			: FLinearColor(0.145f, 0.105f, 0.070f, 0.95f));

		USRHubRouteSettingAction* Action = NewObject<USRHubRouteSettingAction>(OwnerWidget);
		Action->InitializeReturnEmptyWhenNoCargo(OwnerWidget, RouteId, bNewReturnEmptyWhenNoCargo, Button);
		OutActions.Add(Action);
		Button->OnClicked.AddDynamic(Action, &USRHubRouteSettingAction::HandleClicked);

		USizeBox* ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		ButtonSizeBox->SetWidthOverride(78.0f);
		ButtonSizeBox->SetHeightOverride(54.0f);
		ButtonSizeBox->AddChild(Button);

		if (UHorizontalBoxSlot* ButtonSlot = ButtonBox->AddChildToHorizontalBox(ButtonSizeBox))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddHubRouteCargoResourceButton(
		UWidgetTree* WidgetTree,
		UHorizontalBox* ButtonBox,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRHubRouteSettingAction>>& OutActions,
		FName RouteId,
		FName CargoResourceId)
	{
		if (!WidgetTree || !ButtonBox || !OwnerWidget || RouteId.IsNone())
		{
			return;
		}

		const FString Label = CargoResourceId.IsNone()
			? FString(TEXT("Any\nCargo"))
			: FString::Printf(TEXT("Only\n%s"), *BuildCompactResourceIdLabel(CargoResourceId));
		UButton* Button = ConstructDebugInputButton(WidgetTree, NAME_None, FText::FromString(Label));
		Button->SetBackgroundColor(CargoResourceId.IsNone()
			? FLinearColor(0.095f, 0.095f, 0.135f, 0.95f)
			: FLinearColor(0.120f, 0.105f, 0.165f, 0.95f));

		USRHubRouteSettingAction* Action = NewObject<USRHubRouteSettingAction>(OwnerWidget);
		Action->InitializeCargoResourceId(OwnerWidget, RouteId, CargoResourceId, Button);
		OutActions.Add(Action);
		Button->OnClicked.AddDynamic(Action, &USRHubRouteSettingAction::HandleClicked);

		USizeBox* ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		ButtonSizeBox->SetWidthOverride(86.0f);
		ButtonSizeBox->SetHeightOverride(54.0f);
		ButtonSizeBox->AddChild(Button);

		if (UHorizontalBoxSlot* ButtonSlot = ButtonBox->AddChildToHorizontalBox(ButtonSizeBox))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddInputResourceSlotCard(
		UWidgetTree* WidgetTree,
		UVerticalBox* SlotBox,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRFacilityInputSlotDebugAction>>& OutActions,
		const FString& Text,
		int32 InputPortIndex,
		bool bCanAddResource,
		bool bShowDebugButtons)
	{
		if (!WidgetTree || !SlotBox)
		{
			return;
		}

		UVerticalBox* CardBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UTextBlock* SlotTextBlock = ConstructTextBlock(WidgetTree, NAME_None, 11, FLinearColor(0.84f, 0.91f, 1.0f, 1.0f));
		SlotTextBlock->SetText(FText::FromString(Text));
		if (UVerticalBoxSlot* TextSlot = CardBox->AddChildToVerticalBox(SlotTextBlock))
		{
			TextSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
			TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		if (bShowDebugButtons)
		{
			UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			struct FSRFacilityInputDebugButtonSpec
			{
				FName ResourceId;
				FText Label;
			};
			const FSRFacilityInputDebugButtonSpec ButtonSpecs[] =
			{
				{ TEXT("Territe"), NSLOCTEXT("StarRoversFacilityControl", "SlotAddTerrite", "+T") },
				{ TEXT("Aquid"), NSLOCTEXT("StarRoversFacilityControl", "SlotAddAquid", "+A") },
				{ TEXT("Nitain"), NSLOCTEXT("StarRoversFacilityControl", "SlotAddNitain", "+N") },
			};

			for (int32 ButtonIndex = 0; ButtonIndex < UE_ARRAY_COUNT(ButtonSpecs); ++ButtonIndex)
			{
				UButton* Button = ConstructDebugInputButton(WidgetTree, NAME_None, ButtonSpecs[ButtonIndex].Label);
				Button->SetIsEnabled(bCanAddResource);
				BindInputSlotDebugButton(Button, OwnerWidget, OutActions, InputPortIndex, ButtonSpecs[ButtonIndex].ResourceId);
				if (UHorizontalBoxSlot* ButtonSlot = ButtonRow->AddChildToHorizontalBox(Button))
				{
					ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, ButtonIndex < UE_ARRAY_COUNT(ButtonSpecs) - 1 ? 4.0f : 0.0f, 0.0f));
					ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				}
			}
			if (UVerticalBoxSlot* ButtonRowSlot = CardBox->AddChildToVerticalBox(ButtonRow))
			{
				ButtonRowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			}
		}

		UBorder* SlotBorder = ConstructSectionBorder(
			WidgetTree,
			NAME_None,
			CardBox,
			bCanAddResource ? FLinearColor(0.080f, 0.105f, 0.135f, 0.98f) : FLinearColor(0.060f, 0.070f, 0.082f, 0.98f),
			FMargin(7.0f, 5.0f));
		if (UVerticalBoxSlot* Slot = SlotBox->AddChildToVerticalBox(SlotBorder))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}

	FSRResourceInstance MakeDebugEnergyResource(FName ResourceId, double EnergyValue, int32 RemainingProcessLimit)
	{
		FSRResourceInstance ResourceInstance;
		ResourceInstance.ResourceInstanceId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		ResourceInstance.ResourceId = ResourceId;
		ResourceInstance.ResourceKind = ESRResourceKind::Energy;
		ResourceInstance.EnergyValue = EnergyValue;
		ResourceInstance.CatalystOperator = ESRResourceCatalystOperator::None;
		ResourceInstance.RemainingProcessLimit = FMath::Max(0, RemainingProcessLimit);
		ResourceInstance.ProcessCount = 0;
		ResourceInstance.StackCount = 1;
		return ResourceInstance;
	}

	FSRResourceInstance MakeDebugCatalystResource(FName ResourceId, ESRResourceCatalystOperator CatalystOperator)
	{
		FSRResourceInstance ResourceInstance;
		ResourceInstance.ResourceInstanceId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		ResourceInstance.ResourceId = ResourceId;
		ResourceInstance.ResourceKind = ESRResourceKind::Catalyst;
		ResourceInstance.EnergyValue = 0.0;
		ResourceInstance.CatalystOperator = CatalystOperator;
		ResourceInstance.RemainingProcessLimit = 0;
		ResourceInstance.ProcessCount = 0;
		ResourceInstance.StackCount = 1;
		return ResourceInstance;
	}
}

void USRFacilityInputSlotDebugAction::Initialize(USRFacilityControlWidget* InOwnerWidget, int32 InInputPortIndex, FName InResourceId)
{
	OwnerWidget = InOwnerWidget;
	InputPortIndex = InInputPortIndex;
	ResourceId = InResourceId;
}

void USRFacilityInputSlotDebugAction::HandleClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl InputSlotDebug OnClicked InputPortIndex=%d ResourceId=%s"),
		InputPortIndex,
		*ResourceId.ToString());

	if (IsValid(OwnerWidget))
	{
		OwnerWidget->AddDebugInputResourceToPort(InputPortIndex, ResourceId);
	}
}

void USRHubRouteDestinationAction::Initialize(
	USRFacilityControlWidget* InOwnerWidget,
	const FSRSpaceLogisticsHubEndpoint& InDestinationHub,
	UButton* InButton)
{
	OwnerWidget = InOwnerWidget;
	DestinationHub = InDestinationHub;
	Button = InButton;
}

void USRHubRouteDestinationAction::HandleClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl HubRouteDestination OnClicked Destination=%s/%s"),
		*GetNameSafe(DestinationHub.BodyActor.Get()),
		*DestinationHub.HubOccupantId.ToString());

	if (IsValid(OwnerWidget))
	{
		OwnerWidget->SelectRouteDestinationHubEndpoint(DestinationHub);
	}
}

bool USRHubRouteDestinationAction::TryHandleManualClick(const FVector2D& ScreenPosition)
{
	if (!IsValid(Button.Get())
		|| !Button->GetIsEnabled()
		|| !IsWidgetUnderScreenPosition(Button.Get(), ScreenPosition))
	{
		return false;
	}

	HandleClicked();
	return true;
}

void USRHubRouteLaunchAction::Initialize(
	USRFacilityControlWidget* InOwnerWidget,
	const FSRSpaceLogisticsHubEndpoint& InDestinationHub,
	UButton* InButton)
{
	OwnerWidget = InOwnerWidget;
	DestinationHub = InDestinationHub;
	Button = InButton;
}

void USRHubRouteLaunchAction::HandleClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl HubRouteLaunch OnClicked Destination=%s/%s"),
		*GetNameSafe(DestinationHub.BodyActor.Get()),
		*DestinationHub.HubOccupantId.ToString());

	if (IsValid(OwnerWidget))
	{
		OwnerWidget->CreateRouteToHubEndpoint(DestinationHub);
	}
}

bool USRHubRouteLaunchAction::TryHandleManualClick(const FVector2D& ScreenPosition)
{
	if (!IsValid(Button.Get())
		|| !Button->GetIsEnabled()
		|| !IsWidgetUnderScreenPosition(Button.Get(), ScreenPosition))
	{
		return false;
	}

	HandleClicked();
	return true;
}

void USRHubRouteRemovalAction::Initialize(USRFacilityControlWidget* InOwnerWidget, FName InRouteId, UButton* InButton)
{
	OwnerWidget = InOwnerWidget;
	RouteId = InRouteId;
	Button = InButton;
}

void USRHubRouteRemovalAction::HandleClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl HubRouteRemoval OnClicked RouteId=%s"),
		*RouteId.ToString());

	if (IsValid(OwnerWidget))
	{
		OwnerWidget->RemoveHubRoute(RouteId);
	}
}

bool USRHubRouteRemovalAction::TryHandleManualClick(const FVector2D& ScreenPosition)
{
	if (!IsValid(Button.Get())
		|| !Button->GetIsEnabled()
		|| !IsWidgetUnderScreenPosition(Button.Get(), ScreenPosition))
	{
		return false;
	}

	HandleClicked();
	return true;
}

void USRHubRouteDebugOrbitAction::Initialize(USRFacilityControlWidget* InOwnerWidget, UButton* InButton)
{
	OwnerWidget = InOwnerWidget;
	Button = InButton;
}

void USRHubRouteDebugOrbitAction::HandleClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl HubRouteDebugOrbit OnClicked"));

	if (IsValid(OwnerWidget))
	{
		OwnerWidget->LaunchDebugLocalOrbitRoute();
	}
}

bool USRHubRouteDebugOrbitAction::TryHandleManualClick(const FVector2D& ScreenPosition)
{
	if (!IsValid(Button.Get())
		|| !Button->GetIsEnabled()
		|| !IsWidgetUnderScreenPosition(Button.Get(), ScreenPosition))
	{
		return false;
	}

	HandleClicked();
	return true;
}

void USRHubRouteSettingAction::InitializeMaxCargoStackCount(
	USRFacilityControlWidget* InOwnerWidget,
	FName InRouteId,
	int32 InMaxCargoStackCount,
	UButton* InButton)
{
	OwnerWidget = InOwnerWidget;
	RouteId = InRouteId;
	MaxCargoStackCount = FMath::Max(1, InMaxCargoStackCount);
	bSetMaxCargoStackCount = true;
	bSetReturnEmptyWhenNoCargo = false;
	bSetCargoResourceId = false;
	Button = InButton;
}

void USRHubRouteSettingAction::InitializeReturnEmptyWhenNoCargo(
	USRFacilityControlWidget* InOwnerWidget,
	FName InRouteId,
	bool bInReturnEmptyWhenNoCargo,
	UButton* InButton)
{
	OwnerWidget = InOwnerWidget;
	RouteId = InRouteId;
	bReturnEmptyWhenNoCargo = bInReturnEmptyWhenNoCargo;
	bSetMaxCargoStackCount = false;
	bSetReturnEmptyWhenNoCargo = true;
	bSetCargoResourceId = false;
	Button = InButton;
}

void USRHubRouteSettingAction::InitializeCargoResourceId(
	USRFacilityControlWidget* InOwnerWidget,
	FName InRouteId,
	FName InCargoResourceId,
	UButton* InButton)
{
	OwnerWidget = InOwnerWidget;
	RouteId = InRouteId;
	CargoResourceId = InCargoResourceId;
	bSetMaxCargoStackCount = false;
	bSetReturnEmptyWhenNoCargo = false;
	bSetCargoResourceId = true;
	Button = InButton;
}

void USRHubRouteSettingAction::HandleClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl HubRouteSetting OnClicked RouteId=%s SetStack=%s Stack=%d SetReturnEmpty=%s ReturnEmpty=%s SetCargoResource=%s CargoResourceId=%s"),
		*RouteId.ToString(),
		bSetMaxCargoStackCount ? TEXT("true") : TEXT("false"),
		MaxCargoStackCount,
		bSetReturnEmptyWhenNoCargo ? TEXT("true") : TEXT("false"),
		bReturnEmptyWhenNoCargo ? TEXT("true") : TEXT("false"),
		bSetCargoResourceId ? TEXT("true") : TEXT("false"),
		CargoResourceId.IsNone() ? TEXT("Any") : *CargoResourceId.ToString());

	if (!IsValid(OwnerWidget))
	{
		return;
	}

	if (bSetMaxCargoStackCount)
	{
		OwnerWidget->SetHubRouteMaxCargoStackCount(RouteId, MaxCargoStackCount);
	}
	else if (bSetReturnEmptyWhenNoCargo)
	{
		OwnerWidget->SetHubRouteReturnEmptyWhenNoCargo(RouteId, bReturnEmptyWhenNoCargo);
	}
	else if (bSetCargoResourceId)
	{
		OwnerWidget->SetHubRouteCargoResourceId(RouteId, CargoResourceId);
	}
}

bool USRHubRouteSettingAction::TryHandleManualClick(const FVector2D& ScreenPosition)
{
	if (!IsValid(Button.Get())
		|| !Button->GetIsEnabled()
		|| !IsWidgetUnderScreenPosition(Button.Get(), ScreenPosition))
	{
		return false;
	}

	HandleClicked();
	return true;
}

TSharedRef<SWidget> USRFacilityControlWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	BuildFacilityControlWidgetTree();
	return Super::RebuildWidget();
}

void USRFacilityControlWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildFacilityControlWidgetTree();
	BindControlHandlers();
	RefreshControlText();
}

void USRFacilityControlWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	BuildFacilityControlWidgetTree();
	RefreshControlText();
}

void USRFacilityControlWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsVisible() || !bHasFocusedFacility)
	{
		return;
	}

	RefreshControlText();
}

FReply USRFacilityControlWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
	if (IsScreenPositionOverControlPanel(ScreenPosition))
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl NativeOnMouseButtonDown handled Mouse=(%.1f, %.1f)"),
			ScreenPosition.X,
			ScreenPosition.Y);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USRFacilityControlWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
	if (IsScreenPositionOverControlPanel(ScreenPosition))
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl NativeOnMouseButtonUp handled Mouse=(%.1f, %.1f)"),
			ScreenPosition.X,
			ScreenPosition.Y);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USRFacilityControlWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsScreenPositionOverControlPanel(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void USRFacilityControlWidget::SetFocusedFacility(AActor* NewFocusedActor, FName NewOccupantId)
{
	const bool bFocusChanged = FocusedActor.Get() != NewFocusedActor || FocusedOccupantId != NewOccupantId;
	FocusedActor = NewFocusedActor;
	FocusedOccupantId = NewOccupantId;
	bHasFocusedFacility = IsValid(NewFocusedActor) && !NewOccupantId.IsNone();
	if (bFocusChanged)
	{
		LastHubRouteStatus.Reset();
		HubRoutePanelSignature.Reset();
		SelectedHubRouteDestination = FSRSpaceLogisticsHubEndpoint();
		bHasSelectedHubRouteDestination = false;
	}
	RefreshControlText();
}

void USRFacilityControlWidget::ClearFocusedFacility()
{
	FocusedActor.Reset();
	FocusedOccupantId = NAME_None;
	bHasFocusedFacility = false;
	LastHubRouteStatus.Reset();
	HubRoutePanelSignature.Reset();
	SelectedHubRouteDestination = FSRSpaceLogisticsHubEndpoint();
	bHasSelectedHubRouteDestination = false;
	RefreshControlText();
}

bool USRFacilityControlWidget::HasFocusedFacility() const
{
	return bHasFocusedFacility;
}

bool USRFacilityControlWidget::IsPointerOverControlPanel() const
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	return IsScreenPositionOverControlPanel(FSlateApplication::Get().GetCursorPos());
}

bool USRFacilityControlWidget::TryHandleFacilityControlPointerClick()
{
	if (!IsVisible() || !FSlateApplication::IsInitialized())
	{
		return false;
	}

	const FVector2D ScreenPosition = FSlateApplication::Get().GetCursorPos();
	if (!IsScreenPositionOverControlPanel(ScreenPosition))
	{
		return false;
	}

	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl TryHandleFacilityControlPointerClick Mouse=(%.1f, %.1f)"),
		ScreenPosition.X,
		ScreenPosition.Y);

	if (IsWidgetUnderScreenPosition(CloseButton, ScreenPosition) && CloseButton->GetIsEnabled())
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved CloseButton"));
		HandleCloseClicked();
		return true;
	}

	if (IsWidgetUnderScreenPosition(ProcessCheckBox, ScreenPosition) && ProcessCheckBox->GetIsEnabled())
	{
		const bool bNewChecked = !ProcessCheckBox->IsChecked();
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved ProcessCheckBox bNewChecked=%s"),
			bNewChecked ? TEXT("true") : TEXT("false"));
		HandleProcessCheckStateChanged(bNewChecked);
		return true;
	}

	if (IsWidgetUnderScreenPosition(DeliverCheckBox, ScreenPosition) && DeliverCheckBox->GetIsEnabled())
	{
		const bool bNewChecked = !DeliverCheckBox->IsChecked();
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved DeliverCheckBox bNewChecked=%s"),
			bNewChecked ? TEXT("true") : TEXT("false"));
		HandleDeliverCheckStateChanged(bNewChecked);
		return true;
	}

	if (IsWidgetUnderScreenPosition(DebugAddTerriteButton, ScreenPosition) && DebugAddTerriteButton->GetIsEnabled())
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved DebugAddTerriteButton"));
		HandleDebugAddTerriteClicked();
		return true;
	}

	if (IsWidgetUnderScreenPosition(DebugAddAquidButton, ScreenPosition) && DebugAddAquidButton->GetIsEnabled())
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved DebugAddAquidButton"));
		HandleDebugAddAquidClicked();
		return true;
	}

	if (IsWidgetUnderScreenPosition(DebugAddNitainButton, ScreenPosition) && DebugAddNitainButton->GetIsEnabled())
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved DebugAddNitainButton"));
		HandleDebugAddNitainClicked();
		return true;
	}

	for (USRHubRouteDestinationAction* HubRouteDestinationAction : HubRouteDestinationActions)
	{
		if (IsValid(HubRouteDestinationAction) && HubRouteDestinationAction->TryHandleManualClick(ScreenPosition))
		{
			SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved HubRouteDestinationButton"));
			return true;
		}
	}

	for (USRHubRouteLaunchAction* HubRouteLaunchAction : HubRouteLaunchActions)
	{
		if (IsValid(HubRouteLaunchAction) && HubRouteLaunchAction->TryHandleManualClick(ScreenPosition))
		{
			SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved HubRouteLaunchButton"));
			return true;
		}
	}

	for (USRHubRouteRemovalAction* HubRouteRemovalAction : HubRouteRemovalActions)
	{
		if (IsValid(HubRouteRemovalAction) && HubRouteRemovalAction->TryHandleManualClick(ScreenPosition))
		{
			SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved HubRouteRemoveButton"));
			return true;
		}
	}

	for (USRHubRouteDebugOrbitAction* HubRouteDebugOrbitAction : HubRouteDebugOrbitActions)
	{
		if (IsValid(HubRouteDebugOrbitAction) && HubRouteDebugOrbitAction->TryHandleManualClick(ScreenPosition))
		{
			SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved HubRouteDebugOrbitButton"));
			return true;
		}
	}

	for (USRHubRouteSettingAction* HubRouteSettingAction : HubRouteSettingActions)
	{
		if (IsValid(HubRouteSettingAction) && HubRouteSettingAction->TryHandleManualClick(ScreenPosition))
		{
			SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved HubRouteSettingButton"));
			return true;
		}
	}

	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click consumed panel background."));
	return true;
}

bool USRFacilityControlWidget::AddDebugInputResourceToPort(int32 InputPortIndex, FName ResourceId)
{
	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || FocusedOccupantId.IsNone() || InputPortIndex == INDEX_NONE)
	{
		return false;
	}

	FSRResourceInstance ResourceInstance;
	if (ResourceId == FName(TEXT("Territe")))
	{
		ResourceInstance = MakeDebugEnergyResource(TEXT("Territe"), 1.0, 3);
	}
	else if (ResourceId == FName(TEXT("Aquid")))
	{
		ResourceInstance = MakeDebugCatalystResource(TEXT("Aquid"), ESRResourceCatalystOperator::Add);
	}
	else if (ResourceId == FName(TEXT("Nitain")))
	{
		ResourceInstance = MakeDebugCatalystResource(TEXT("Nitain"), ESRResourceCatalystOperator::Multiply);
	}
	else
	{
		return false;
	}

	const bool bAdded = FacilityNetwork->AddInputResourceToPort(FocusedOccupantId, InputPortIndex, ResourceInstance);
	RefreshControlText();
	return bAdded;
}

bool USRFacilityControlWidget::SelectRouteDestinationHubEndpoint(const FSRSpaceLogisticsHubEndpoint& DestinationHub)
{
	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || !FacilityNetwork->IsHubFacility(FocusedOccupantId))
	{
		LastHubRouteStatus = TEXT("Destination select failed: selected facility is not a Hub.");
		RefreshControlText();
		return false;
	}

	if (!DestinationHub.IsValid())
	{
		LastHubRouteStatus = TEXT("Destination select failed: invalid Hub endpoint.");
		RefreshControlText();
		return false;
	}

	USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = GetSpaceLogisticsSubsystem();
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		LastHubRouteStatus = TEXT("Destination select failed: logistics subsystem unavailable.");
		RefreshControlText();
		return false;
	}

	FSRSpaceLogisticsHubEndpoint SourceHub;
	if (!SpaceLogisticsSubsystem->GetHubEndpoint(FocusedActor.Get(), FocusedOccupantId, SourceHub))
	{
		LastHubRouteStatus = TEXT("Destination select failed: source Hub endpoint not found.");
		RefreshControlText();
		return false;
	}

	if (AreHubEndpointKeysEqual(SourceHub, DestinationHub))
	{
		LastHubRouteStatus = TEXT("Destination select failed: select a different Hub.");
		RefreshControlText();
		return false;
	}

	SelectedHubRouteDestination = DestinationHub;
	bHasSelectedHubRouteDestination = true;
	LastHubRouteStatus = FString::Printf(
		TEXT("Destination selected: %s. Press Launch Route."),
		*BuildCelestialBodyDisplayName(DestinationHub.BodyActor.Get()));
	HubRoutePanelSignature.Reset();
	RefreshControlText();
	return true;
}

bool USRFacilityControlWidget::CreateRouteToHubEndpoint(const FSRSpaceLogisticsHubEndpoint& DestinationHub)
{
	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || !FacilityNetwork->IsHubFacility(FocusedOccupantId))
	{
		LastHubRouteStatus = TEXT("Route failed: selected facility is not a Hub.");
		RefreshControlText();
		return false;
	}

	USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = GetSpaceLogisticsSubsystem();
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		LastHubRouteStatus = TEXT("Route failed: logistics subsystem unavailable.");
		RefreshControlText();
		return false;
	}

	FSRSpaceLogisticsHubEndpoint SourceHub;
	if (!SpaceLogisticsSubsystem->GetHubEndpoint(FocusedActor.Get(), FocusedOccupantId, SourceHub))
	{
		LastHubRouteStatus = TEXT("Route failed: source Hub endpoint not found.");
		RefreshControlText();
		return false;
	}

	FName RouteId = NAME_None;
	const bool bCreated = SpaceLogisticsSubsystem->CreateHubRoute(
		SourceHub,
		DestinationHub,
		RouteId,
		true,
		1);
	LastHubRouteStatus = bCreated
		? FString::Printf(TEXT("Route created: %s"), *RouteId.ToString())
		: TEXT("Route failed: endpoint invalid or route already exists.");
	if (bCreated)
	{
		SelectedHubRouteDestination = FSRSpaceLogisticsHubEndpoint();
		bHasSelectedHubRouteDestination = false;
	}
	HubRoutePanelSignature.Reset();
	RefreshControlText();
	return bCreated;
}

bool USRFacilityControlWidget::LaunchDebugLocalOrbitRoute()
{
	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || !FacilityNetwork->IsHubFacility(FocusedOccupantId))
	{
		LastHubRouteStatus = TEXT("Debug orbit failed: selected facility is not a Hub.");
		RefreshControlText();
		return false;
	}

	USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = GetSpaceLogisticsSubsystem();
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		LastHubRouteStatus = TEXT("Debug orbit failed: logistics subsystem unavailable.");
		RefreshControlText();
		return false;
	}

	FSRSpaceLogisticsHubEndpoint SourceHub;
	if (!SpaceLogisticsSubsystem->GetHubEndpoint(FocusedActor.Get(), FocusedOccupantId, SourceHub))
	{
		LastHubRouteStatus = TEXT("Debug orbit failed: source Hub endpoint not found.");
		RefreshControlText();
		return false;
	}

	FName RouteId = NAME_None;
	const bool bCreated = SpaceLogisticsSubsystem->CreateDebugLocalOrbitRoute(
		SourceHub,
		RouteId);
	LastHubRouteStatus = bCreated
		? FString::Printf(TEXT("Debug orbit launched: %s"), *RouteId.ToString())
		: TEXT("Debug orbit failed: already active or endpoint invalid.");
	HubRoutePanelSignature.Reset();
	RefreshControlText();
	return bCreated;
}

bool USRFacilityControlWidget::RemoveHubRoute(FName RouteId)
{
	if (RouteId.IsNone())
	{
		LastHubRouteStatus = TEXT("Remove failed: invalid route.");
		RefreshControlText();
		return false;
	}

	USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = GetSpaceLogisticsSubsystem();
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		LastHubRouteStatus = TEXT("Remove failed: logistics subsystem unavailable.");
		RefreshControlText();
		return false;
	}

	const bool bRemoved = SpaceLogisticsSubsystem->RemoveHubRoute(RouteId);
	LastHubRouteStatus = bRemoved
		? FString::Printf(TEXT("Route removed: %s"), *RouteId.ToString())
		: FString::Printf(TEXT("Remove failed: %s"), *RouteId.ToString());
	HubRoutePanelSignature.Reset();
	RefreshControlText();
	return bRemoved;
}

bool USRFacilityControlWidget::SetHubRouteMaxCargoStackCount(FName RouteId, int32 MaxCargoStackCount)
{
	if (RouteId.IsNone())
	{
		LastHubRouteStatus = TEXT("Stack update failed: invalid route.");
		RefreshControlText();
		return false;
	}

	USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = GetSpaceLogisticsSubsystem();
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		LastHubRouteStatus = TEXT("Stack update failed: logistics subsystem unavailable.");
		RefreshControlText();
		return false;
	}

	const int32 ClampedMaxCargoStackCount = FMath::Max(1, MaxCargoStackCount);
	const bool bUpdated = SpaceLogisticsSubsystem->SetHubRouteMaxCargoStackCount(RouteId, ClampedMaxCargoStackCount);
	LastHubRouteStatus = bUpdated
		? FString::Printf(TEXT("Route stack updated: x%d"), ClampedMaxCargoStackCount)
		: FString::Printf(TEXT("Stack update failed: %s"), *RouteId.ToString());
	HubRoutePanelSignature.Reset();
	RefreshControlText();
	return bUpdated;
}

bool USRFacilityControlWidget::SetHubRouteReturnEmptyWhenNoCargo(FName RouteId, bool bReturnEmptyWhenNoCargo)
{
	if (RouteId.IsNone())
	{
		LastHubRouteStatus = TEXT("Return setting failed: invalid route.");
		RefreshControlText();
		return false;
	}

	USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = GetSpaceLogisticsSubsystem();
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		LastHubRouteStatus = TEXT("Return setting failed: logistics subsystem unavailable.");
		RefreshControlText();
		return false;
	}

	const bool bUpdated = SpaceLogisticsSubsystem->SetHubRouteReturnEmptyWhenNoCargo(RouteId, bReturnEmptyWhenNoCargo);
	LastHubRouteStatus = bUpdated
		? FString::Printf(TEXT("Empty return: %s"), bReturnEmptyWhenNoCargo ? TEXT("ON") : TEXT("OFF"))
		: FString::Printf(TEXT("Return setting failed: %s"), *RouteId.ToString());
	HubRoutePanelSignature.Reset();
	RefreshControlText();
	return bUpdated;
}

bool USRFacilityControlWidget::SetHubRouteCargoResourceId(FName RouteId, FName CargoResourceId)
{
	if (RouteId.IsNone())
	{
		LastHubRouteStatus = TEXT("Cargo filter failed: invalid route.");
		RefreshControlText();
		return false;
	}

	USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = GetSpaceLogisticsSubsystem();
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		LastHubRouteStatus = TEXT("Cargo filter failed: logistics subsystem unavailable.");
		RefreshControlText();
		return false;
	}

	const bool bUpdated = SpaceLogisticsSubsystem->SetHubRouteCargoResourceId(RouteId, CargoResourceId);
	LastHubRouteStatus = bUpdated
		? FString::Printf(TEXT("Cargo filter: %s"), CargoResourceId.IsNone() ? TEXT("Any") : *CargoResourceId.ToString())
		: FString::Printf(TEXT("Cargo filter failed: %s"), *RouteId.ToString());
	HubRoutePanelSignature.Reset();
	RefreshControlText();
	return bUpdated;
}

USRFacilityNetworkComponent* USRFacilityControlWidget::GetFocusedFacilityNetwork() const
{
	AActor* Actor = FocusedActor.Get();
	return IsValid(Actor) ? Actor->FindComponentByClass<USRFacilityNetworkComponent>() : nullptr;
}

USRSpaceLogisticsSubsystem* USRFacilityControlWidget::GetSpaceLogisticsSubsystem() const
{
	UWorld* World = GetWorld();
	return IsValid(World)
		? World->GetSubsystem<USRSpaceLogisticsSubsystem>()
		: nullptr;
}

bool USRFacilityControlWidget::IsScreenPositionOverControlPanel(const FVector2D& ScreenPosition) const
{
	return IsVisible()
		&& PanelBorder
		&& PanelBorder->GetCachedGeometry().IsUnderLocation(ScreenPosition);
}

void USRFacilityControlWidget::HandleCloseClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl CloseButton OnClicked"));

	if (ASRPlayerController* PlayerController = Cast<ASRPlayerController>(GetOwningPlayer()))
	{
		PlayerController->ClearFacilityFocus();
		return;
	}

	ClearFocusedFacility();
}

void USRFacilityControlWidget::HandleProcessCheckStateChanged(bool bIsChecked)
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl ProcessCheckBox changed bIsChecked=%s"),
		bIsChecked ? TEXT("true") : TEXT("false"));

	if (bUpdatingControls)
	{
		return;
	}

	if (USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork())
	{
		FacilityNetwork->SetFacilityProcessEnabled(FocusedOccupantId, bIsChecked);
	}
	RefreshControlText();
}

void USRFacilityControlWidget::HandleDeliverCheckStateChanged(bool bIsChecked)
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl DeliverCheckBox changed bIsChecked=%s"),
		bIsChecked ? TEXT("true") : TEXT("false"));

	if (bUpdatingControls)
	{
		return;
	}

	if (USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork())
	{
		FacilityNetwork->SetFacilityDeliverEnabled(FocusedOccupantId, bIsChecked);
	}
	RefreshControlText();
}

void USRFacilityControlWidget::HandleDebugAddTerriteClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl DebugAddTerriteButton OnClicked"));

	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || FocusedOccupantId.IsNone())
	{
		return;
	}

	FacilityNetwork->AddInputResource(
		FocusedOccupantId,
		MakeDebugEnergyResource(TEXT("Territe"), 1.0, 3));
	RefreshControlText();
}

void USRFacilityControlWidget::HandleDebugAddAquidClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl DebugAddAquidButton OnClicked"));

	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || FocusedOccupantId.IsNone())
	{
		return;
	}

	FacilityNetwork->AddInputResource(
		FocusedOccupantId,
		MakeDebugCatalystResource(TEXT("Aquid"), ESRResourceCatalystOperator::Add));
	RefreshControlText();
}

void USRFacilityControlWidget::HandleDebugAddNitainClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl DebugAddNitainButton OnClicked"));

	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || FocusedOccupantId.IsNone())
	{
		return;
	}

	FacilityNetwork->AddInputResource(
		FocusedOccupantId,
		MakeDebugCatalystResource(TEXT("Nitain"), ESRResourceCatalystOperator::Multiply));
	RefreshControlText();
}

void USRFacilityControlWidget::BuildFacilityControlWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	if (WidgetTree->RootWidget)
	{
		PanelBorder = Cast<UBorder>(WidgetTree->FindWidget(FName(TEXT("FacilityControlPanelBorder"))));
		TitleTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlTitleTextBlock"))));
		CloseButton = Cast<UButton>(WidgetTree->FindWidget(FName(TEXT("FacilityControlCloseButton"))));
		ProcessCheckBox = Cast<UCheckBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlProcessCheckBox"))));
		ProcessStatusTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlProcessStatusTextBlock"))));
		InputResourceTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlInputResourceTextBlock"))));
		InputResourceSlotBox = Cast<UHorizontalBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlInputResourceSlotBox"))));
		EffectsTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlEffectsTextBlock"))));
		ProcessProgressBar = Cast<UProgressBar>(WidgetTree->FindWidget(FName(TEXT("FacilityControlProcessProgressBar"))));
		ProcessTimeTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlProcessTimeTextBlock"))));
		OutputPreviewTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlOutputPreviewTextBlock"))));
		OutputResourceSlotBox = Cast<UHorizontalBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlOutputResourceSlotBox"))));
		InputInventoryTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlInputInventoryTextBlock"))));
		InputInventorySlotBox = Cast<UHorizontalBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlInputInventorySlotBox"))));
		OutputInventoryTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlOutputInventoryTextBlock"))));
		OutputInventorySlotBox = Cast<UHorizontalBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlOutputInventorySlotBox"))));
		DebugAddTerriteButton = Cast<UButton>(WidgetTree->FindWidget(FName(TEXT("FacilityControlDebugAddTerriteButton"))));
		DebugAddAquidButton = Cast<UButton>(WidgetTree->FindWidget(FName(TEXT("FacilityControlDebugAddAquidButton"))));
		DebugAddNitainButton = Cast<UButton>(WidgetTree->FindWidget(FName(TEXT("FacilityControlDebugAddNitainButton"))));
		DeliverCheckBox = Cast<UCheckBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlDeliverCheckBox"))));
		DeliverStatusTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlDeliverStatusTextBlock"))));
		HubRouteTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlHubRouteTextBlock"))));
		HubDestinationButtonBox = Cast<UHorizontalBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlHubDestinationButtonBox"))));
		HubRouteStatusTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlHubRouteStatusTextBlock"))));
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FacilityControlCanvasPanel"));
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FacilityControlPanelBorder"));
	PanelBorder->SetPadding(FMargin(16.0f));
	PanelBorder->SetBrushColor(FLinearColor(0.025f, 0.032f, 0.040f, 0.96f));
	if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(PanelBorder))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetPosition(FVector2D(0.0f, 18.0f));
		PanelSlot->SetSize(FVector2D(920.0f, 780.0f));
	}

	UCanvasPanel* PanelCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FacilityControlPanelCanvas"));
	PanelCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PanelBorder->SetContent(PanelCanvas);

	TitleTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlTitleTextBlock"), 18, FLinearColor::White);
	TitleTextBlock->SetJustification(ETextJustify::Center);
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(
			WidgetTree,
			TEXT("FacilityControlTitleBorder"),
			TitleTextBlock,
			FLinearColor(0.055f, 0.070f, 0.085f, 0.98f),
			FMargin(12.0f, 8.0f)),
		FVector2D(16.0f, 12.0f),
		FVector2D(816.0f, 56.0f));

	CloseButton = ConstructDebugInputButton(
		WidgetTree,
		TEXT("FacilityControlCloseButton"),
		NSLOCTEXT("StarRoversFacilityControl", "CloseButton", "X"));
	CloseButton->SetBackgroundColor(FLinearColor(0.28f, 0.075f, 0.070f, 0.95f));
	AddWidgetToCanvas(
		PanelCanvas,
		CloseButton,
		FVector2D(842.0f, 12.0f),
		FVector2D(40.0f, 56.0f));

	UHorizontalBox* ProcessRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacilityControlProcessRow"));
	ProcessCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("FacilityControlProcessCheckBox"));
	if (UHorizontalBoxSlot* CheckSlot = ProcessRow->AddChildToHorizontalBox(ProcessCheckBox))
	{
		CheckSlot->SetPadding(FMargin(0.0f, 3.0f, 10.0f, 0.0f));
		CheckSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	ProcessStatusTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlProcessStatusTextBlock"), 15, FLinearColor(0.86f, 0.92f, 0.96f, 1.0f));
	ProcessRow->AddChildToHorizontalBox(ProcessStatusTextBlock);
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(WidgetTree, TEXT("FacilityControlProcessBorder"), ProcessRow, FLinearColor(0.07f, 0.085f, 0.105f, 0.98f), FMargin(14.0f)),
		FVector2D(308.0f, 82.0f),
		FVector2D(264.0f, 62.0f));

	UVerticalBox* InputResourceSectionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlInputResourceSectionBox"));
	InputResourceTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlInputResourceTextBlock"), 13, FLinearColor(0.80f, 0.88f, 1.0f, 1.0f));
	if (UVerticalBoxSlot* InputResourceTitleSlot = InputResourceSectionBox->AddChildToVerticalBox(InputResourceTextBlock))
	{
		InputResourceTitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		InputResourceTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	UScrollBox* InputResourceScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("FacilityControlInputResourceScrollBox"));
	InputResourceScrollBox->SetOrientation(Orient_Horizontal);
	InputResourceSlotBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacilityControlInputResourceSlotBox"));
	InputResourceScrollBox->AddChild(InputResourceSlotBox);
	if (UVerticalBoxSlot* InputResourceSlotBoxSlot = InputResourceSectionBox->AddChildToVerticalBox(InputResourceScrollBox))
	{
		InputResourceSlotBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	EffectsTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlEffectsTextBlock"), 13, FLinearColor(0.96f, 0.90f, 0.72f, 1.0f));
	UVerticalBox* OutputResourceSectionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlOutputResourceSectionBox"));
	OutputPreviewTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlOutputPreviewTextBlock"), 13, FLinearColor(0.78f, 1.0f, 0.86f, 1.0f));
	if (UVerticalBoxSlot* OutputResourceTitleSlot = OutputResourceSectionBox->AddChildToVerticalBox(OutputPreviewTextBlock))
	{
		OutputResourceTitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		OutputResourceTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	UScrollBox* OutputResourceScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("FacilityControlOutputResourceScrollBox"));
	OutputResourceScrollBox->SetOrientation(Orient_Horizontal);
	OutputResourceSlotBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacilityControlOutputResourceSlotBox"));
	OutputResourceScrollBox->AddChild(OutputResourceSlotBox);
	if (UVerticalBoxSlot* OutputResourceSlotBoxSlot = OutputResourceSectionBox->AddChildToVerticalBox(OutputResourceScrollBox))
	{
		OutputResourceSlotBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(WidgetTree, TEXT("FacilityControlInputResourceBorder"), InputResourceSectionBox),
		FVector2D(18.0f, 166.0f),
		FVector2D(266.0f, 190.0f));
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(WidgetTree, TEXT("FacilityControlEffectsBorder"), EffectsTextBlock, FLinearColor(0.085f, 0.080f, 0.060f, 0.96f)),
		FVector2D(308.0f, 166.0f),
		FVector2D(264.0f, 122.0f));
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(WidgetTree, TEXT("FacilityControlOutputPreviewBorder"), OutputResourceSectionBox),
		FVector2D(596.0f, 166.0f),
		FVector2D(266.0f, 190.0f));

	ProcessProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("FacilityControlProcessProgressBar"));
	ProcessProgressBar->SetFillColorAndOpacity(FLinearColor(0.40f, 0.72f, 1.0f, 1.0f));
	UVerticalBox* ProgressBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlProgressBox"));
	if (UVerticalBoxSlot* ProgressSlot = ProgressBox->AddChildToVerticalBox(ProcessProgressBar))
	{
		ProgressSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}
	ProcessTimeTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlProcessTimeTextBlock"), 12, FLinearColor(0.82f, 0.84f, 0.86f, 1.0f));
	ProcessTimeTextBlock->SetJustification(ETextJustify::Center);
	ProgressBox->AddChildToVerticalBox(ProcessTimeTextBlock);
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(
			WidgetTree,
			TEXT("FacilityControlProgressBorder"),
			ProgressBox,
			FLinearColor(0.060f, 0.073f, 0.088f, 0.96f),
			FMargin(10.0f, 9.0f)),
		FVector2D(332.0f, 306.0f),
		FVector2D(216.0f, 58.0f));

	UVerticalBox* InputInventorySectionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlInputInventorySectionBox"));
	InputInventoryTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlInputInventoryTextBlock"), 12, FLinearColor(0.82f, 0.88f, 1.0f, 1.0f));
	if (UVerticalBoxSlot* InputInventoryTitleSlot = InputInventorySectionBox->AddChildToVerticalBox(InputInventoryTextBlock))
	{
		InputInventoryTitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		InputInventoryTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	UScrollBox* InputInventoryScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("FacilityControlInputInventoryScrollBox"));
	InputInventoryScrollBox->SetOrientation(Orient_Horizontal);
	InputInventorySlotBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacilityControlInputInventorySlotBox"));
	InputInventoryScrollBox->AddChild(InputInventorySlotBox);
	if (UVerticalBoxSlot* InputInventorySlotsSlot = InputInventorySectionBox->AddChildToVerticalBox(InputInventoryScrollBox))
	{
		InputInventorySlotsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UVerticalBox* OutputInventorySectionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlOutputInventorySectionBox"));
	OutputInventoryTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlOutputInventoryTextBlock"), 12, FLinearColor(0.82f, 1.0f, 0.88f, 1.0f));
	if (UVerticalBoxSlot* OutputInventoryTitleSlot = OutputInventorySectionBox->AddChildToVerticalBox(OutputInventoryTextBlock))
	{
		OutputInventoryTitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		OutputInventoryTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	UScrollBox* OutputInventoryScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("FacilityControlOutputInventoryScrollBox"));
	OutputInventoryScrollBox->SetOrientation(Orient_Horizontal);
	OutputInventorySlotBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacilityControlOutputInventorySlotBox"));
	OutputInventoryScrollBox->AddChild(OutputInventorySlotBox);
	if (UVerticalBoxSlot* OutputInventorySlotsSlot = OutputInventorySectionBox->AddChildToVerticalBox(OutputInventoryScrollBox))
	{
		OutputInventorySlotsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(WidgetTree, TEXT("FacilityControlInputInventoryBorder"), InputInventorySectionBox),
		FVector2D(18.0f, 386.0f),
		FVector2D(390.0f, 154.0f));
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(WidgetTree, TEXT("FacilityControlOutputInventoryBorder"), OutputInventorySectionBox),
		FVector2D(472.0f, 386.0f),
		FVector2D(390.0f, 154.0f));

	UVerticalBox* DebugInputBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlDebugInputBox"));
	UTextBlock* DebugInputLabelTextBlock = ConstructTextBlock(
		WidgetTree,
		TEXT("FacilityControlDebugInputLabelTextBlock"),
		11,
		FLinearColor(0.92f, 0.82f, 0.64f, 1.0f));
	DebugInputLabelTextBlock->SetText(NSLOCTEXT("StarRoversFacilityControl", "DebugInputLabel", "Debug Input"));
	if (UVerticalBoxSlot* DebugInputLabelSlot = DebugInputBox->AddChildToVerticalBox(DebugInputLabelTextBlock))
	{
		DebugInputLabelSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
	}

	UHorizontalBox* DebugInputRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacilityControlDebugInputRow"));

	DebugAddTerriteButton = ConstructDebugInputButton(
		WidgetTree,
		TEXT("FacilityControlDebugAddTerriteButton"),
		NSLOCTEXT("StarRoversFacilityControl", "DebugAddTerrite", "+ Territe"));
	if (UHorizontalBoxSlot* TerriteButtonSlot = DebugInputRow->AddChildToHorizontalBox(DebugAddTerriteButton))
	{
		TerriteButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		TerriteButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	DebugAddAquidButton = ConstructDebugInputButton(
		WidgetTree,
		TEXT("FacilityControlDebugAddAquidButton"),
		NSLOCTEXT("StarRoversFacilityControl", "DebugAddAquid", "+ Aquid"));
	if (UHorizontalBoxSlot* AquidButtonSlot = DebugInputRow->AddChildToHorizontalBox(DebugAddAquidButton))
	{
		AquidButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		AquidButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	DebugAddNitainButton = ConstructDebugInputButton(
		WidgetTree,
		TEXT("FacilityControlDebugAddNitainButton"),
		NSLOCTEXT("StarRoversFacilityControl", "DebugAddNitain", "+ Nitain"));
	if (UHorizontalBoxSlot* NitainButtonSlot = DebugInputRow->AddChildToHorizontalBox(DebugAddNitainButton))
	{
		NitainButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	DebugInputBox->AddChildToVerticalBox(DebugInputRow);
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(
			WidgetTree,
			TEXT("FacilityControlDebugInputBorder"),
			DebugInputBox,
			FLinearColor(0.070f, 0.065f, 0.050f, 0.96f),
			FMargin(10.0f, 8.0f)),
		FVector2D(18.0f, 556.0f),
		FVector2D(390.0f, 52.0f));

	UHorizontalBox* DeliverRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacilityControlDeliverRow"));
	DeliverCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("FacilityControlDeliverCheckBox"));
	if (UHorizontalBoxSlot* DeliverCheckSlot = DeliverRow->AddChildToHorizontalBox(DeliverCheckBox))
	{
		DeliverCheckSlot->SetPadding(FMargin(0.0f, 3.0f, 10.0f, 0.0f));
		DeliverCheckSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	DeliverStatusTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlDeliverStatusTextBlock"), 14, FLinearColor(0.86f, 0.92f, 0.96f, 1.0f));
	DeliverRow->AddChildToHorizontalBox(DeliverStatusTextBlock);
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(
			WidgetTree,
			TEXT("FacilityControlDeliverBorder"),
			DeliverRow,
			FLinearColor(0.060f, 0.085f, 0.070f, 0.96f),
			FMargin(14.0f)),
		FVector2D(554.0f, 552.0f),
		FVector2D(254.0f, 58.0f));

	UVerticalBox* HubRouteSectionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlHubRouteSectionBox"));
	HubRouteTextBlock = ConstructTextBlock(
		WidgetTree,
		TEXT("FacilityControlHubRouteTextBlock"),
		12,
		FLinearColor(0.78f, 0.92f, 1.0f, 1.0f));
	if (UVerticalBoxSlot* HubRouteTitleSlot = HubRouteSectionBox->AddChildToVerticalBox(HubRouteTextBlock))
	{
		HubRouteTitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		HubRouteTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	UScrollBox* HubDestinationScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("FacilityControlHubDestinationScrollBox"));
	HubDestinationScrollBox->SetOrientation(Orient_Horizontal);
	HubDestinationButtonBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacilityControlHubDestinationButtonBox"));
	HubDestinationScrollBox->AddChild(HubDestinationButtonBox);
	if (UVerticalBoxSlot* HubRouteButtonSlot = HubRouteSectionBox->AddChildToVerticalBox(HubDestinationScrollBox))
	{
		HubRouteButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		HubRouteButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	HubRouteStatusTextBlock = ConstructTextBlock(
		WidgetTree,
		TEXT("FacilityControlHubRouteStatusTextBlock"),
		11,
		FLinearColor(0.82f, 0.86f, 0.90f, 1.0f));
	HubRouteStatusTextBlock->SetAutoWrapText(false);
	HubRouteSectionBox->AddChildToVerticalBox(HubRouteStatusTextBlock);
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(
			WidgetTree,
			TEXT("FacilityControlHubRouteBorder"),
			HubRouteSectionBox,
			FLinearColor(0.050f, 0.072f, 0.090f, 0.96f),
			FMargin(10.0f, 8.0f)),
		FVector2D(18.0f, 622.0f),
		FVector2D(844.0f, 134.0f));
}

void USRFacilityControlWidget::BindControlHandlers()
{
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &USRFacilityControlWidget::HandleCloseClicked);
		CloseButton->OnClicked.AddDynamic(this, &USRFacilityControlWidget::HandleCloseClicked);
	}
	if (ProcessCheckBox)
	{
		ProcessCheckBox->OnCheckStateChanged.RemoveDynamic(this, &USRFacilityControlWidget::HandleProcessCheckStateChanged);
		ProcessCheckBox->OnCheckStateChanged.AddDynamic(this, &USRFacilityControlWidget::HandleProcessCheckStateChanged);
	}
	if (DeliverCheckBox)
	{
		DeliverCheckBox->OnCheckStateChanged.RemoveDynamic(this, &USRFacilityControlWidget::HandleDeliverCheckStateChanged);
		DeliverCheckBox->OnCheckStateChanged.AddDynamic(this, &USRFacilityControlWidget::HandleDeliverCheckStateChanged);
	}
	if (DebugAddTerriteButton)
	{
		DebugAddTerriteButton->OnClicked.RemoveDynamic(this, &USRFacilityControlWidget::HandleDebugAddTerriteClicked);
		DebugAddTerriteButton->OnClicked.AddDynamic(this, &USRFacilityControlWidget::HandleDebugAddTerriteClicked);
	}
	if (DebugAddAquidButton)
	{
		DebugAddAquidButton->OnClicked.RemoveDynamic(this, &USRFacilityControlWidget::HandleDebugAddAquidClicked);
		DebugAddAquidButton->OnClicked.AddDynamic(this, &USRFacilityControlWidget::HandleDebugAddAquidClicked);
	}
	if (DebugAddNitainButton)
	{
		DebugAddNitainButton->OnClicked.RemoveDynamic(this, &USRFacilityControlWidget::HandleDebugAddNitainClicked);
		DebugAddNitainButton->OnClicked.AddDynamic(this, &USRFacilityControlWidget::HandleDebugAddNitainClicked);
	}
}

void USRFacilityControlWidget::RefreshInputResourceSlots(USRFacilityNetworkComponent* FacilityNetwork, const FSRFacilityInstance& FacilityInstance)
{
	if (!InputResourceSlotBox)
	{
		return;
	}

	TArray<FSRResourceInstance> PreviewResources;
	FString EmptyText;
	int32 PreviewResourceCount = 0;
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	const bool bIsMiningFacility = IsValid(FacilityDataAsset) && FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine;
	if (bIsMiningFacility)
	{
		FSRResourceDepositInstance MiningTarget;
		if (IsValid(FacilityNetwork)
			&& FacilityNetwork->GetFacilityMiningTarget(FacilityInstance.OccupantId, MiningTarget)
			&& IsValid(MiningTarget.ResourceDataAsset.Get()))
		{
			PreviewResources.Add(MiningTarget.ResourceDataAsset->BuildDefaultInstance());
		}
		PreviewResourceCount = PreviewResources.Num();
		EmptyText = BuildMiningTargetSummary(FacilityNetwork, FacilityInstance.OccupantId);
	}
	else
	{
		BuildNextInputPreviewResources(FacilityInstance, PreviewResources);
		PreviewResourceCount = PreviewResources.Num();
		EmptyText = TEXT("No queued resource");
	}

	if (InputResourceTextBlock)
	{
		InputResourceTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Input Resource Preview (%d)"),
			PreviewResourceCount)));
	}

	FString NewSignature = FString::Printf(TEXT("InputPreview:%d:%s"), PreviewResources.Num(), *EmptyText);
	for (int32 PreviewIndex = 0; PreviewIndex < PreviewResources.Num(); ++PreviewIndex)
	{
		NewSignature += TEXT("|");
		NewSignature += BuildResourceSlotText(TEXT("Input"), PreviewIndex, NAME_None, &PreviewResources[PreviewIndex], TEXT("No Input"));
	}
	if (InputResourcePanelSignature == NewSignature)
	{
		return;
	}

	InputResourcePanelSignature = NewSignature;
	InputResourceSlotBox->ClearChildren();
	InputSlotDebugActions.Reset();
	if (PreviewResources.IsEmpty())
	{
		AddInventoryInfoCard(
			WidgetTree,
			InputResourceSlotBox,
			EmptyText,
			FLinearColor(0.84f, 0.91f, 1.0f, 1.0f));
		return;
	}

	for (const FSRResourceInstance& PreviewResource : PreviewResources)
	{
		AddPreviewResourceCard(
			WidgetTree,
			InputResourceSlotBox,
			PreviewResource,
			FLinearColor(0.84f, 0.91f, 1.0f, 1.0f));
	}
}

void USRFacilityControlWidget::RefreshOutputResourceSlots(USRFacilityNetworkComponent* FacilityNetwork, const FSRFacilityInstance&)
{
	if (!OutputResourceSlotBox)
	{
		return;
	}

	TArray<FSRResourceInstance> PreviewOutputs;
	if (IsValid(FacilityNetwork))
	{
		FSRResourceInstance PrimaryOutput;
		TArray<FSRResourceInstance> AdditionalOutputs;
		int32 OutputCount = 0;
		if (FacilityNetwork->GetFacilityOutputPreview(FocusedOccupantId, PrimaryOutput, AdditionalOutputs, OutputCount))
		{
			const int32 PrimaryOutputCount = FMath::Max(0, OutputCount);
			PreviewOutputs.Reserve(PrimaryOutputCount + AdditionalOutputs.Num());
			for (int32 OutputIndex = 0; OutputIndex < PrimaryOutputCount; ++OutputIndex)
			{
				PreviewOutputs.Add(PrimaryOutput);
			}
			PreviewOutputs.Append(AdditionalOutputs);
		}
	}

	const int32 PreviewOutputCount = PreviewOutputs.Num();
	const FString EmptyText = TEXT("Process result unavailable");

	if (OutputPreviewTextBlock)
	{
		OutputPreviewTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Output Resource Preview (%d)"),
			PreviewOutputCount)));
	}

	FString NewSignature = FString::Printf(TEXT("OutputPreview:%d:%s"), PreviewOutputs.Num(), *EmptyText);
	for (int32 PreviewIndex = 0; PreviewIndex < PreviewOutputs.Num(); ++PreviewIndex)
	{
		NewSignature += TEXT("|");
		NewSignature += BuildResourceSlotText(TEXT("Output"), PreviewIndex, NAME_None, &PreviewOutputs[PreviewIndex], TEXT("No Preview"));
	}
	if (OutputResourcePanelSignature == NewSignature)
	{
		return;
	}

	OutputResourcePanelSignature = NewSignature;
	OutputResourceSlotBox->ClearChildren();
	if (PreviewOutputs.IsEmpty())
	{
		AddInventoryInfoCard(
			WidgetTree,
			OutputResourceSlotBox,
			EmptyText,
			FLinearColor(0.84f, 1.0f, 0.90f, 1.0f));
		return;
	}

	for (const FSRResourceInstance& PreviewOutput : PreviewOutputs)
	{
		AddPreviewResourceCard(
			WidgetTree,
			OutputResourceSlotBox,
			PreviewOutput,
			FLinearColor(0.84f, 1.0f, 0.90f, 1.0f));
	}
}

void USRFacilityControlWidget::RefreshInputInventorySlots(
	USRFacilityNetworkComponent* FacilityNetwork,
	const FSRFacilityInstance& FacilityInstance,
	bool bIsMiningFacility)
{
	if (!InputInventorySlotBox)
	{
		return;
	}

	const FString EmptyText = bIsMiningFacility
		? BuildMiningTargetSummary(FacilityNetwork, FacilityInstance.OccupantId)
		: TEXT("No input slots");
	const FString NewSignature = bIsMiningFacility
		? FString::Printf(TEXT("InputMining:%s"), *EmptyText)
		: BuildInventoryPanelSignature(TEXT("Input"), FacilityInstance.InputPortInventories, EmptyText);
	if (InputInventoryPanelSignature == NewSignature)
	{
		return;
	}

	InputInventoryPanelSignature = NewSignature;
	InputInventorySlotBox->ClearChildren();
	if (bIsMiningFacility)
	{
		AddInventoryInfoCard(WidgetTree, InputInventorySlotBox, EmptyText, FLinearColor(0.82f, 0.88f, 1.0f, 1.0f));
		return;
	}

	if (FacilityInstance.InputPortInventories.IsEmpty())
	{
		AddInventoryInfoCard(WidgetTree, InputInventorySlotBox, EmptyText, FLinearColor(0.82f, 0.88f, 1.0f, 1.0f));
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < FacilityInstance.InputPortInventories.Num(); ++SlotIndex)
	{
		AddInventorySlotCard(
			WidgetTree,
			InputInventorySlotBox,
			FacilityInstance.InputPortInventories[SlotIndex],
			SlotIndex,
			TEXT("Input"),
			FLinearColor(0.82f, 0.88f, 1.0f, 1.0f));
	}
}

void USRFacilityControlWidget::RefreshOutputInventorySlots(const FSRFacilityInstance& FacilityInstance)
{
	if (!OutputInventorySlotBox)
	{
		return;
	}

	const FString EmptyText = TEXT("No output slots");
	const FString NewSignature = BuildInventoryPanelSignature(TEXT("Output"), FacilityInstance.OutputPortInventories, EmptyText);
	if (OutputInventoryPanelSignature == NewSignature)
	{
		return;
	}

	OutputInventoryPanelSignature = NewSignature;
	OutputInventorySlotBox->ClearChildren();
	if (FacilityInstance.OutputPortInventories.IsEmpty())
	{
		AddInventoryInfoCard(WidgetTree, OutputInventorySlotBox, EmptyText, FLinearColor(0.82f, 1.0f, 0.88f, 1.0f));
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < FacilityInstance.OutputPortInventories.Num(); ++SlotIndex)
	{
		AddInventorySlotCard(
			WidgetTree,
			OutputInventorySlotBox,
			FacilityInstance.OutputPortInventories[SlotIndex],
			SlotIndex,
			TEXT("Output"),
			FLinearColor(0.82f, 1.0f, 0.88f, 1.0f));
	}
}

void USRFacilityControlWidget::RefreshHubRouteSection(USRFacilityNetworkComponent* FacilityNetwork, const FSRFacilityInstance& FacilityInstance)
{
	if (!HubRouteTextBlock || !HubDestinationButtonBox || !HubRouteStatusTextBlock)
	{
		return;
	}

	const bool bIsHubFacility = IsValid(FacilityNetwork) && FacilityNetwork->IsHubFacility(FacilityInstance.OccupantId);
	if (!bIsHubFacility)
	{
		HubRouteTextBlock->SetText(NSLOCTEXT("StarRoversFacilityControl", "HubRoutesUnavailableTitle", "Hub Routes"));
		HubRouteStatusTextBlock->SetText(NSLOCTEXT("StarRoversFacilityControl", "HubRoutesUnavailableStatus", "Only Hub facilities can launch spaceships."));
		if (HubRoutePanelSignature != TEXT("NotHub"))
		{
			HubRoutePanelSignature = TEXT("NotHub");
			ClearHubRouteButtonsAndActions(
				HubDestinationButtonBox,
				HubRouteDestinationActions,
				HubRouteLaunchActions,
				HubRouteRemovalActions,
				HubRouteDebugOrbitActions,
				HubRouteSettingActions);
		}
		return;
	}

	UWorld* World = GetWorld();
	USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = IsValid(World)
		? World->GetSubsystem<USRSpaceLogisticsSubsystem>()
		: nullptr;
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		HubRouteTextBlock->SetText(NSLOCTEXT("StarRoversFacilityControl", "HubRoutesNoSubsystemTitle", "Hub Routes"));
		HubRouteStatusTextBlock->SetText(NSLOCTEXT("StarRoversFacilityControl", "HubRoutesNoSubsystemStatus", "Logistics subsystem unavailable."));
		if (HubRoutePanelSignature != TEXT("NoSubsystem"))
		{
			HubRoutePanelSignature = TEXT("NoSubsystem");
			ClearHubRouteButtonsAndActions(
				HubDestinationButtonBox,
				HubRouteDestinationActions,
				HubRouteLaunchActions,
				HubRouteRemovalActions,
				HubRouteDebugOrbitActions,
				HubRouteSettingActions);
		}
		return;
	}

	FSRSpaceLogisticsHubEndpoint SourceHub;
	if (!SpaceLogisticsSubsystem->GetHubEndpoint(FocusedActor.Get(), FocusedOccupantId, SourceHub))
	{
		HubRouteTextBlock->SetText(NSLOCTEXT("StarRoversFacilityControl", "HubRoutesNoEndpointTitle", "Hub Routes"));
		HubRouteStatusTextBlock->SetText(NSLOCTEXT("StarRoversFacilityControl", "HubRoutesNoEndpointStatus", "Source Hub endpoint not found."));
		if (HubRoutePanelSignature != TEXT("NoSourceEndpoint"))
		{
			HubRoutePanelSignature = TEXT("NoSourceEndpoint");
			ClearHubRouteButtonsAndActions(
				HubDestinationButtonBox,
				HubRouteDestinationActions,
				HubRouteLaunchActions,
				HubRouteRemovalActions,
				HubRouteDebugOrbitActions,
				HubRouteSettingActions);
		}
		return;
	}

	TArray<FSRSpaceLogisticsHubEndpoint> HubEndpoints;
	SpaceLogisticsSubsystem->GetHubEndpoints(HubEndpoints);

	TArray<FSRSpaceLogisticsHubRoute> HubRoutes;
	SpaceLogisticsSubsystem->GetHubRoutes(HubRoutes);

	TArray<FName> AvailableCargoResourceIds;
	if (IsValid(FacilityNetwork))
	{
		FacilityNetwork->GetHubOutboundCargoResourceIds(FocusedOccupantId, AvailableCargoResourceIds);
	}

	int32 DestinationCount = 0;
	int32 ConnectedRouteCount = 0;
	for (const FSRSpaceLogisticsHubEndpoint& HubEndpoint : HubEndpoints)
	{
		if (!HubEndpoint.IsValid() || AreHubEndpointKeysEqual(HubEndpoint, SourceHub))
		{
			continue;
		}
		++DestinationCount;
	}

	for (const FSRSpaceLogisticsHubRoute& HubRoute : HubRoutes)
	{
		if (HubRoute.bDebugLocalOrbit)
		{
			continue;
		}

		if (AreHubEndpointKeysEqual(HubRoute.SourceHub, SourceHub) || AreHubEndpointKeysEqual(HubRoute.DestinationHub, SourceHub))
		{
			++ConnectedRouteCount;
		}
	}

	if (bHasSelectedHubRouteDestination)
	{
		bool bFoundSelectedDestination = false;
		for (const FSRSpaceLogisticsHubEndpoint& HubEndpoint : HubEndpoints)
		{
			if (!HubEndpoint.IsValid() || AreHubEndpointKeysEqual(HubEndpoint, SourceHub))
			{
				continue;
			}

			if (AreHubEndpointKeysEqual(HubEndpoint, SelectedHubRouteDestination))
			{
				SelectedHubRouteDestination = HubEndpoint;
				bFoundSelectedDestination = true;
				break;
			}
		}

		if (!bFoundSelectedDestination)
		{
			SelectedHubRouteDestination = FSRSpaceLogisticsHubEndpoint();
			bHasSelectedHubRouteDestination = false;
			LastHubRouteStatus.Reset();
		}
	}

	HubRouteTextBlock->SetText(FText::FromString(FString::Printf(TEXT("Hub Routes (%d active)"), ConnectedRouteCount)));
	const FString StatusText = !LastHubRouteStatus.IsEmpty()
		? LastHubRouteStatus
		: (bHasSelectedHubRouteDestination
			? FString::Printf(TEXT("Destination selected: %s. Press Launch Route."), *BuildCelestialBodyDisplayName(SelectedHubRouteDestination.BodyActor.Get()))
			: FString::Printf(
				TEXT("%s"),
				DestinationCount > 0 ? TEXT("Select destination Hub.") : TEXT("No destination Hub available.")));
	HubRouteStatusTextBlock->SetText(FText::FromString(StatusText));

	FString NewSignature = FString::Printf(
		TEXT("Hub:%s:%s:%d:%d:%s:Selected:%s:%s"),
		*GetNameSafe(SourceHub.BodyActor.Get()),
		*SourceHub.HubOccupantId.ToString(),
		DestinationCount,
		ConnectedRouteCount,
		*StatusText,
		bHasSelectedHubRouteDestination ? *GetNameSafe(SelectedHubRouteDestination.BodyActor.Get()) : TEXT("None"),
		bHasSelectedHubRouteDestination ? *SelectedHubRouteDestination.HubOccupantId.ToString() : TEXT("None"));
	for (const FSRSpaceLogisticsHubEndpoint& HubEndpoint : HubEndpoints)
	{
		if (!HubEndpoint.IsValid() || AreHubEndpointKeysEqual(HubEndpoint, SourceHub))
		{
			continue;
		}

		NewSignature += FString::Printf(
			TEXT("|Endpoint:%s:%s"),
			*GetNameSafe(HubEndpoint.BodyActor.Get()),
			*HubEndpoint.HubOccupantId.ToString());
	}
	for (const FName AvailableCargoResourceId : AvailableCargoResourceIds)
	{
		NewSignature += FString::Printf(TEXT("|CargoOption:%s"), *AvailableCargoResourceId.ToString());
	}
	for (const FSRSpaceLogisticsHubRoute& HubRoute : HubRoutes)
	{
		if (HubRoute.bDebugLocalOrbit)
		{
			continue;
		}

		if (!AreHubEndpointKeysEqual(HubRoute.SourceHub, SourceHub) && !AreHubEndpointKeysEqual(HubRoute.DestinationHub, SourceHub))
		{
			continue;
		}

		NewSignature += FString::Printf(
			TEXT("|Route:%s:%d:%d:%d:%d:%s"),
			*HubRoute.RouteId.ToString(),
			static_cast<int32>(HubRoute.Phase),
			HubRoute.bDebugLocalOrbit ? 1 : 0,
			HubRoute.MaxCargoStackCount,
			HubRoute.bReturnEmptyWhenNoCargo ? 1 : 0,
			*HubRoute.CargoResourceId.ToString());
	}

	if (HubRoutePanelSignature == NewSignature)
	{
		return;
	}

	HubRoutePanelSignature = NewSignature;
	ClearHubRouteButtonsAndActions(
		HubDestinationButtonBox,
		HubRouteDestinationActions,
		HubRouteLaunchActions,
		HubRouteRemovalActions,
		HubRouteDebugOrbitActions,
		HubRouteSettingActions);
	HubRouteDestinationActions.Reserve(DestinationCount);
	HubRouteLaunchActions.Reserve(bHasSelectedHubRouteDestination ? 1 : 0);
	HubRouteRemovalActions.Reserve(ConnectedRouteCount);
	HubRouteSettingActions.Reserve(ConnectedRouteCount * (AvailableCargoResourceIds.Num() + 3));

	for (const FSRSpaceLogisticsHubEndpoint& HubEndpoint : HubEndpoints)
	{
		if (!HubEndpoint.IsValid() || AreHubEndpointKeysEqual(HubEndpoint, SourceHub))
		{
			continue;
		}

		const FSRSpaceLogisticsHubRoute* ExistingRoute = nullptr;
		for (const FSRSpaceLogisticsHubRoute& HubRoute : HubRoutes)
		{
			if (HubRoute.bDebugLocalOrbit)
			{
				continue;
			}

			if (DoesHubRouteConnectEndpoints(HubRoute, SourceHub, HubEndpoint))
			{
				ExistingRoute = &HubRoute;
				break;
			}
		}

		const bool bSelectedDestination = bHasSelectedHubRouteDestination
			&& AreHubEndpointKeysEqual(HubEndpoint, SelectedHubRouteDestination);
		const FString ButtonLabel = BuildHubEndpointUILabel(HubEndpoint, World);
		AddHubDestinationButton(
			WidgetTree,
			HubDestinationButtonBox,
			this,
			HubRouteDestinationActions,
			HubEndpoint,
			ButtonLabel,
			bSelectedDestination,
			ExistingRoute == nullptr);
		if (bSelectedDestination && !ExistingRoute)
		{
			AddHubRouteLaunchButton(
				WidgetTree,
				HubDestinationButtonBox,
				this,
				HubRouteLaunchActions,
				HubEndpoint,
				true);
		}
		if (ExistingRoute)
		{
			const int32 NextMaxCargoStackCount = ExistingRoute->MaxCargoStackCount <= 1 ? 5 : 1;
			AddHubRouteMaxCargoStackCountButton(
				WidgetTree,
				HubDestinationButtonBox,
				this,
				HubRouteSettingActions,
				ExistingRoute->RouteId,
				NextMaxCargoStackCount);
			AddHubRouteReturnEmptyButton(
				WidgetTree,
				HubDestinationButtonBox,
				this,
				HubRouteSettingActions,
				ExistingRoute->RouteId,
				!ExistingRoute->bReturnEmptyWhenNoCargo);
			if (!ExistingRoute->CargoResourceId.IsNone())
			{
				AddHubRouteCargoResourceButton(
					WidgetTree,
					HubDestinationButtonBox,
					this,
					HubRouteSettingActions,
					ExistingRoute->RouteId,
					NAME_None);
			}
			for (const FName AvailableCargoResourceId : AvailableCargoResourceIds)
			{
				if (AvailableCargoResourceId.IsNone() || AvailableCargoResourceId == ExistingRoute->CargoResourceId)
				{
					continue;
				}

				AddHubRouteCargoResourceButton(
					WidgetTree,
					HubDestinationButtonBox,
					this,
					HubRouteSettingActions,
					ExistingRoute->RouteId,
					AvailableCargoResourceId);
			}
			AddHubRouteRemoveButton(
				WidgetTree,
				HubDestinationButtonBox,
				this,
				HubRouteRemovalActions,
				ExistingRoute->RouteId);
		}
	}
}

void USRFacilityControlWidget::RefreshControlText()
{
	FSRFacilityInstance FacilityInstance;
	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	const bool bHasFacility = bHasFocusedFacility
		&& IsValid(FacilityNetwork)
		&& FacilityNetwork->GetFacilityInstance(FocusedOccupantId, FacilityInstance);

	if (!bHasFacility)
	{
		if (TitleTextBlock)
		{
			TitleTextBlock->SetText(NSLOCTEXT("StarRoversFacilityControl", "NoFacility", "No facility selected"));
		}
		if (InputResourceSlotBox)
		{
			InputResourceSlotBox->ClearChildren();
			InputSlotDebugActions.Reset();
			InputResourcePanelSignature.Reset();
		}
		if (OutputResourceSlotBox)
		{
			OutputResourceSlotBox->ClearChildren();
			OutputResourcePanelSignature.Reset();
		}
		if (InputInventorySlotBox)
		{
			InputInventorySlotBox->ClearChildren();
			InputInventoryPanelSignature.Reset();
		}
		if (OutputInventorySlotBox)
		{
			OutputInventorySlotBox->ClearChildren();
			OutputInventoryPanelSignature.Reset();
		}
		if (HubDestinationButtonBox)
		{
			ClearHubRouteButtonsAndActions(
				HubDestinationButtonBox,
				HubRouteDestinationActions,
				HubRouteLaunchActions,
				HubRouteRemovalActions,
				HubRouteDebugOrbitActions,
				HubRouteSettingActions);
			HubRoutePanelSignature.Reset();
		}
		if (HubRouteTextBlock)
		{
			HubRouteTextBlock->SetText(NSLOCTEXT("StarRoversFacilityControl", "HubRoutesNoFacility", "Hub Routes"));
		}
		if (HubRouteStatusTextBlock)
		{
			HubRouteStatusTextBlock->SetText(FText::GetEmpty());
		}
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	const FString FacilityName = IsValid(FacilityDataAsset) && !FacilityDataAsset->DisplayName.IsEmpty()
		? FacilityDataAsset->DisplayName.ToString()
		: FocusedOccupantId.ToString();
	const float ProcessSeconds = ResolveProcessSeconds(FacilityInstance);
	const float ProgressRatio = ProcessSeconds > 0.0f
		? FMath::Clamp(FacilityInstance.ProcessProgressSeconds / ProcessSeconds, 0.0f, 1.0f)
		: 0.0f;

	FString ProcessReason;
	bool bCanToggleProcess = CanToggleProcess(FacilityInstance, ProcessReason);
	const bool bHasOutputConveyor = FacilityNetwork->HasConnectedConveyorForFacilityPort(FocusedOccupantId, ESRFacilityPortKind::Output);
	const bool bCanDebugAddInput = HasAvailableInputPortCapacity(FacilityInstance);
	const bool bIsMiningFacility = IsValid(FacilityDataAsset) && FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine;
	FSRResourceDepositInstance MiningTarget;
	const bool bHasMiningTarget = bIsMiningFacility && FacilityNetwork->GetFacilityMiningTarget(FocusedOccupantId, MiningTarget);
	if (bIsMiningFacility && !bHasMiningTarget)
	{
		bCanToggleProcess = false;
		ProcessReason = TEXT("No adjacent deposit");
	}

	bUpdatingControls = true;
	if (ProcessCheckBox)
	{
		ProcessCheckBox->SetIsChecked(FacilityInstance.bProcessEnabled);
		ProcessCheckBox->SetIsEnabled(bCanToggleProcess);
	}
	if (DeliverCheckBox)
	{
		DeliverCheckBox->SetIsChecked(FacilityInstance.bDeliverEnabled);
		DeliverCheckBox->SetIsEnabled(bHasOutputConveyor);
	}
	if (DebugAddTerriteButton)
	{
		DebugAddTerriteButton->SetIsEnabled(bCanDebugAddInput);
	}
	if (DebugAddAquidButton)
	{
		DebugAddAquidButton->SetIsEnabled(bCanDebugAddInput);
	}
	if (DebugAddNitainButton)
	{
		DebugAddNitainButton->SetIsEnabled(bCanDebugAddInput);
	}
	bUpdatingControls = false;

	if (TitleTextBlock)
	{
		TitleTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("%s\nOccupant: %s  Temp: %s"),
			*FacilityName,
			*FocusedOccupantId.ToString(),
			GetFacilityTemperatureLabel(FacilityInstance.TemperatureState))));
	}
	if (ProcessStatusTextBlock)
	{
		ProcessStatusTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Process: %s  %s"),
			FacilityInstance.bProcessEnabled ? TEXT("ON") : TEXT("OFF"),
			*ProcessReason)));
	}
	RefreshInputResourceSlots(FacilityNetwork, FacilityInstance);
	if (EffectsTextBlock)
	{
		EffectsTextBlock->SetText(FText::FromString(BuildEffectsSummary(FacilityDataAsset)));
	}
	if (ProcessProgressBar)
	{
		ProcessProgressBar->SetPercent(ProgressRatio);
	}
	if (ProcessTimeTextBlock)
	{
		ProcessTimeTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Process Time: %.2f / %.2f sec"),
			FacilityInstance.ProcessProgressSeconds,
			ProcessSeconds)));
	}
	RefreshOutputResourceSlots(FacilityNetwork, FacilityInstance);
	if (InputInventoryTextBlock)
	{
		InputInventoryTextBlock->SetText(FText::FromString(
			bIsMiningFacility
				? TEXT("Input Inventory (Mining)")
				: FString::Printf(TEXT("Input Inventory (%d slots)"), FacilityInstance.InputPortInventories.Num())));
	}
	RefreshInputInventorySlots(FacilityNetwork, FacilityInstance, bIsMiningFacility);
	if (OutputInventoryTextBlock)
	{
		OutputInventoryTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Output Inventory (%d slots)"),
			FacilityInstance.OutputPortInventories.Num())));
	}
	RefreshOutputInventorySlots(FacilityInstance);
	if (DeliverStatusTextBlock)
	{
		DeliverStatusTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Deliver: %s  %s"),
			FacilityInstance.bDeliverEnabled ? TEXT("ON") : TEXT("OFF"),
			bHasOutputConveyor ? TEXT("Output conveyor connected") : TEXT("No output conveyor"))));
	}
	RefreshHubRouteSection(FacilityNetwork, FacilityInstance);
}
