#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Automation/SRFacilityNetworkComponent.h"
#include "Automation/SRResourceSystemContent.h"
#include "Automation/SRResourceSystemValidationActor.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Simulation/SRAugmentSubsystem.h"
#include "Simulation/SRResourceV2RunSaveSubsystem.h"
#include "Simulation/SRRunTelemetrySubsystem.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "UI/SRCelestialBodyFocusInfoWidget.h"
#include "UObject/UObjectIterator.h"

namespace StarRovers::ResourceSystemTests
{
	UWorld* FindPIEWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			if (WorldContext.WorldType == EWorldType::PIE && IsValid(WorldContext.World()))
			{
				return WorldContext.World();
			}
		}
		return nullptr;
	}

	USRCelestialBodyFocusInfoWidget* FindPIEFocusInfoWidget(UWorld* PIEWorld)
	{
		if (!IsValid(PIEWorld))
		{
			return nullptr;
		}

		for (TObjectIterator<USRCelestialBodyFocusInfoWidget> WidgetIt;
			WidgetIt; ++WidgetIt)
		{
			USRCelestialBodyFocusInfoWidget* Widget = *WidgetIt;
			if (IsValid(Widget)
				&& !Widget->HasAnyFlags(RF_ClassDefaultObject)
				&& Widget->GetWorld() == PIEWorld)
			{
				return Widget;
			}
		}
		return nullptr;
	}

	AActor* FindPIEOperationalBody(UWorld* PIEWorld)
	{
		if (!IsValid(PIEWorld))
		{
			return nullptr;
		}

		AActor* FallbackBody = nullptr;
		for (TActorIterator<AActor> ActorIt(PIEWorld); ActorIt; ++ActorIt)
		{
			AActor* BodyActor = *ActorIt;
			if (IsValid(BodyActor)
				&& USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(BodyActor)
				&& IsValid(BodyActor->FindComponentByClass<USRFacilityNetworkComponent>()))
			{
				FallbackBody = FallbackBody ? FallbackBody : BodyActor;
				if (const USRStructureInstanceManagerComponent* StructureManager =
					BodyActor->FindComponentByClass<USRStructureInstanceManagerComponent>())
				{
					TArray<FSRResourceDepositInstance> Deposits;
					StructureManager->GetResourceDepositInstances(Deposits);
					if (!Deposits.IsEmpty())
					{
						return BodyActor;
					}
				}
			}
		}
		return FallbackBody;
	}

	bool ValidatePIEBodyOperationsUI(
		UWorld* PIEWorld,
		FAutomationTestBase& Test)
	{
		USRCelestialBodyFocusInfoWidget* FocusWidget = FindPIEFocusInfoWidget(PIEWorld);
		AActor* OperationalBody = FindPIEOperationalBody(PIEWorld);
		if (!IsValid(FocusWidget) || !IsValid(OperationalBody))
		{
			return false;
		}

		FocusWidget->SetFocusInfo(
			USRCelestialBodyRuntimeLibrary::BuildCelestialBodyFocusInfo(OperationalBody));
		const FSRCelestialBodyOperationsSummary Summary =
			FocusWidget->GetBodyOperationsSummary();
		Test.TestTrue(TEXT("Focused planet operations snapshot is valid"), Summary.bIsValid);
		Test.TestTrue(TEXT("Focused planet exposes positive Operational Capacity"),
			Summary.OperationalCapacity.TotalCapacity > 0);

		USizeBox* OperationsContainer = Cast<USizeBox>(
			FocusWidget->GetWidgetFromName(FName(TEXT("BodyOperationsContainer"))));
		UProgressBar* LoadProgressBar = Cast<UProgressBar>(
			FocusWidget->GetWidgetFromName(FName(TEXT("OperationalLoadProgressBar"))));
		UTextBlock* ResourceReserveText = Cast<UTextBlock>(
			FocusWidget->GetWidgetFromName(FName(TEXT("ResourceReserveTextBlock"))));
		Test.TestNotNull(TEXT("Focus UI contains the body operations card"), OperationsContainer);
		Test.TestNotNull(TEXT("Focus UI contains the Operational Load bar"), LoadProgressBar);
		Test.TestNotNull(TEXT("Focus UI contains the resource reserve row"), ResourceReserveText);
		Test.TestTrue(TEXT("A generated resource body exposes local finite reserves"),
			Summary.ResourceReserve.bHasDeposits
				&& Summary.ResourceReserve.TotalFiniteAmount > 0);
		if (IsValid(ResourceReserveText))
		{
			Test.TestTrue(TEXT("The reserve row is immediately readable"),
				ResourceReserveText->GetText().ToString().Contains(TEXT("RESERVES")));
		}
		if (IsValid(OperationsContainer))
		{
			Test.TestEqual(TEXT("Operations card is visible for a planet or moon"),
				OperationsContainer->GetVisibility(), ESlateVisibility::Visible);
		}
		return true;
	}

	bool ValidatePIEFiniteResourceReserveContract(
		UWorld* PIEWorld,
		FAutomationTestBase& Test)
	{
		if (!IsValid(PIEWorld))
		{
			return false;
		}
		TArray<FSRResourceDepositInstance> SystemDeposits;
		for (TActorIterator<AActor> ActorIt(PIEWorld); ActorIt; ++ActorIt)
		{
			AActor* BodyActor = *ActorIt;
			if (!IsValid(BodyActor)
				|| !USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(BodyActor))
			{
				continue;
			}
			const USRStructureInstanceManagerComponent* StructureManager =
				BodyActor->FindComponentByClass<USRStructureInstanceManagerComponent>();
			if (!IsValid(StructureManager))
			{
				continue;
			}
			TArray<FSRResourceDepositInstance> BodyDeposits;
			StructureManager->GetResourceDepositInstances(BodyDeposits);
			for (const FSRResourceDepositInstance& Deposit : BodyDeposits)
			{
				const USRResourceDataAsset* Resource = Deposit.ResourceDataAsset.Get();
				int32 ExpectedAmount = 0;
				if (!IsValid(Resource)
					|| !FSRResourceSystemContent::TryGetDepositTotalAmount(
						Resource->ResourceV2Preset,
						ExpectedAmount)
					|| Deposit.TotalAmount != ExpectedAmount
					|| Deposit.RemainingAmount != ExpectedAmount)
				{
					return false;
				}
			}
			SystemDeposits.Append(MoveTemp(BodyDeposits));
		}
		const FSRResourceReserveSnapshot Reserve =
			FSRResourceReserveModel::BuildSnapshot(SystemDeposits);
		if (!Reserve.bHasDeposits
			|| Reserve.CoveredReferenceCardTypeCount != 5
			|| Reserve.PotentialFuelBatchCount <= 0)
		{
			return false;
		}
		Test.TestTrue(TEXT("PIE spawned only Catalog-balanced finite Resource V2 deposits"),
			Reserve.InfiniteDepositCount == 0
				&& Reserve.DepletedDepositCount == 0
				&& FMath::IsNearlyEqual(Reserve.RemainingRatio, 1.0f));
		Test.TestTrue(TEXT("The generated system can form complete five-Card batches"),
			Reserve.CoveredReferenceCardTypeCount == 5
				&& Reserve.PotentialFuelBatchCount > 0
				&& Reserve.LimitingReferenceCardId != NAME_None);
		return true;
	}

	template <typename T>
	bool MatchesUniqueValues(const TArray<T>& Actual, const TSet<T>& Expected)
	{
		if (Actual.Num() != Expected.Num())
		{
			return false;
		}
		for (const T& Value : Actual)
		{
			if (!Expected.Contains(Value))
			{
				return false;
			}
		}
		return true;
	}

	bool ValidatePIEAugmentResourceContext(
		UWorld* PIEWorld,
		FAutomationTestBase& Test)
	{
		if (!IsValid(PIEWorld))
		{
			return false;
		}
		USRAugmentSubsystem* AugmentSubsystem =
			PIEWorld->GetSubsystem<USRAugmentSubsystem>();
		if (!IsValid(AugmentSubsystem))
		{
			return false;
		}

		TSet<ESRResourceFamily> ExpectedFamilies;
		TSet<ESRResourceSpectrum> ExpectedSpectra;
		TSet<int32> ExpectedGrades;
		for (TActorIterator<AActor> ActorIt(PIEWorld); ActorIt; ++ActorIt)
		{
			AActor* BodyActor = *ActorIt;
			if (!IsValid(BodyActor)
				|| !USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(BodyActor)
				|| !USRCelestialBodyRuntimeLibrary::GetCelestialCanConstruct(BodyActor))
			{
				continue;
			}
			const USRStructureInstanceManagerComponent* StructureManager =
				BodyActor->FindComponentByClass<USRStructureInstanceManagerComponent>();
			if (!IsValid(StructureManager))
			{
				continue;
			}
			TArray<FSRPlacedStructureInstance> PlacedStructures;
			StructureManager->GetPlacedStructures(PlacedStructures);
			for (const FSRPlacedStructureInstance& PlacedStructure : PlacedStructures)
			{
				FSRResourceDepositInstance Deposit;
				if (!StructureManager->GetResourceDepositInstance(
						PlacedStructure.OccupantId,
						Deposit)
					|| !FSRResourceDepositAmountModel::CanHarvest(Deposit.RemainingAmount))
				{
					continue;
				}
				const USRResourceDataAsset* Resource = Deposit.ResourceDataAsset.Get();
				if (!IsValid(Resource)
					|| Resource->ResourceDefinitionVersion
						< StarRovers::Resources::CurrentResourceDefinitionVersion
					|| Resource->ResourceClass != ESRResourceClass::Card)
				{
					continue;
				}
				if (Resource->Family != ESRResourceFamily::None)
				{
					ExpectedFamilies.Add(Resource->Family);
				}
				if (Resource->NativeSpectrum != ESRResourceSpectrum::None)
				{
					ExpectedSpectra.Add(Resource->NativeSpectrum);
				}
				if (Resource->NativeGrade >= StarRovers::Resources::MinimumGrade
					&& Resource->NativeGrade <= StarRovers::Resources::MaximumGrade)
				{
					ExpectedGrades.Add(Resource->NativeGrade);
				}
			}
		}
		if (ExpectedFamilies.IsEmpty()
			|| ExpectedSpectra.IsEmpty()
			|| ExpectedGrades.IsEmpty())
		{
			return false;
		}

		const FSRAugmentBuildContextV2 Context =
			AugmentSubsystem->BuildResourceV2OfferContext();
		const bool bFamiliesMatch = MatchesUniqueValues(
			Context.AccessibleFamilies,
			ExpectedFamilies);
		const bool bSpectraMatch = MatchesUniqueValues(
			Context.AccessibleSpectra,
			ExpectedSpectra);
		const bool bGradesMatch = MatchesUniqueValues(
			Context.AccessibleGrades,
			ExpectedGrades);
		if (!bFamiliesMatch || !bSpectraMatch || !bGradesMatch)
		{
			return false;
		}
		Test.TestTrue(TEXT("Augment Families come only from spawned harvestable deposits"), bFamiliesMatch);
		Test.TestTrue(TEXT("Augment Spectra come only from spawned harvestable deposits"), bSpectraMatch);
		Test.TestTrue(TEXT("Augment Grades come only from spawned harvestable deposits"), bGradesMatch);
		return true;
	}

	bool ValidatePIERunSaveRestore(
		UWorld* PIEWorld,
		FAutomationTestBase& Test)
	{
		USRResourceV2RunSaveSubsystem* RunSave = IsValid(PIEWorld)
			? PIEWorld->GetSubsystem<USRResourceV2RunSaveSubsystem>()
			: nullptr;
		USRTimeControlSubsystem* TimeControl = IsValid(PIEWorld)
			? PIEWorld->GetSubsystem<USRTimeControlSubsystem>()
			: nullptr;
		if (!IsValid(RunSave) || !IsValid(TimeControl))
		{
			return false;
		}

		FSRResourceV2RunSaveData Checkpoint;
		FString FailureReason;
		if (!RunSave->CaptureRunState(Checkpoint, FailureReason))
		{
			Test.AddError(FailureReason);
			return false;
		}

		USRStructureInstanceManagerComponent* TargetManager = nullptr;
		FSRResourceDepositInstance OriginalDeposit;
		for (TActorIterator<AActor> ActorIt(PIEWorld); ActorIt; ++ActorIt)
		{
			USRStructureInstanceManagerComponent* Candidate =
				ActorIt->FindComponentByClass<USRStructureInstanceManagerComponent>();
			TArray<FSRResourceDepositInstance> Deposits;
			if (IsValid(Candidate))
			{
				Candidate->GetResourceDepositInstances(Deposits);
			}
			if (!Deposits.IsEmpty()
				&& !FSRResourceDepositAmountModel::IsInfinite(Deposits[0].TotalAmount))
			{
				TargetManager = Candidate;
				OriginalDeposit = Deposits[0];
				break;
			}
		}
		if (!IsValid(TargetManager))
		{
			return false;
		}

		FSRResourceDepositInstance MutatedDeposit;
		const int32 MutatedRemaining = FMath::Max(0, OriginalDeposit.RemainingAmount - 17);
		if (!TargetManager->TryConfigureResourceDepositAmount(
				OriginalDeposit.OccupantId,
				OriginalDeposit.TotalAmount,
				MutatedRemaining,
				MutatedDeposit))
		{
			return false;
		}
		TimeControl->SetTimeScale(4.0f);

		FSRResourceV2RunRestoreReport RestoreReport;
		if (!RunSave->RestoreRunState(Checkpoint, RestoreReport))
		{
			Test.AddError(RestoreReport.FailureReason);
			return false;
		}

		FSRResourceDepositInstance RestoredDeposit;
		const bool bDepositRestored = TargetManager->GetResourceDepositInstance(
			OriginalDeposit.OccupantId,
			RestoredDeposit)
			&& RestoredDeposit.TotalAmount == OriginalDeposit.TotalAmount
			&& RestoredDeposit.RemainingAmount == OriginalDeposit.RemainingAmount;
		Test.TestTrue(TEXT("PIE Run Load restores an exact finite deposit amount"),
			bDepositRestored);
		Test.TestEqual(TEXT("PIE Run Load restores the selected simulation speed"),
			TimeControl->GetTimeScale(),
			Checkpoint.TimeControl.TimeScale);
		Test.TestEqual(TEXT("PIE Run Load restores the simulation pause"),
			TimeControl->IsSimulationPaused(),
			Checkpoint.TimeControl.bSimulationPaused);
		Test.TestTrue(TEXT("PIE Run Load rebuilds Structures and deposits"),
			RestoreReport.RestoredBodyCount == Checkpoint.Bodies.Num()
				&& RestoreReport.RestoredStructureCount > 0
				&& RestoreReport.RestoredDepositCount > 0);
		Test.TestTrue(TEXT("PIE Run Load rebuilds Telemetry from restored authority"),
			RestoreReport.bTelemetryRebuilt);

		FSRRunTelemetrySnapshot Telemetry;
		USRRunTelemetrySubsystem* TelemetrySubsystem =
			PIEWorld->GetSubsystem<USRRunTelemetrySubsystem>();
		Test.TestTrue(TEXT("Restored Telemetry contains the current finite reserve"),
			IsValid(TelemetrySubsystem)
				&& TelemetrySubsystem->GetLatestSnapshot(Telemetry)
				&& Telemetry.ResourceReserve.bHasDeposits
				&& Telemetry.ResourceReserve.RemainingFiniteAmount > 0);
		return bDepositRestored && RestoreReport.bTelemetryRebuilt;
	}

	class FWaitForResourceSystemBaselinePIECommand final : public IAutomationLatentCommand
	{
	public:
		explicit FWaitForResourceSystemBaselinePIECommand(FAutomationTestBase& InTest)
			: Test(InTest)
			, StartTimeSeconds(FPlatformTime::Seconds())
		{
		}

		virtual bool Update() override
		{
			UWorld* PIEWorld = FindPIEWorld();
			FSRResourceSystemValidationReport Report;
			if (IsValid(PIEWorld)
				&& ASRResourceSystemValidationActor::ValidateWorld(
					PIEWorld,
					true,
					true,
					Report)
				&& ValidatePIEAugmentResourceContext(PIEWorld, Test)
				&& ValidatePIEFiniteResourceReserveContract(PIEWorld, Test)
				&& ValidatePIEBodyOperationsUI(PIEWorld, Test)
				&& ValidatePIERunSaveRestore(PIEWorld, Test))
			{
				Test.AddInfo(Report.Summary);
				return true;
			}

			constexpr double TimeoutSeconds = 60.0;
			if (FPlatformTime::Seconds() - StartTimeSeconds < TimeoutSeconds)
			{
				return false;
			}

			if (!Report.Summary.IsEmpty())
			{
				const FString FailureDetails = Report.FailureMessages.IsEmpty()
					? FString()
					: FString::Printf(
						TEXT(" Failures: %s"),
						*FString::Join(Report.FailureMessages, TEXT(" | ")));
				Test.AddError(FString::Printf(
					TEXT("SolarSystem PIE baseline did not become valid within %.0f seconds. %s%s"),
					TimeoutSeconds,
					*Report.Summary,
					*FailureDetails));
			}
			else
			{
				Test.AddError(FString::Printf(
					TEXT("SolarSystem PIE world did not start within %.0f seconds."),
					TimeoutSeconds));
			}
			return true;
		}

	private:
		FAutomationTestBase& Test;
		double StartTimeSeconds = 0.0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceSystemSolarSystemPIEBaselineTest,
	"StarRovers.ResourceSystem.PIE.SolarSystemBaseline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceSystemSolarSystemPIEBaselineTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Levels/SolarSystem")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(
		StarRovers::ResourceSystemTests::FWaitForResourceSystemBaselinePIECommand(*this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
