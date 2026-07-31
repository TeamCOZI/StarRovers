#include "Simulation/SRSolarSystemGenerator.h"

#include "Celestial/SRPlanet.h"
#include "Celestial/SRStar.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Conveyor/SRConveyorBeltActor.h"
#include "PCGComponent.h"
#include "Structure/SRStructure.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "TimerManager.h"
#include "UI/SRLoadingScreenWidget.h"
#include "Utility/SRLog.h"
#include "Utility/SRMemoryDiagnostics.h"
#include "Utility/SRTimingLog.h"
void ASRSolarSystemGenerator::EnsureMemoryDiagnosticTrackedClasses() const
{
	static bool bRegistered = false;
	if (bRegistered)
	{
		return;
	}

	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("ASRCelestialBody"), ASRCelestialBody::StaticClass(), TEXT("ASRCelestialBody"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("ASRPlanet"), ASRPlanet::StaticClass(), TEXT("ASRPlanet"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("ASRStar"), ASRStar::StaticClass(), TEXT("ASRStar"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("UDynamicMeshComponent"), UDynamicMeshComponent::StaticClass(), TEXT("UDynamicMeshComponent"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("USRPlanetSurfaceGrid"), USRPlanetSurfaceGrid::StaticClass(), TEXT("USRPlanetSurfaceGrid"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("USRStructureInstanceManagerComponent"), USRStructureInstanceManagerComponent::StaticClass(), TEXT("USRStructureInstanceManagerComponent"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("ASRStructure"), ASRStructure::StaticClass(), TEXT("ASRStructure"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("ASRConveyorBeltActor"), ASRConveyorBeltActor::StaticClass(), TEXT("ASRConveyorBeltActor"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("UHierarchicalInstancedStaticMeshComponent"), UHierarchicalInstancedStaticMeshComponent::StaticClass(), TEXT("UHierarchicalInstancedStaticMeshComponent"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("UPCGComponent"), UPCGComponent::StaticClass(), TEXT("UPCGComponent"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("USplineComponent"), USplineComponent::StaticClass(), TEXT("USplineComponent"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("USplineMeshComponent"), USplineMeshComponent::StaticClass(), TEXT("USplineMeshComponent"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("USRLoadingScreenWidget"), USRLoadingScreenWidget::StaticClass(), TEXT("USRLoadingScreenWidget"));
	bRegistered = true;
}

void ASRSolarSystemGenerator::LogMemoryDiagnosticsSnapshot(const FString& Label) const
{
	if (!bEnableMemoryDiagnostics)
	{
		return;
	}

	EnsureMemoryDiagnosticTrackedClasses();

	TArray<FString> ExtraLines;
	ExtraLines.Add(FString::Printf(
		TEXT("GeneratorRefs Star=%s Planets=%d Moons=%d NaturalStructureActors=%d LoadingScreen=%s"),
		*GetNameSafe(RuntimeStarBody.Get()),
		RuntimePlanetBodies.Num(),
		RuntimeMoonBodies.Num(),
		RuntimeNaturalStructureActors.Num(),
		*GetNameSafe(LoadingScreenWidget.Get())));

	if (const UWorld* World = GetWorld())
	{
		ExtraLines.Add(FString::Printf(
			TEXT("GeneratorTimer DeferredGenerationActive=%s"),
			World->GetTimerManager().IsTimerActive(DeferredGenerateRuntimeSystemTimerHandle) ? TEXT("true") : TEXT("false")));
	}

	ASRCelestialBody::AppendRuntimeMemoryDiagnostics(ExtraLines);
	FSRMemoryDiagnostics::LogSnapshot(GetWorld(), Label, ExtraLines);
}

void ASRSolarSystemGenerator::LogAsyncGenerationStageTiming(const TCHAR* StageName, double Milliseconds, const FString& Suffix)
{
	AsyncGenerationStageTimings.Add({ FString(StageName), Milliseconds });
	FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateRuntimeSystem.%s %.2f ms%s"), StageName, Milliseconds, *Suffix));
	SR_LOG(SolarSystem, LogTemp, Log,
		TEXT("Solar system generation stage '%s' completed in %.2f ms%s"),
		StageName,
		Milliseconds,
		*Suffix);
}
