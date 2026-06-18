#include "Camera/SRPlayerController.h"

#include "Assembly/SRAssemblyComponent.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"

namespace StarRoversControllerInputPaths
{
	static constexpr TCHAR LeftClickAction[] = TEXT("/Game/BlueprintClasses/Core/IA_LeftClick.IA_LeftClick");
	static constexpr TCHAR DeleteStructureAction[] = TEXT("/Game/BlueprintClasses/Core/IA_DragHold.IA_DragHold");
	static constexpr TCHAR FocusParentAction[] = TEXT("/Game/BlueprintClasses/Core/IA_FocusParent.IA_FocusParent");
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

	FocusInfoWidgetZOrder = 0;
	OverviewWidgetZOrder = 1;
	TimeControlWidgetZOrder = 2;
	StructureSelectionWidgetZOrder = 3;
	MaxStructurePlacementsPerFrame = 4;
	MaxQueuedStructurePlacements = 256;
	SelectedStructureBuildId = NAME_None;
	bHasSelectedStructureBuildId = false;
	SelectedStructureDataAsset = nullptr;
	bPendingInitialPrimaryStarFocus = true;

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
	TryAutoFocusPrimaryStar();
}

void ASRPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateHitResultTraceDistance();
	TryBindCameraPawnFocusEvents();
	TryBindCelestialBodyRegistryEvents();
}
