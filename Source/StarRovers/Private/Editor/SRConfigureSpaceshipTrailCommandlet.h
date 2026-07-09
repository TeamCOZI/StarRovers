#pragma once

#include "Commandlets/Commandlet.h"
#include "SRConfigureSpaceshipTrailCommandlet.generated.h"

UCLASS()
class USRConfigureSpaceshipTrailCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	USRConfigureSpaceshipTrailCommandlet();

	virtual int32 Main(const FString& Params) override;
};
