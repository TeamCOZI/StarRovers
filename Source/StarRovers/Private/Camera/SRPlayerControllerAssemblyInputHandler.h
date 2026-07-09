#pragma once

#include "CoreMinimal.h"
#include "Camera/SRPlayerControllerRuntimeState.h"
#include "Templates/Function.h"

class USRAssemblyComponent;

class FSRPlayerControllerAssemblyInputHandler
{
public:
	static void HandleAreaDeletionDragStarted(
		FSRPlayerControllerRuntimeState& RuntimeState,
		bool bPointerOverBlockingUI,
		USRAssemblyComponent* AssemblyComponent);

	static void HandleAreaDeletionDragCompleted(
		FSRPlayerControllerRuntimeState& RuntimeState,
		USRAssemblyComponent* AssemblyComponent);

	static void HandleAreaSelectionDelete(
		bool bPointerOverBlockingUI,
		bool bAssemblyModeActive,
		USRAssemblyComponent* AssemblyComponent);

	static void HandleAreaSelectionCopy(
		bool bPointerOverBlockingUI,
		bool bAssemblyModeActive,
		bool bControlDown,
		USRAssemblyComponent* AssemblyComponent);

	static void HandleAreaCopyMirror(
		bool bPointerOverBlockingUI,
		bool bAssemblyModeActive,
		bool bAssemblyShiftModifierActive,
		USRAssemblyComponent* AssemblyComponent);

	static void HandlePickStructure(
		bool bPointerOverBlockingUI,
		bool bAssemblyModeActive,
		bool bControlDown,
		TFunctionRef<bool()> TrySelectBuildOptionFromHoveredCell);

	static void HandleUndoRedoAction(
		bool bAugmentChoiceVisible,
		bool bAssemblyModeActive,
		bool bControlDown,
		bool bAssemblyShiftModifierActive,
		USRAssemblyComponent* AssemblyComponent);

	static void HandleConveyorPlacementWaypoint(
		bool bPointerOverBlockingUI,
		bool bControlDown,
		USRAssemblyComponent* AssemblyComponent);
};
