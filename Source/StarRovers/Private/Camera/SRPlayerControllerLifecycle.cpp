#include "SRPlayerControllerLifecycle.h"

#include "Assembly/SRAssemblyComponent.h"
#include "Camera/SRPlayerController.h"

void FSRPlayerControllerLifecycle::BeginPlay(ASRPlayerController& PlayerController)
{
	if (PlayerController.AssemblyComponent)
	{
		PlayerController.AssemblyComponent->ConfigurePlacementPerformance(
			PlayerController.MaxStructurePlacementsPerFrame,
			PlayerController.MaxQueuedStructurePlacements);
	}
	PlayerController.ApplyRuntimeAssemblyInputMapping();

	ConfigureInputMode(PlayerController);

	PlayerController.UpdateHitResultTraceDistance();
	PlayerController.TryBindCameraPawnFocusEvents();
	PlayerController.TryBindCelestialBodyRegistryEvents();
	InitializeWidgets(PlayerController);
	PlayerController.TryAutoFocusPrimaryStar();
}

void FSRPlayerControllerLifecycle::Tick(ASRPlayerController& PlayerController)
{
	PlayerController.UpdateHitResultTraceDistance();
	PlayerController.TryBindCameraPawnFocusEvents();
	PlayerController.TryBindCelestialBodyRegistryEvents();
	if (!PlayerController.RuntimeState.bRuntimeAssemblyInputMappingApplied)
	{
		PlayerController.ApplyRuntimeAssemblyInputMapping();
	}
	PlayerController.UpdateAssemblyModeFromFocusedActorScreenSize();
	PlayerController.RefreshFocusedHubShortcutWidget(false);
}

void FSRPlayerControllerLifecycle::ConfigureInputMode(ASRPlayerController& PlayerController)
{
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController.SetInputMode(InputMode);
}

void FSRPlayerControllerLifecycle::InitializeWidgets(ASRPlayerController& PlayerController)
{
	PlayerController.CreateFocusInfoWidget();
	PlayerController.RefreshFocusInfoWidget();
	PlayerController.CreateOverviewWidget();
	PlayerController.RefreshOverviewWidget();
	PlayerController.CreateTimeControlWidget();
	PlayerController.CreateAugmentChoiceWidget();
	PlayerController.BindAugmentSubsystem();
	PlayerController.RegisterAvailableStructuresForAugments();
	PlayerController.CreateStructureSelectionWidget();
	PlayerController.RefreshStructureSelectionWidget();
	PlayerController.CreateFacilityControlWidget();
	PlayerController.RefreshFacilityControlWidget();
	PlayerController.CreateFocusedHubShortcutWidget();
	PlayerController.RefreshFocusedHubShortcutWidget(true);
	PlayerController.CreateStellarContractHUDWidget();
	PlayerController.CreateGameOverWidget();
}
