#pragma once

#include "Commandlets/Commandlet.h"
#include "SRRunResourceEconomySoakCommandlet.generated.h"

/** Runs the authored multi-seed Resource V2 portfolio and economy balance gate. */
UCLASS()
class USRRunResourceEconomySoakCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	USRRunResourceEconomySoakCommandlet();
	virtual int32 Main(const FString& Params) override;
};
