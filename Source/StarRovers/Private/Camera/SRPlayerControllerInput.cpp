#include "Camera/SRPlayerController.h"

#include "Assembly/SRAssemblyComponent.h"
#include "Camera/SRCameraPawn.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "EnhancedInputComponent.h"
void ASRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

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

		if (DeleteStructureAction)
		{
			EnhancedInputComponent->BindAction(DeleteStructureAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleRightClick);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires DeleteStructureAction before right-click structure deletion binding."));
		}
	}
}

void ASRPlayerController::HandleLeftClick()
{
	bPendingInitialPrimaryStarFocus = false;

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
	AActor* SelectedBody = USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(HitActor) ? HitActor : nullptr;
	if (!SelectedBody)
	{
		if (const ASRCameraPawn* CameraPawn = Cast<ASRCameraPawn>(GetPawn()))
		{
			AActor* CurrentFocusActor = CameraPawn->GetFocusedActor();
			if (IsValid(CurrentFocusActor)
				&& USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(CurrentFocusActor)
				&& !USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(CurrentFocusActor))
			{
				return;
			}
		}
	}

	RequestFocusActor(SelectedBody);
}

void ASRPlayerController::HandleRightClick()
{
	AActor* AssemblySelectedActor = nullptr;
	if (AssemblyComponent && AssemblyComponent->TryHandleAssemblyDelete(AssemblySelectedActor))
	{
		if (IsValid(AssemblySelectedActor) && SelectedActor != AssemblySelectedActor)
		{
			UpdateSelection(AssemblySelectedActor);
		}
	}
}

void ASRPlayerController::HandleFocusParent()
{
	bPendingInitialPrimaryStarFocus = false;

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
