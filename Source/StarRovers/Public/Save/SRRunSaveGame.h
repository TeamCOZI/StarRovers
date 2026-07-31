#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityRuntimeData.h"
#include "Celestial/SRCelestialBodyCategory.h"
#include "Celestial/SRStar.h"
#include "Conveyor/SRConveyorTypes.h"
#include "GameFramework/SaveGame.h"
#include "Logistics/SRSpaceLogisticsTypes.h"
#include "Simulation/SRAugmentSubsystem.h"
#include "Simulation/SROrbit.h"
#include "Simulation/SRRunModifierSubsystem.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "SRRunSaveGame.generated.h"

namespace StarRovers::Save::Run
{
	inline constexpr int32 CurrentVersion = 2;
	inline bool IsSupportedVersion(int32 Version) { return Version == CurrentVersion; }
	inline constexpr int32 CurrentTopologyContentVersion = 1;
}

USTRUCT(BlueprintType)
struct STARROVERS_API FSRRunGenerationSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	int32 ContentVersion = StarRovers::Save::Run::CurrentTopologyContentVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	FName GeneratorActorName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	int32 RuntimeGenerationSeed = 0;

	bool IsValid() const
	{
		return ContentVersion == StarRovers::Save::Run::CurrentTopologyContentVersion
			&& !GeneratorActorName.IsNone()
			&& RuntimeGenerationSeed >= 0;
	}
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRRunCelestialBodyKey
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	FName ActorName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	FString VariableName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	ESRCelestialBodyCategory BodyCategory = ESRCelestialBodyCategory::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	int32 GenerationSeed = 0;

	bool IsValid() const
	{
		return (!ActorName.IsNone() || !VariableName.IsEmpty())
			&& BodyCategory != ESRCelestialBodyCategory::Unknown;
	}
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRRunCelestialBodySaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	FSRRunCelestialBodyKey BodyKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	FTransform ActorTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	bool bHasOrbitState = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	FSROrbitSaveData OrbitState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	bool bHasSurfaceState = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	FSRStructureManagerSaveData Structures;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	FSRFacilityNetworkSaveData Facilities;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	FSRConveyorNetworkSaveData Conveyors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	bool bHasStarState = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	FSRStarSaveData StarState;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRRunSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	int32 Version = StarRovers::Save::Run::CurrentVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	FGuid SaveId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	FSRRunGenerationSaveData Generation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	FSRTimeControlSaveData TimeControl;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	FSRRunModifierSaveData RunModifiers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	FSRAugmentOfferSaveData AugmentOffer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	TArray<FSRRunCelestialBodySaveData> CelestialBodies;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	FSRSpaceLogisticsSaveData SpaceLogistics;
};

UCLASS(BlueprintType)
class STARROVERS_API USRRunSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run")
	FSRRunSaveData RunData;
};
