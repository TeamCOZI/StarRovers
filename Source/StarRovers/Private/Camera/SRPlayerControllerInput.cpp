#include "Camera/SRPlayerController.h"

#include "Assembly/SRAssemblyComponent.h"
#include "Camera/SRCameraPawn.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "SRPlayerControllerAssemblyInputHandler.h"
#include "SRPlayerControllerAssemblyRightClickHandler.h"
#include "SRPlayerControllerFocusParentHandler.h"
#include "SRPlayerControllerInputBinder.h"
#include "SRPlayerControllerLeftClickHandler.h"
#include "SRPlayerControllerPointerUIRouter.h"
#include "SRPlayerControllerRotationInputHandler.h"
#include "SRPlayerControllerRuntimeInputMapper.h"
#include "Structure/SRStructureDataAsset.h"
#include "UI/SRAugmentChoiceWidget.h"
#include "UI/SRCelestialBodyFocusInfoWidget.h"
#include "UI/SRCelestialBodyOverviewWidget.h"
#include "UI/SRFacilityControlWidget.h"
#include "UI/SRStructureSelectionWidget.h"
#include "UI/SRTimeControlWidget.h"

void ASRPlayerController::EnsureAssemblyAreaCopyMirrorInputAction()
{
	FSRPlayerControllerRuntimeInputMapper::EnsureAssemblyAreaCopyMirrorInputAction(this, AssemblyAreaCopyMirrorAction);
}

void ASRPlayerController::EnsureAssemblyPickStructureInputAction()
{
	FSRPlayerControllerRuntimeInputMapper::EnsureAssemblyPickStructureInputAction(this, AssemblyPickStructureAction);
}

void ASRPlayerController::EnsureRotatePlacementInputActions()
{
	FSRPlayerControllerRuntimeInputMapper::EnsureRotatePlacementInputActions(
		this,
		RotatePlacementCounterClockwiseAction,
		RotatePlacementClockwiseAction);
}

void ASRPlayerController::EnsureRotateAssemblyPlacementInputAction()
{
	FSRPlayerControllerRuntimeInputMapper::EnsureRotateAssemblyPlacementInputAction(this, RotateAssemblyPlacementAction);
}

void ASRPlayerController::EnsureStructureSelectionTabInputAction()
{
	FSRPlayerControllerRuntimeInputMapper::EnsureStructureSelectionTabInputAction(this, StructureSelectionTabAction);
}

void ASRPlayerController::ApplyRuntimeAssemblyInputMapping()
{
	FSRPlayerControllerRuntimeInputMapper::ApplyRuntimeAssemblyInputMapping(
		this,
		RuntimeState,
		RuntimeAssemblyInputMappingContext,
		AssemblyAreaCopyMirrorAction,
		AssemblyPickStructureAction,
		RotatePlacementCounterClockwiseAction,
		RotatePlacementClockwiseAction,
		RotateAssemblyPlacementAction,
		StructureSelectionTabAction);
}

void ASRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	EnsureAssemblyAreaCopyMirrorInputAction();
	EnsureAssemblyPickStructureInputAction();
	EnsureRotatePlacementInputActions();
	EnsureRotateAssemblyPlacementInputAction();
	EnsureStructureSelectionTabInputAction();

	FSRPlayerControllerInputBinder::BindInputActions(*this, InputComponent);
	ApplyRuntimeAssemblyInputMapping();
}

void ASRPlayerController::HandleStructureSelectionTab()
{
	if ((IsValid(AugmentChoiceWidget) && AugmentChoiceWidget->IsVisible())
		|| !IsAssemblyModeActive()
		|| !StructureSelectionWidget)
	{
		return;
	}

	StructureSelectionWidget->AdvanceStructureSelectionTab();
}

void ASRPlayerController::HandleStructureSelectionCategory1()
{
	HandleStructureSelectionCategoryShortcut(0);
}

void ASRPlayerController::HandleStructureSelectionCategory2()
{
	HandleStructureSelectionCategoryShortcut(1);
}

void ASRPlayerController::HandleStructureSelectionCategory3()
{
	HandleStructureSelectionCategoryShortcut(2);
}

void ASRPlayerController::HandleStructureSelectionCategory4()
{
	HandleStructureSelectionCategoryShortcut(3);
}

void ASRPlayerController::HandleStructureSelectionCategoryShortcut(int32 CategoryIndex)
{
	if ((IsValid(AugmentChoiceWidget) && AugmentChoiceWidget->IsVisible())
		|| !IsAssemblyModeActive()
		|| !StructureSelectionWidget)
	{
		return;
	}

	StructureSelectionWidget->SelectStructureCategoryByShortcut(CategoryIndex);
}

void ASRPlayerController::HandleLeftClick()
{
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	const bool bHasMousePosition = GetMousePosition(MouseX, MouseY);
	if (FSRPlayerControllerPointerUIRouter::RouteLeftClick(
		WidgetLayerOrder,
		MouseX,
		MouseY,
		bHasMousePosition,
		FacilityControlWidget,
		FocusInfoWidget,
		OverviewWidget,
		TimeControlWidget,
		AugmentChoiceWidget,
		StructureSelectionWidget))
	{
		return;
	}

	FSRPlayerControllerLeftClickHandler::HandleWorldLeftClick(
		RuntimeState,
		ShouldHandleAssemblyAreaSelectionDrag(),
		AssemblyComponent,
		SelectedActor,
		[this]()
		{
			FHitResult CursorHitResult;
			const bool bHasCursorHit = GetHitResultUnderCursor(ECC_Visibility, false, CursorHitResult);
			return bHasCursorHit ? CursorHitResult.GetActor() : nullptr;
		},
		[this](AActor* NewSelectedActor)
		{
			UpdateSelection(NewSelectedActor);
		},
		[this](AActor* NewFocusedActor)
		{
			RequestFocusActor(NewFocusedActor);
		});
}

void ASRPlayerController::HandleRightClick()
{
	if (IsPointerOverBlockingUI())
	{
		return;
	}

	FSRPlayerControllerAssemblyRightClickHandler::HandleRightClick(
		AssemblyComponent,
		SelectedActor,
		RuntimeState.bAssemblyAreaDeletionDragHoldActive,
		[this]()
		{
			return ClearSelectedStructureBuildOption();
		},
		[this](AActor* NewSelectedActor)
		{
			UpdateSelection(NewSelectedActor);
		});
}

void ASRPlayerController::HandleAssemblyAreaDeletionDragStarted()
{
	FSRPlayerControllerAssemblyInputHandler::HandleAreaDeletionDragStarted(
		RuntimeState,
		IsPointerOverBlockingUI(),
		AssemblyComponent);
}

void ASRPlayerController::HandleAssemblyAreaDeletionDragCompleted()
{
	FSRPlayerControllerAssemblyInputHandler::HandleAreaDeletionDragCompleted(RuntimeState, AssemblyComponent);
}

void ASRPlayerController::HandleAssemblyAreaSelectionDelete()
{
	FSRPlayerControllerAssemblyInputHandler::HandleAreaSelectionDelete(
		IsPointerOverBlockingUI(),
		IsAssemblyModeActive(),
		AssemblyComponent);
}

void ASRPlayerController::HandleAssemblyAreaSelectionCopy()
{
	const bool bControlDown = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	FSRPlayerControllerAssemblyInputHandler::HandleAreaSelectionCopy(
		IsPointerOverBlockingUI(),
		IsAssemblyModeActive(),
		bControlDown,
		AssemblyComponent);
}

void ASRPlayerController::HandleAssemblyAreaCopyMirror()
{
	FSRPlayerControllerAssemblyInputHandler::HandleAreaCopyMirror(
		IsPointerOverBlockingUI(),
		IsAssemblyModeActive(),
		IsAssemblyShiftModifierActive(),
		AssemblyComponent);
}

void ASRPlayerController::HandleAssemblyPickStructure()
{
	const bool bControlDown = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	FSRPlayerControllerAssemblyInputHandler::HandlePickStructure(
		IsPointerOverBlockingUI(),
		IsAssemblyModeActive(),
		bControlDown,
		[this]()
		{
			return TrySelectBuildOptionFromHoveredCell();
		});
}

void ASRPlayerController::HandleAssemblyUndoRedoAction()
{
	const bool bControlDown = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	FSRPlayerControllerAssemblyInputHandler::HandleUndoRedoAction(
		IsValid(AugmentChoiceWidget) && AugmentChoiceWidget->IsVisible(),
		IsAssemblyModeActive(),
		bControlDown,
		IsAssemblyShiftModifierActive(),
		AssemblyComponent);
}

void ASRPlayerController::HandleFocusParent()
{
	AActor* ParentBody = FSRPlayerControllerFocusParentHandler::ResolveParentFocusActor(
		RuntimeState,
		IsValid(AugmentChoiceWidget) && AugmentChoiceWidget->IsVisible(),
		IsAssemblyModeActive(),
		Cast<ASRCameraPawn>(GetPawn()),
		AssemblyComponent);
	if (!IsValid(ParentBody))
	{
		return;
	}

	SetAssemblyModeActive(false);
	RequestFocusActor(ParentBody);
}

void ASRPlayerController::HandleConveyorPlacementWaypoint()
{
	const bool bControlDown = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	FSRPlayerControllerAssemblyInputHandler::HandleConveyorPlacementWaypoint(
		IsPointerOverBlockingUI(),
		bControlDown,
		AssemblyComponent);
}

void ASRPlayerController::HandleRotatePlacementCounterClockwise()
{
	TryHandleSurfaceViewRotationInput(-1);
}

void ASRPlayerController::HandleRotatePlacementClockwise()
{
	TryHandleSurfaceViewRotationInput(1);
}

void ASRPlayerController::HandleRotateAssemblyPlacement()
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
	return FSRPlayerControllerRotationInputHandler::TryHandlePlacementRotationInput(
		RuntimeState,
		IsAssemblyModeActive(),
		IsPointerOverBlockingUI(),
		AssemblyComponent,
		IsValid(GetSelectedStructureDataAsset()),
		StepDelta);
}

bool ASRPlayerController::TryHandleSurfaceViewRotationInput(int32 StepDelta)
{
	return FSRPlayerControllerRotationInputHandler::TryHandleSurfaceViewRotationInput(
		RuntimeState,
		IsAssemblyModeActive(),
		IsPointerOverBlockingUI(),
		Cast<ASRCameraPawn>(GetPawn()),
		StepDelta);
}
