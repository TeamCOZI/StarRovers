#include "Camera/SRPlayerController.h"

#include "Assembly/SRAssemblyComponent.h"
#include "Camera/SRCameraPawn.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Logistics/SRSpaceshipActor.h"
#include "Structure/SRStructureDataAsset.h"
#include "UI/SRCelestialBodyFocusInfoWidget.h"
#include "UI/SRCelestialBodyOverviewWidget.h"
#include "UI/SRFacilityControlWidget.h"
#include "UI/SRStructureSelectionWidget.h"
#include "UI/SRTimeControlWidget.h"

namespace
{
	constexpr int32 RuntimeAssemblyInputMappingPriority = 1;
	constexpr TCHAR RuntimeAreaCopyMirrorActionName[] = TEXT("IA_RuntimeAssemblyAreaCopyMirrorAction");
	constexpr TCHAR RuntimePickStructureActionName[] = TEXT("IA_RuntimeAssemblyPickStructureAction");
	constexpr TCHAR RuntimeRotatePlacementCounterClockwiseActionName[] = TEXT("IA_RuntimeRotatePlacementCounterClockwiseAction");
	constexpr TCHAR RuntimeRotatePlacementClockwiseActionName[] = TEXT("IA_RuntimeRotatePlacementClockwiseAction");
	constexpr TCHAR RuntimeStructureSelectionTabActionName[] = TEXT("IA_RuntimeStructureSelectionTabAction");
	constexpr TCHAR RuntimeAssemblyInputMappingContextName[] = TEXT("IMC_RuntimeAssemblyInput");
}

void ASRPlayerController::EnsureAssemblyAreaCopyMirrorInputAction()
{
	if (AssemblyAreaCopyMirrorAction)
	{
		return;
	}

	AssemblyAreaCopyMirrorAction = NewObject<UInputAction>(this, RuntimeAreaCopyMirrorActionName);
	if (AssemblyAreaCopyMirrorAction)
	{
		AssemblyAreaCopyMirrorAction->ValueType = EInputActionValueType::Boolean;
		AssemblyAreaCopyMirrorAction->ActionDescription = NSLOCTEXT("StarRovers", "AssemblyAreaCopyMirrorActionDescription", "Mirror area copy placement");
	}
}

void ASRPlayerController::EnsureAssemblyPickStructureInputAction()
{
	if (AssemblyPickStructureAction)
	{
		AssemblyPickStructureAction->bConsumeInput = false;
		return;
	}

	AssemblyPickStructureAction = NewObject<UInputAction>(this, RuntimePickStructureActionName);
	if (AssemblyPickStructureAction)
	{
		AssemblyPickStructureAction->ValueType = EInputActionValueType::Boolean;
		AssemblyPickStructureAction->bConsumeInput = false;
		AssemblyPickStructureAction->ActionDescription = NSLOCTEXT("StarRovers", "AssemblyPickStructureActionDescription", "Pick hovered structure for construction");
	}
}

void ASRPlayerController::EnsureRotatePlacementInputActions()
{
	if (!RotatePlacementCounterClockwiseAction)
	{
		RotatePlacementCounterClockwiseAction = NewObject<UInputAction>(this, RuntimeRotatePlacementCounterClockwiseActionName);
		if (RotatePlacementCounterClockwiseAction)
		{
			RotatePlacementCounterClockwiseAction->ValueType = EInputActionValueType::Boolean;
			RotatePlacementCounterClockwiseAction->ActionDescription = NSLOCTEXT("StarRovers", "RotatePlacementCounterClockwiseActionDescription", "Rotate placement counter-clockwise");
		}
	}

	if (!RotatePlacementClockwiseAction)
	{
		RotatePlacementClockwiseAction = NewObject<UInputAction>(this, RuntimeRotatePlacementClockwiseActionName);
		if (RotatePlacementClockwiseAction)
		{
			RotatePlacementClockwiseAction->ValueType = EInputActionValueType::Boolean;
			RotatePlacementClockwiseAction->ActionDescription = NSLOCTEXT("StarRovers", "RotatePlacementClockwiseActionDescription", "Rotate placement clockwise");
		}
	}
}

void ASRPlayerController::EnsureStructureSelectionTabInputAction()
{
	if (StructureSelectionTabAction)
	{
		return;
	}

	StructureSelectionTabAction = NewObject<UInputAction>(this, RuntimeStructureSelectionTabActionName);
	if (StructureSelectionTabAction)
	{
		StructureSelectionTabAction->ValueType = EInputActionValueType::Boolean;
		StructureSelectionTabAction->ActionDescription = NSLOCTEXT("StarRovers", "StructureSelectionTabActionDescription", "Switch structure selection tab");
	}
}

void ASRPlayerController::ApplyRuntimeAssemblyInputMapping()
{
	if (RuntimeState.bRuntimeAssemblyInputMappingApplied)
	{
		return;
	}

	EnsureAssemblyAreaCopyMirrorInputAction();
	EnsureAssemblyPickStructureInputAction();
	EnsureRotatePlacementInputActions();
	EnsureStructureSelectionTabInputAction();

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
	if (!InputSubsystem)
	{
		return;
	}

	if (!RuntimeAssemblyInputMappingContext)
	{
		RuntimeAssemblyInputMappingContext = NewObject<UInputMappingContext>(this, RuntimeAssemblyInputMappingContextName);
		if (AssemblyAreaCopyMirrorAction)
		{
			RuntimeAssemblyInputMappingContext->MapKey(AssemblyAreaCopyMirrorAction.Get(), EKeys::F);
		}
		if (AssemblyPickStructureAction)
		{
			RuntimeAssemblyInputMappingContext->MapKey(AssemblyPickStructureAction.Get(), EKeys::Z);
		}
		if (RotatePlacementCounterClockwiseAction)
		{
			RuntimeAssemblyInputMappingContext->MapKey(RotatePlacementCounterClockwiseAction.Get(), EKeys::Q);
		}
		if (RotatePlacementClockwiseAction)
		{
			RuntimeAssemblyInputMappingContext->MapKey(RotatePlacementClockwiseAction.Get(), EKeys::E);
		}
		if (StructureSelectionTabAction)
		{
			RuntimeAssemblyInputMappingContext->MapKey(StructureSelectionTabAction.Get(), EKeys::Tab);
		}
	}

	InputSubsystem->AddMappingContext(RuntimeAssemblyInputMappingContext, RuntimeAssemblyInputMappingPriority);
	RuntimeState.bRuntimeAssemblyInputMappingApplied = true;
}

void ASRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	EnsureAssemblyAreaCopyMirrorInputAction();
	EnsureAssemblyPickStructureInputAction();
	EnsureRotatePlacementInputActions();
	EnsureStructureSelectionTabInputAction();

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (LeftClickAction)
		{
			EnhancedInputComponent->BindAction(LeftClickAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleLeftClick);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ASRPlayerController requires LeftClickAction before input binding."));
		}

		if (FocusParentAction)
		{
			EnhancedInputComponent->BindAction(FocusParentAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleFocusParent);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ASRPlayerController requires FocusParentAction before input binding."));
		}

		if (AssemblyAreaDeletionDragHoldAction)
		{
			EnhancedInputComponent->BindAction(AssemblyAreaDeletionDragHoldAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleAssemblyAreaDeletionDragStarted);
			EnhancedInputComponent->BindAction(AssemblyAreaDeletionDragHoldAction, ETriggerEvent::Completed, this, &ASRPlayerController::HandleAssemblyAreaDeletionDragCompleted);
			EnhancedInputComponent->BindAction(AssemblyAreaDeletionDragHoldAction, ETriggerEvent::Canceled, this, &ASRPlayerController::HandleAssemblyAreaDeletionDragCompleted);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyAreaDeletionDragHoldAction before assembly area deletion drag binding."));
		}

		if (AssemblyAreaSelectionDeleteAction)
		{
			EnhancedInputComponent->BindAction(AssemblyAreaSelectionDeleteAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleAssemblyAreaSelectionDelete);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyAreaSelectionDeleteAction before assembly area selection delete binding."));
		}

		if (AssemblyAreaSelectionCopyAction)
		{
			EnhancedInputComponent->BindAction(AssemblyAreaSelectionCopyAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleAssemblyAreaSelectionCopy);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyAreaSelectionCopyAction before assembly area selection copy binding."));
		}

		if (AssemblyAreaCopyMirrorAction)
		{
			EnhancedInputComponent->BindAction(AssemblyAreaCopyMirrorAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleAssemblyAreaCopyMirror);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyAreaCopyMirrorAction before assembly area copy mirror binding."));
		}

		if (AssemblyPickStructureAction)
		{
			EnhancedInputComponent->BindAction(AssemblyPickStructureAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleAssemblyPickStructure);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyPickStructureAction before assembly pick structure binding."));
		}

		if (DeleteStructureAction)
		{
			EnhancedInputComponent->BindAction(DeleteStructureAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleRightClick);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires DeleteStructureAction before right-click structure deletion binding."));
		}

		if (RotatePlacementCounterClockwiseAction)
		{
			EnhancedInputComponent->BindAction(RotatePlacementCounterClockwiseAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleRotatePlacementCounterClockwise);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires RotatePlacementCounterClockwiseAction before placement rotation binding."));
		}

		if (RotatePlacementClockwiseAction)
		{
			EnhancedInputComponent->BindAction(RotatePlacementClockwiseAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleRotatePlacementClockwise);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires RotatePlacementClockwiseAction before placement rotation binding."));
		}

		if (ConveyorWaypointAction)
		{
			EnhancedInputComponent->BindAction(ConveyorWaypointAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleConveyorPlacementWaypoint);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires ConveyorWaypointAction before conveyor waypoint binding."));
		}

		if (BulkDeleteConveyorModifierAction)
		{
			EnhancedInputComponent->BindAction(BulkDeleteConveyorModifierAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleBulkDeleteConveyorModifierStarted);
			EnhancedInputComponent->BindAction(BulkDeleteConveyorModifierAction, ETriggerEvent::Completed, this, &ASRPlayerController::HandleBulkDeleteConveyorModifierEnded);
			EnhancedInputComponent->BindAction(BulkDeleteConveyorModifierAction, ETriggerEvent::Canceled, this, &ASRPlayerController::HandleBulkDeleteConveyorModifierEnded);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires BulkDeleteConveyorModifierAction before conveyor bulk deletion modifier binding."));
		}

		if (AssemblyShiftModifierAction)
		{
			EnhancedInputComponent->BindAction(AssemblyShiftModifierAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleAssemblyShiftModifierStarted);
			EnhancedInputComponent->BindAction(AssemblyShiftModifierAction, ETriggerEvent::Completed, this, &ASRPlayerController::HandleAssemblyShiftModifierEnded);
			EnhancedInputComponent->BindAction(AssemblyShiftModifierAction, ETriggerEvent::Canceled, this, &ASRPlayerController::HandleAssemblyShiftModifierEnded);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyShiftModifierAction before assembly shift modifier binding."));
		}

		if (AssemblyUndoRedoAction)
		{
			EnhancedInputComponent->BindAction(AssemblyUndoRedoAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleAssemblyUndoRedoAction);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyUndoRedoAction before assembly undo/redo binding."));
		}

		if (StructureSelectionTabAction)
		{
			EnhancedInputComponent->BindAction(StructureSelectionTabAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleStructureSelectionTab);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires StructureSelectionTabAction before structure selection tab binding."));
		}
	}

	ApplyRuntimeAssemblyInputMapping();
}

void ASRPlayerController::HandleStructureSelectionTab()
{
	if (!IsAssemblyModeActive() || !StructureSelectionWidget)
	{
		return;
	}

	StructureSelectionWidget->AdvanceStructureSelectionTab();
}

void ASRPlayerController::HandleLeftClick()
{
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	const bool bHasMousePosition = GetMousePosition(MouseX, MouseY);
	const bool bOverFacilityControl = IsValid(FacilityControlWidget) && FacilityControlWidget->IsPointerOverControlPanel();
	const bool bOverFocusInfo = IsValid(FocusInfoWidget) && FocusInfoWidget->IsPointerOverFocusInfoUi();
	const bool bOverOverview = IsValid(OverviewWidget) && OverviewWidget->IsPointerOverOverviewUi();
	const bool bOverTimeControl = IsValid(TimeControlWidget) && TimeControlWidget->IsPointerOverTimeControlPanel();
	const bool bOverStructureSelection = IsValid(StructureSelectionWidget) && StructureSelectionWidget->IsPointerOverStructureSelectionPanel();
	const bool bOverBlockingUi = bOverFacilityControl
		|| bOverFocusInfo
		|| bOverOverview
		|| bOverTimeControl
		|| bOverStructureSelection;
	ESRPlayerUiLayer TopBlockingUiLayer = ESRPlayerUiLayer::FocusInfo;
	const TCHAR* TopBlockingUiName = TEXT("None");
	int32 TopBlockingUiZOrder = MIN_int32;

	const auto ConsiderBlockingUiLayer =
		[this, &TopBlockingUiLayer, &TopBlockingUiName, &TopBlockingUiZOrder](bool bIsPointerOverLayer, ESRPlayerUiLayer Layer, const TCHAR* LayerName)
		{
			if (!bIsPointerOverLayer)
			{
				return;
			}

			const int32 LayerZOrder = ResolveWidgetLayerZOrder(Layer);
			if (LayerZOrder >= TopBlockingUiZOrder)
			{
				TopBlockingUiLayer = Layer;
				TopBlockingUiName = LayerName;
				TopBlockingUiZOrder = LayerZOrder;
			}
		};

	ConsiderBlockingUiLayer(bOverFacilityControl, ESRPlayerUiLayer::FacilityControl, TEXT("FacilityControl"));
	ConsiderBlockingUiLayer(bOverFocusInfo, ESRPlayerUiLayer::FocusInfo, TEXT("FocusInfo"));
	ConsiderBlockingUiLayer(bOverOverview, ESRPlayerUiLayer::Overview, TEXT("Overview"));
	ConsiderBlockingUiLayer(bOverTimeControl, ESRPlayerUiLayer::TimeControl, TEXT("TimeControl"));
	ConsiderBlockingUiLayer(bOverStructureSelection, ESRPlayerUiLayer::StructureSelection, TEXT("StructureSelection"));

	UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: PlayerController LeftClick Mouse=(%.1f, %.1f) HasMouse=%s FacilityControl=%s FocusInfo=%s Overview=%s TimeControl=%s StructureSelection=%s TopBlockingUi=%s TopZOrder=%d"),
		MouseX,
		MouseY,
		bHasMousePosition ? TEXT("true") : TEXT("false"),
		bOverFacilityControl ? TEXT("true") : TEXT("false"),
		bOverFocusInfo ? TEXT("true") : TEXT("false"),
		bOverOverview ? TEXT("true") : TEXT("false"),
		bOverTimeControl ? TEXT("true") : TEXT("false"),
		bOverStructureSelection ? TEXT("true") : TEXT("false"),
		TopBlockingUiName,
		TopBlockingUiZOrder);

	if (bOverBlockingUi)
	{
		if (TopBlockingUiLayer == ESRPlayerUiLayer::FacilityControl
			&& FacilityControlWidget
			&& FacilityControlWidget->TryHandleFacilityControlPointerClick())
		{
			UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: PlayerController LeftClick handled by top FacilityControl UI."));
			return;
		}

		if (TopBlockingUiLayer == ESRPlayerUiLayer::StructureSelection
			&& StructureSelectionWidget
			&& StructureSelectionWidget->TryHandleStructureSelectionPointerClick())
		{
			UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: PlayerController LeftClick handled by top StructureSelection UI."));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: PlayerController LeftClick blocked by top UI hit test: %s."), TopBlockingUiName);
		return;
	}

	if (ShouldHandleAssemblyAreaSelectionDrag()
		&& (!AssemblyComponent || !AssemblyComponent->IsAreaCopyPlacementActive()))
	{
		return;
	}

	RuntimeState.bPendingInitialPrimaryStarFocus = false;

	AActor* AssemblySelectedActor = nullptr;
	if (AssemblyComponent && AssemblyComponent->TryHandleAssemblyClick(AssemblySelectedActor))
	{
		if (IsValid(AssemblySelectedActor) && SelectedActor != AssemblySelectedActor)
		{
			UpdateSelection(AssemblySelectedActor);
		}
		return;
	}

	FHitResult CursorHitResult;
	const bool bHasCursorHit = GetHitResultUnderCursor(ECC_Visibility, false, CursorHitResult);

	AActor* HitActor = bHasCursorHit ? CursorHitResult.GetActor() : nullptr;
	AActor* SelectedFocusActor = nullptr;
	if (USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(HitActor) || Cast<ASRSpaceshipActor>(HitActor))
	{
		SelectedFocusActor = HitActor;
	}

	if (!SelectedFocusActor)
	{
		return;
	}

	RequestFocusActor(SelectedFocusActor);
}

void ASRPlayerController::HandleRightClick()
{
	if (IsPointerOverBlockingUi())
	{
		return;
	}

	if ((AssemblyComponent && AssemblyComponent->IsAreaSelectionDragActive())
		|| RuntimeState.bAssemblyAreaDeletionDragHoldActive
		|| (AssemblyComponent && AssemblyComponent->IsAreaDeletionDragActive()))
	{
		return;
	}

	if (AssemblyComponent && AssemblyComponent->IsAreaCopyPlacementActive())
	{
		AActor* AssemblySelectedActor = nullptr;
		if (AssemblyComponent->TryHandleAssemblyDelete(AssemblySelectedActor)
			&& IsValid(AssemblySelectedActor)
			&& SelectedActor != AssemblySelectedActor)
		{
			UpdateSelection(AssemblySelectedActor);
		}
		return;
	}

	if (ClearSelectedStructureBuildOption())
	{
		return;
	}

	AActor* AssemblySelectedActor = nullptr;
	if (AssemblyComponent && AssemblyComponent->TryHandleAssemblyDelete(AssemblySelectedActor))
	{
		if (IsValid(AssemblySelectedActor) && SelectedActor != AssemblySelectedActor)
		{
			UpdateSelection(AssemblySelectedActor);
		}
	}
}

void ASRPlayerController::HandleAssemblyAreaDeletionDragStarted()
{
	if (IsPointerOverBlockingUi())
	{
		return;
	}

	if (AssemblyComponent && AssemblyComponent->IsAreaSelectionDragActive())
	{
		return;
	}

	RuntimeState.bAssemblyAreaDeletionDragHoldActive = true;
}

void ASRPlayerController::HandleAssemblyAreaDeletionDragCompleted()
{
	RuntimeState.bAssemblyAreaDeletionDragHoldActive = false;
	EndAssemblyAreaDeletionDrag();
}

void ASRPlayerController::HandleAssemblyAreaSelectionDelete()
{
	if (IsPointerOverBlockingUi() || !IsAssemblyModeActive() || !AssemblyComponent)
	{
		return;
	}

	AssemblyComponent->TryDeleteAreaSelection();
}

void ASRPlayerController::HandleAssemblyAreaSelectionCopy()
{
	if (IsPointerOverBlockingUi() || !IsAssemblyModeActive() || !AssemblyComponent)
	{
		return;
	}

	const bool bControlDown = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	if (!bControlDown)
	{
		return;
	}

	AssemblyComponent->TryBeginAreaSelectionCopyPlacement();
}

void ASRPlayerController::HandleAssemblyAreaCopyMirror()
{
	if (IsPointerOverBlockingUi() || !IsAssemblyModeActive() || !AssemblyComponent)
	{
		return;
	}

	if (!AssemblyComponent->IsAreaCopyPlacementActive())
	{
		return;
	}

	AssemblyComponent->MirrorAreaCopyPlacement(IsAssemblyShiftModifierActive());
}

void ASRPlayerController::HandleAssemblyPickStructure()
{
	if (IsPointerOverBlockingUi() || !IsAssemblyModeActive())
	{
		return;
	}

	const bool bControlDown = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	if (bControlDown)
	{
		return;
	}

	TrySelectBuildOptionFromHoveredCell();
}

void ASRPlayerController::HandleAssemblyUndoRedoAction()
{
	if (!IsAssemblyModeActive() || !AssemblyComponent)
	{
		return;
	}

	const bool bControlDown = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	if (!bControlDown)
	{
		return;
	}

	if (IsAssemblyShiftModifierActive())
	{
		AssemblyComponent->TryRedoAssemblyPlacement();
		return;
	}

	AssemblyComponent->TryUndoAssemblyPlacement();
}

void ASRPlayerController::HandleFocusParent()
{
	if (IsAssemblyModeActive())
	{
		return;
	}

	RuntimeState.bPendingInitialPrimaryStarFocus = false;

	ASRCameraPawn* CameraPawn = Cast<ASRCameraPawn>(GetPawn());
	if (!CameraPawn)
	{
		return;
	}

	AActor* CurrentFocusActor = CameraPawn->GetFocusedActor();
	if (!IsValid(CurrentFocusActor))
	{
		return;
	}

	AActor* ParentBody = nullptr;
	if (!USRCelestialBodyRuntimeLibrary::GetCelestialParentBody(CurrentFocusActor, ParentBody) || !IsValid(ParentBody))
	{
		return;
	}

	if (AssemblyComponent)
	{
		AssemblyComponent->ClearSurfaceGridInteraction(CurrentFocusActor);
	}
	SetAssemblyModeActive(false);
	RequestFocusActor(ParentBody);
}

void ASRPlayerController::HandleConveyorPlacementWaypoint()
{
	if (IsPointerOverBlockingUi())
	{
		return;
	}

	const bool bControlDown = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	if (bControlDown)
	{
		if (AssemblyComponent)
		{
			AssemblyComponent->TryBeginAreaSelectionCopyPlacement();
		}
		return;
	}

	if (AssemblyComponent)
	{
		AssemblyComponent->TryAddConveyorPlacementDragWaypoint();
	}
}

void ASRPlayerController::HandleRotatePlacementCounterClockwise()
{
	TryHandlePlacementRotationInput(-1);
}

void ASRPlayerController::HandleRotatePlacementClockwise()
{
	TryHandlePlacementRotationInput(1);
}

void ASRPlayerController::HandleBulkDeleteConveyorModifierStarted()
{
	RuntimeState.bConveyorBulkDeleteModifierActive = true;
}

void ASRPlayerController::HandleBulkDeleteConveyorModifierEnded()
{
	RuntimeState.bConveyorBulkDeleteModifierActive = false;
}

bool ASRPlayerController::IsConveyorBulkDeleteModifierActive() const
{
	return RuntimeState.bConveyorBulkDeleteModifierActive;
}

void ASRPlayerController::HandleAssemblyShiftModifierStarted()
{
	RuntimeState.bAssemblyShiftModifierActive = true;
}

void ASRPlayerController::HandleAssemblyShiftModifierEnded()
{
	RuntimeState.bAssemblyShiftModifierActive = false;
}

bool ASRPlayerController::IsAssemblyShiftModifierActive() const
{
	return RuntimeState.bAssemblyShiftModifierActive;
}

bool ASRPlayerController::TryHandlePlacementRotationInput(int32 StepDelta)
{
	const uint64 CurrentFrame = GFrameCounter;
	if (RuntimeState.IsDuplicatePlacementRotationInput(CurrentFrame, StepDelta))
	{
		return true;
	}

	if (!IsAssemblyModeActive() || IsPointerOverBlockingUi())
	{
		return false;
	}

	const bool bHasSelectedStructureForPlacement = IsValid(GetSelectedStructureDataAsset());
	if (bHasSelectedStructureForPlacement)
	{
		if (!RotateStructurePlacement(StepDelta))
		{
			return false;
		}
	}
	else
	{
		ASRCameraPawn* CameraPawn = Cast<ASRCameraPawn>(GetPawn());
		if (!CameraPawn || !CameraPawn->RotateFocusSurfaceViewBySteps(StepDelta))
		{
			return false;
		}
	}

	RuntimeState.StorePlacementRotationInput(CurrentFrame, StepDelta);
	return true;
}
