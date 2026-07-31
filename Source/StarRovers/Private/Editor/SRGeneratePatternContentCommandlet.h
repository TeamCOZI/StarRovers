#pragma once

#include "Commandlets/Commandlet.h"
#include "SRGeneratePatternContentCommandlet.generated.h"

UCLASS()
class USRGeneratePatternContentCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	USRGeneratePatternContentCommandlet();

	virtual int32 Main(const FString& Params) override;
};
