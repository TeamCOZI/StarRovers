#pragma once

#include "CoreMinimal.h"

class UInputAction;

class FSRPlayerControllerDefaultInputActions
{
public:
	static void Load(
		TObjectPtr<UInputAction>& LeftClickAction,
		TObjectPtr<UInputAction>& FocusParentAction,
		TObjectPtr<UInputAction>& DeleteStructureAction,
		TObjectPtr<UInputAction>& AssemblyUndoRedoAction,
		TObjectPtr<UInputAction>& AssemblyAreaSelectionCopyAction,
		TObjectPtr<UInputAction>& AssemblyAreaCopyMirrorAction,
		TObjectPtr<UInputAction>& AssemblyPickStructureAction,
		TObjectPtr<UInputAction>& RotatePlacementCounterClockwiseAction,
		TObjectPtr<UInputAction>& RotatePlacementClockwiseAction,
		TObjectPtr<UInputAction>& RotateAssemblyPlacementAction);
};
