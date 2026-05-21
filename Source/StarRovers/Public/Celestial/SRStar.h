#pragma once

#include "CoreMinimal.h"
#include "Celestial/SRCelestialBody.h"
#include "SRStar.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;

UCLASS(Blueprintable)
class STARROVERS_API ASRStar : public ASRCelestialBody
{
	GENERATED_BODY()

public:
	ASRStar();

	virtual void ApplySpec(const FSRCelestialBodySpec& NewSpec) override;
	virtual void ApplyConfiguredBodyState() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "StarPointLight"))
	TObjectPtr<UPointLightComponent> StarPointLight;

	float StarMaterialEmissiveStrength = 30.0f;

	float StarPointLightIntensity = 100.0f;

	FLinearColor StarPointLightColor = FLinearColor(1.0f, 0.956f, 0.84f, 1.0f);

private:
	void ApplyStarAppearance();
	UMaterialInstanceDynamic* ResolveStarMaterialInstanceDynamic();
};
