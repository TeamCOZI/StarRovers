#include "Camera/SRPlayerController.h"

#include "Assembly/SRAssemblyComponent.h"
#include "Automation/SRResourceV2AuthoredContent.h"
#include "SRPlayerControllerLifecycle.h"
#include "UI/SRFocusedHubShortcutWidget.h"
#include "UI/SRGameOverWidget.h"
#include "UI/SRPlayerGuidanceWidget.h"

ASRPlayerController::ASRPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	DefaultMouseCursor = EMouseCursor::Default;

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
	FocusedHubShortcutWidgetClass = USRFocusedHubShortcutWidget::StaticClass();
	FocusedHubShortcutRefreshInterval = 0.20f;
	PlayerGuidanceWidgetClass = USRPlayerGuidanceWidget::StaticClass();
	GameOverWidgetClass = USRGameOverWidget::StaticClass();

	TArray<FSoftObjectPath> ResourceV2StructurePaths;
	FSRResourceV2AuthoredContent::GetFacilityStructureObjectPaths(ResourceV2StructurePaths);
	AuthoredResourceV2StructureDataAssets.Reserve(ResourceV2StructurePaths.Num());
	for (const FSoftObjectPath& StructurePath : ResourceV2StructurePaths)
	{
		AuthoredResourceV2StructureDataAssets.Emplace(StructurePath);
	}

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
