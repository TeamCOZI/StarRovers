#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SRGameMode.generated.h"

class ASRSpaceshipActor;

UCLASS(Blueprintable)
class STARROVERS_API ASRGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ASRGameMode();

    UFUNCTION(BlueprintPure, Category = "StarRovers|Space Logistics")
    TSubclassOf<ASRSpaceshipActor> ResolveSpaceLogisticsSpaceshipActorClass() const;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "Spaceship Actor Class", ToolTip = "Visual actor class spawned for active space logistics routes and star fuel missiles. Set this on BP_SRGameMode."))
    TSoftClassPtr<ASRSpaceshipActor> SpaceLogisticsSpaceshipActorClass;
};
