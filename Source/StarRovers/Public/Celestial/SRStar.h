#pragma once

#include "CoreMinimal.h"
#include "Celestial/SRCelestialBody.h"
#include "SRStar.generated.h"

class UPointLightComponent;

UCLASS(Blueprintable)
class STARROVERS_API ASRStar : public ASRCelestialBody
{
	GENERATED_BODY()

public:
	ASRStar();

	virtual void SetData(const FSRCelestialBodyData& NewData) override;
	virtual void ApplyData() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "StarPointLight"))
	TObjectPtr<UPointLightComponent> StarPointLight;

	float StarPointLightIntensity = 100.0f;

	FLinearColor StarPointLightColor = FLinearColor(1.0f, 0.956f, 0.84f, 1.0f);

private:
	void ApplyStarAppearance();
};
