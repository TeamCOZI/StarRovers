#pragma once

#include "Commandlets/Commandlet.h"
#include "SRGenerateResourceV2ContentCommandlet.generated.h"

UCLASS()
class USRGenerateResourceV2ContentCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	USRGenerateResourceV2ContentCommandlet();

	virtual int32 Main(const FString& Params) override;
};
