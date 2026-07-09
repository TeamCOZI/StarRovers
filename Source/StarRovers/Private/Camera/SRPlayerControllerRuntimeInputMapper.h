#pragma once

#include "CoreMinimal.h"
#include "Camera/SRPlayerControllerRuntimeState.h"

class APlayerController;
class UInputAction;
class UInputMappingContext;

class FSRPlayerControllerRuntimeInputMapper
{
public:
	static void EnsureAssemblyAreaCopyMirrorInputAction(
		UObject* Owner,
		TObjectPtr<UInputAction>& AssemblyAreaCopyMirrorAction);

	static void EnsureAssemblyPickStructureInputAction(
		UObject* Owner,
		TObjectPtr<UInputAction>& AssemblyPickStructureAction);

	static void EnsureRotatePlacementInputActions(
		UObject* Owner,
		TObjectPtr<UInputAction>& RotatePlacementCounterClockwiseAction,
		TObjectPtr<UInputAction>& RotatePlacementClockwiseAction);

	static void EnsureRotateAssemblyPlacementInputAction(
		UObject* Owner,
		TObjectPtr<UInputAction>& RotateAssemblyPlacementAction);

	static void EnsureStructureSelectionTabInputAction(
		UObject* Owner,
		TObjectPtr<UInputAction>& StructureSelectionTabAction);

	static void ApplyRuntimeAssemblyInputMapping(
		APlayerController* PlayerController,
		FSRPlayerControllerRuntimeState& RuntimeState,
		TObjectPtr<UInputMappingContext>& RuntimeAssemblyInputMappingContext,
		TObjectPtr<UInputAction>& AssemblyAreaCopyMirrorAction,
		TObjectPtr<UInputAction>& AssemblyPickStructureAction,
		TObjectPtr<UInputAction>& RotatePlacementCounterClockwiseAction,
		TObjectPtr<UInputAction>& RotatePlacementClockwiseAction,
		TObjectPtr<UInputAction>& RotateAssemblyPlacementAction,
		TObjectPtr<UInputAction>& StructureSelectionTabAction);
};
