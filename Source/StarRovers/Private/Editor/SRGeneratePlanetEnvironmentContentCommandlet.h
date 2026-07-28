#pragma once

#include "Commandlets/Commandlet.h"
#include "SRGeneratePlanetEnvironmentContentCommandlet.generated.h"

/** Reproducible authoring and inspection entry point for test-play planet environments. */
UCLASS()
class USRGeneratePlanetEnvironmentContentCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	USRGeneratePlanetEnvironmentContentCommandlet();

	virtual int32 Main(const FString& Params) override;
};
