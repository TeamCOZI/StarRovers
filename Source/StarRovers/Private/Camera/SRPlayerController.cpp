#include "Camera/SRPlayerController.h"

#include "Assembly/SRAssemblyComponent.h"
#include "SRPlayerControllerDefaultInputActions.h"
#include "SRPlayerControllerLifecycle.h"

ASRPlayerController::ASRPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	DefaultMouseCursor = EMouseCursor::Default;

	FSRPlayerControllerDefaultInputActions::Load(
		LeftClickAction,
		FocusParentAction,
		DeleteStructureAction,
		AssemblyUndoRedoAction,
		AssemblyAreaSelectionCopyAction,
		AssemblyAreaCopyMirrorAction,
		AssemblyPickStructureAction,
		RotatePlacementCounterClockwiseAction,
		RotatePlacementClockwiseAction,
		RotateAssemblyPlacementAction);

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

	FSRPlayerControllerLifecycle::BeginPlay(*this);
}

void ASRPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FSRPlayerControllerLifecycle::Tick(*this);
}
