#include "SRPlayerControllerRuntimeInputMapper.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"

namespace
{
	constexpr int32 RuntimeAssemblyInputMappingPriority = 1;
	constexpr TCHAR RuntimeAreaCopyMirrorActionName[] = TEXT("IA_RuntimeAssemblyAreaCopyMirrorAction");
	constexpr TCHAR RuntimePickStructureActionName[] = TEXT("IA_RuntimeAssemblyPickStructureAction");
	constexpr TCHAR RuntimeRotatePlacementCounterClockwiseActionName[] = TEXT("IA_RuntimeRotatePlacementCounterClockwiseAction");
	constexpr TCHAR RuntimeRotatePlacementClockwiseActionName[] = TEXT("IA_RuntimeRotatePlacementClockwiseAction");
	constexpr TCHAR RuntimeRotateAssemblyPlacementActionName[] = TEXT("IA_RuntimeRotateAssemblyPlacementAction");
	constexpr TCHAR RuntimeStructureSelectionTabActionName[] = TEXT("IA_RuntimeStructureSelectionTabAction");
	constexpr TCHAR RuntimeAssemblyInputMappingContextName[] = TEXT("IMC_RuntimeAssemblyInput");
}

void FSRPlayerControllerRuntimeInputMapper::EnsureAssemblyAreaCopyMirrorInputAction(
	UObject* Owner,
	TObjectPtr<UInputAction>& AssemblyAreaCopyMirrorAction)
{
	if (AssemblyAreaCopyMirrorAction)
	{
		return;
	}

	AssemblyAreaCopyMirrorAction = NewObject<UInputAction>(Owner, RuntimeAreaCopyMirrorActionName);
	if (AssemblyAreaCopyMirrorAction)
	{
		AssemblyAreaCopyMirrorAction->ValueType = EInputActionValueType::Boolean;
		AssemblyAreaCopyMirrorAction->ActionDescription = NSLOCTEXT("StarRovers", "AssemblyAreaCopyMirrorActionDescription", "Mirror area copy placement");
	}
}

void FSRPlayerControllerRuntimeInputMapper::EnsureAssemblyPickStructureInputAction(
	UObject* Owner,
	TObjectPtr<UInputAction>& AssemblyPickStructureAction)
{
	if (AssemblyPickStructureAction)
	{
		AssemblyPickStructureAction->bConsumeInput = false;
		return;
	}

	AssemblyPickStructureAction = NewObject<UInputAction>(Owner, RuntimePickStructureActionName);
	if (AssemblyPickStructureAction)
	{
		AssemblyPickStructureAction->ValueType = EInputActionValueType::Boolean;
		AssemblyPickStructureAction->bConsumeInput = false;
		AssemblyPickStructureAction->ActionDescription = NSLOCTEXT("StarRovers", "AssemblyPickStructureActionDescription", "Pick hovered structure for construction");
	}
}

void FSRPlayerControllerRuntimeInputMapper::EnsureRotatePlacementInputActions(
	UObject* Owner,
	TObjectPtr<UInputAction>& RotatePlacementCounterClockwiseAction,
	TObjectPtr<UInputAction>& RotatePlacementClockwiseAction)
{
	if (!RotatePlacementCounterClockwiseAction)
	{
		RotatePlacementCounterClockwiseAction = NewObject<UInputAction>(Owner, RuntimeRotatePlacementCounterClockwiseActionName);
		if (RotatePlacementCounterClockwiseAction)
		{
			RotatePlacementCounterClockwiseAction->ValueType = EInputActionValueType::Boolean;
			RotatePlacementCounterClockwiseAction->ActionDescription = NSLOCTEXT("StarRovers", "RotateSurfaceViewCounterClockwiseActionDescription", "Rotate focused surface view counter-clockwise");
		}
	}

	if (!RotatePlacementClockwiseAction)
	{
		RotatePlacementClockwiseAction = NewObject<UInputAction>(Owner, RuntimeRotatePlacementClockwiseActionName);
		if (RotatePlacementClockwiseAction)
		{
			RotatePlacementClockwiseAction->ValueType = EInputActionValueType::Boolean;
			RotatePlacementClockwiseAction->ActionDescription = NSLOCTEXT("StarRovers", "RotateSurfaceViewClockwiseActionDescription", "Rotate focused surface view clockwise");
		}
	}
}

void FSRPlayerControllerRuntimeInputMapper::EnsureRotateAssemblyPlacementInputAction(
	UObject* Owner,
	TObjectPtr<UInputAction>& RotateAssemblyPlacementAction)
{
	if (RotateAssemblyPlacementAction)
	{
		return;
	}

	RotateAssemblyPlacementAction = NewObject<UInputAction>(Owner, RuntimeRotateAssemblyPlacementActionName);
	if (RotateAssemblyPlacementAction)
	{
		RotateAssemblyPlacementAction->ValueType = EInputActionValueType::Boolean;
		RotateAssemblyPlacementAction->ActionDescription = NSLOCTEXT("StarRovers", "RotateAssemblyPlacementActionDescription", "Rotate selected structure or area copy placement");
	}
}

void FSRPlayerControllerRuntimeInputMapper::EnsureStructureSelectionTabInputAction(
	UObject* Owner,
	TObjectPtr<UInputAction>& StructureSelectionTabAction)
{
	if (StructureSelectionTabAction)
	{
		return;
	}

	StructureSelectionTabAction = NewObject<UInputAction>(Owner, RuntimeStructureSelectionTabActionName);
	if (StructureSelectionTabAction)
	{
		StructureSelectionTabAction->ValueType = EInputActionValueType::Boolean;
		StructureSelectionTabAction->ActionDescription = NSLOCTEXT("StarRovers", "StructureSelectionTabActionDescription", "Switch structure selection tab");
	}
}

void FSRPlayerControllerRuntimeInputMapper::ApplyRuntimeAssemblyInputMapping(
	APlayerController* PlayerController,
	FSRPlayerControllerRuntimeState& RuntimeState,
	TObjectPtr<UInputMappingContext>& RuntimeAssemblyInputMappingContext,
	TObjectPtr<UInputAction>& AssemblyAreaCopyMirrorAction,
	TObjectPtr<UInputAction>& AssemblyPickStructureAction,
	TObjectPtr<UInputAction>& RotatePlacementCounterClockwiseAction,
	TObjectPtr<UInputAction>& RotatePlacementClockwiseAction,
	TObjectPtr<UInputAction>& RotateAssemblyPlacementAction,
	TObjectPtr<UInputAction>& StructureSelectionTabAction)
{
	if (RuntimeState.bRuntimeAssemblyInputMappingApplied)
	{
		return;
	}

	EnsureAssemblyAreaCopyMirrorInputAction(PlayerController, AssemblyAreaCopyMirrorAction);
	EnsureAssemblyPickStructureInputAction(PlayerController, AssemblyPickStructureAction);
	EnsureRotatePlacementInputActions(PlayerController, RotatePlacementCounterClockwiseAction, RotatePlacementClockwiseAction);
	EnsureRotateAssemblyPlacementInputAction(PlayerController, RotateAssemblyPlacementAction);
	EnsureStructureSelectionTabInputAction(PlayerController, StructureSelectionTabAction);

	ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
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
		RuntimeAssemblyInputMappingContext = NewObject<UInputMappingContext>(PlayerController, RuntimeAssemblyInputMappingContextName);
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
		if (RotateAssemblyPlacementAction)
		{
			RuntimeAssemblyInputMappingContext->MapKey(RotateAssemblyPlacementAction.Get(), EKeys::R);
		}
		if (StructureSelectionTabAction)
		{
			RuntimeAssemblyInputMappingContext->MapKey(StructureSelectionTabAction.Get(), EKeys::Tab);
		}
	}

	InputSubsystem->AddMappingContext(RuntimeAssemblyInputMappingContext, RuntimeAssemblyInputMappingPriority);
	RuntimeState.bRuntimeAssemblyInputMappingApplied = true;
}
