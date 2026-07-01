#include "Camera/SRPlayerController.h"

#include "Assembly/SRAssemblyComponent.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"

namespace StarRoversControllerInputPaths
{
	static constexpr TCHAR LeftClickAction[] = TEXT("/Game/BlueprintClasses/Core/IA_LeftClick.IA_LeftClick");
	static constexpr TCHAR DeleteStructureAction[] = TEXT("/Game/BlueprintClasses/Core/IA_DragHold.IA_DragHold");
	static constexpr TCHAR FocusParentAction[] = TEXT("/Game/BlueprintClasses/Core/IA_FocusParent.IA_FocusParent");
	static constexpr TCHAR AssemblyUndoRedoAction[] = TEXT("/Game/BlueprintClasses/Core/IA_AssemblyUndoRedoAction.IA_AssemblyUndoRedoAction");
	static constexpr TCHAR AssemblyAreaSelectionCopyAction[] = TEXT("/Game/BlueprintClasses/Core/IA_AssemblyAreaSelectionCopyAction.IA_AssemblyAreaSelectionCopyAction");
	static constexpr TCHAR AssemblyAreaCopyMirrorAction[] = TEXT("/Game/BlueprintClasses/Core/IA_AssemblyAreaCopyMirrorAction.IA_AssemblyAreaCopyMirrorAction");
	static constexpr TCHAR AssemblyPickStructureAction[] = TEXT("/Game/BlueprintClasses/Core/IA_AssemblyPickStructureAction.IA_AssemblyPickStructureAction");
	static constexpr TCHAR RotatePlacementCounterClockwiseAction[] = TEXT("/Game/BlueprintClasses/Core/IA_RotatePlacementCounterClockwiseAction.IA_RotatePlacementCounterClockwiseAction");
	static constexpr TCHAR RotatePlacementClockwiseAction[] = TEXT("/Game/BlueprintClasses/Core/IA_RotatePlacementClockwiseAction.IA_RotatePlacementClockwiseAction");
}

ASRPlayerController::ASRPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	DefaultMouseCursor = EMouseCursor::Default;

	static ConstructorHelpers::FObjectFinder<UInputAction> LeftClickActionFinder(StarRoversControllerInputPaths::LeftClickAction);
	if (LeftClickActionFinder.Succeeded())
	{
		LeftClickAction = LeftClickActionFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ASRPlayerController requires LeftClickAction at '%s'."), StarRoversControllerInputPaths::LeftClickAction);
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> FocusParentActionFinder(StarRoversControllerInputPaths::FocusParentAction);
	if (FocusParentActionFinder.Succeeded())
	{
		FocusParentAction = FocusParentActionFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ASRPlayerController requires FocusParentAction at '%s'."), StarRoversControllerInputPaths::FocusParentAction);
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> DeleteStructureActionFinder(StarRoversControllerInputPaths::DeleteStructureAction);
	if (DeleteStructureActionFinder.Succeeded())
	{
		DeleteStructureAction = DeleteStructureActionFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires DeleteStructureAction at '%s' for right-click structure deletion."), StarRoversControllerInputPaths::DeleteStructureAction);
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> AssemblyUndoRedoActionFinder(StarRoversControllerInputPaths::AssemblyUndoRedoAction);
	if (AssemblyUndoRedoActionFinder.Succeeded())
	{
		AssemblyUndoRedoAction = AssemblyUndoRedoActionFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyUndoRedoAction at '%s' for assembly undo/redo."), StarRoversControllerInputPaths::AssemblyUndoRedoAction);
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> AssemblyAreaSelectionCopyActionFinder(StarRoversControllerInputPaths::AssemblyAreaSelectionCopyAction);
	if (AssemblyAreaSelectionCopyActionFinder.Succeeded())
	{
		AssemblyAreaSelectionCopyAction = AssemblyAreaSelectionCopyActionFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyAreaSelectionCopyAction at '%s' for assembly area copy."), StarRoversControllerInputPaths::AssemblyAreaSelectionCopyAction);
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> AssemblyAreaCopyMirrorActionFinder(StarRoversControllerInputPaths::AssemblyAreaCopyMirrorAction);
	if (AssemblyAreaCopyMirrorActionFinder.Succeeded())
	{
		AssemblyAreaCopyMirrorAction = AssemblyAreaCopyMirrorActionFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("ASRPlayerController did not find AssemblyAreaCopyMirrorAction at '%s'; a runtime F-key input action will be created."), StarRoversControllerInputPaths::AssemblyAreaCopyMirrorAction);
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> AssemblyPickStructureActionFinder(StarRoversControllerInputPaths::AssemblyPickStructureAction);
	if (AssemblyPickStructureActionFinder.Succeeded())
	{
		AssemblyPickStructureAction = AssemblyPickStructureActionFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("ASRPlayerController did not find AssemblyPickStructureAction at '%s'; a runtime Z-key input action will be created."), StarRoversControllerInputPaths::AssemblyPickStructureAction);
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> RotatePlacementCounterClockwiseActionFinder(StarRoversControllerInputPaths::RotatePlacementCounterClockwiseAction);
	if (RotatePlacementCounterClockwiseActionFinder.Succeeded())
	{
		RotatePlacementCounterClockwiseAction = RotatePlacementCounterClockwiseActionFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("ASRPlayerController did not find RotatePlacementCounterClockwiseAction at '%s'; a runtime Q-key input action will be created."), StarRoversControllerInputPaths::RotatePlacementCounterClockwiseAction);
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> RotatePlacementClockwiseActionFinder(StarRoversControllerInputPaths::RotatePlacementClockwiseAction);
	if (RotatePlacementClockwiseActionFinder.Succeeded())
	{
		RotatePlacementClockwiseAction = RotatePlacementClockwiseActionFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("ASRPlayerController did not find RotatePlacementClockwiseAction at '%s'; a runtime E-key input action will be created."), StarRoversControllerInputPaths::RotatePlacementClockwiseAction);
	}

	WidgetLayerOrder = StarRovers::PlayerControllerUI::MakeDefaultWidgetLayerOrder();
	MaxStructurePlacementsPerFrame = 4;
	MaxQueuedStructurePlacements = 256;
	AssemblyModeScreenSizeThreshold = 0.30f;
	SelectedStructureBuildId = NAME_None;
	bHasSelectedStructureBuildId = false;
	SelectedStructureDataAsset = nullptr;
	AssemblyAreaDeletionDragHoldAction = nullptr;
	AssemblyAreaSelectionDeleteAction = nullptr;
	ConveyorWaypointAction = nullptr;
	BulkDeleteConveyorModifierAction = nullptr;
	AssemblyShiftModifierAction = nullptr;

	AssemblyComponent = CreateDefaultSubobject<USRAssemblyComponent>(TEXT("AssemblyComponent"));
	AssemblyComponent->ConfigurePlacementPerformance(MaxStructurePlacementsPerFrame, MaxQueuedStructurePlacements);
}

void ASRPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (AssemblyComponent)
	{
		AssemblyComponent->ConfigurePlacementPerformance(MaxStructurePlacementsPerFrame, MaxQueuedStructurePlacements);
	}
	ApplyRuntimeAssemblyInputMapping();

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	UpdateHitResultTraceDistance();
	TryBindCameraPawnFocusEvents();
	TryBindCelestialBodyRegistryEvents();
	CreateFocusInfoWidget();
	RefreshFocusInfoWidget();
	CreateOverviewWidget();
	RefreshOverviewWidget();
	CreateTimeControlWidget();
	CreateStructureSelectionWidget();
	RefreshStructureSelectionWidget();
	CreateFacilityControlWidget();
	RefreshFacilityControlWidget();
	TryAutoFocusPrimaryStar();
}

void ASRPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateHitResultTraceDistance();
	TryBindCameraPawnFocusEvents();
	TryBindCelestialBodyRegistryEvents();
	if (!RuntimeState.bRuntimeAssemblyInputMappingApplied)
	{
		ApplyRuntimeAssemblyInputMapping();
	}
	UpdateAssemblyModeFromFocusedActorScreenSize();
}
