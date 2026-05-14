#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SRCelestialBodyRegistrySubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FSRCelestialRegistryPrimaryStarChangedSignature, AActor*);
DECLARE_MULTICAST_DELEGATE(FSRCelestialRegistryBodiesChangedSignature);

UCLASS()
class STARROVERS_API USRCelestialBodyRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Celestial|Registry")
	void RegisterCelestialBody(AActor* BodyActor);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Celestial|Registry")
	void UnregisterCelestialBody(AActor* BodyActor);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Celestial|Registry")
	void RefreshCelestialBodies();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Celestial|Registry")
	void GetCelestialBodies(TArray<AActor*>& OutCelestialBodies) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Celestial|Registry")
	void SetPrimaryStarActor(AActor* NewPrimaryStarActor);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial|Registry")
	AActor* GetPrimaryStarActor() const;

	FSRCelestialRegistryBodiesChangedSignature& OnCelestialBodiesChanged();
	FSRCelestialRegistryPrimaryStarChangedSignature& OnPrimaryStarActorChanged();

private:
	void CompactCelestialBodies();
	void SetResolvedPrimaryStarActor(AActor* NewPrimaryStarActor, bool bBroadcast);

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> CelestialBodies;

	UPROPERTY(Transient)
	TObjectPtr<AActor> PrimaryStarActor;

	FSRCelestialRegistryBodiesChangedSignature CelestialBodiesChangedEvent;
	FSRCelestialRegistryPrimaryStarChangedSignature PrimaryStarActorChangedEvent;
};
