#include "Celestial/SRStar.h"

#include "Celestial/SRCelestialBodyCategory.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	void SetStarMaterialEmissiveStrength(UMaterialInstanceDynamic* MaterialInstance, const float Value)
	{
		if (!IsValid(MaterialInstance))
		{
			return;
		}

		MaterialInstance->SetScalarParameterValue(TEXT("Emissive Strength"), Value);
		MaterialInstance->SetScalarParameterValue(TEXT("EmissiveStrength"), Value);
		MaterialInstance->SetScalarParameterValue(TEXT("GlowIntensity"), Value);
		MaterialInstance->SetScalarParameterValue(TEXT("Glow"), Value);
		MaterialInstance->SetScalarParameterValue(TEXT("Intensity"), Value);
		MaterialInstance->SetScalarParameterValue(TEXT("Brightness"), Value);
	}
}

ASRStar::ASRStar()
{
	BodyCategory = ESRCelestialBodyCategory::Star;
	StarMaterialEmissiveStrength = 30.0f;
	StarPointLightIntensity = 100.0f;
	StarPointLightColor = FLinearColor(1.0f, 0.956f, 0.84f, 1.0f);

	StarPointLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StarPointLight"));
	StarPointLight->SetupAttachment(SceneRoot);
	StarPointLight->SetMobility(EComponentMobility::Movable);
	StarPointLight->SetVisibility(true);
	StarPointLight->SetUseInverseSquaredFalloff(false);
	StarPointLight->LightingChannels.bChannel0 = true;
	StarPointLight->LightingChannels.bChannel1 = false;
	StarPointLight->LightingChannels.bChannel2 = false;
}

void ASRStar::ApplySpec(const FSRCelestialBodySpec& NewSpec)
{
	StarMaterialEmissiveStrength = NewSpec.StarMaterialEmissiveStrength;
	StarPointLightIntensity = NewSpec.StarPointLightIntensity;
	StarPointLightColor = NewSpec.StarPointLightColor;

	Super::ApplySpec(NewSpec);
	ApplyStarAppearance();
}

void ASRStar::ApplyConfiguredBodyState()
{
	Super::ApplyConfiguredBodyState();
	ApplyStarAppearance();
}

void ASRStar::ApplyStarAppearance()
{
	StarMaterialEmissiveStrength = FMath::Max(0.0f, StarMaterialEmissiveStrength);
	StarPointLightIntensity = FMath::Max(0.0f, StarPointLightIntensity);

	if (UMaterialInstanceDynamic* StarMaterial = ResolveStarMaterialInstanceDynamic())
	{
		SetStarMaterialEmissiveStrength(StarMaterial, StarMaterialEmissiveStrength);
	}

	if (UPointLightComponent* ActiveStarPointLight = StarPointLight)
	{
		ActiveStarPointLight->SetVisibility(true);
		ActiveStarPointLight->SetUseInverseSquaredFalloff(false);
		ActiveStarPointLight->SetIntensityUnits(ELightUnits::Candelas);
		ActiveStarPointLight->SetIntensity(StarPointLightIntensity);
		ActiveStarPointLight->SetLightColor(StarPointLightColor);
	}
}

UMaterialInstanceDynamic* ASRStar::ResolveStarMaterialInstanceDynamic()
{
	UStaticMeshComponent* BodyStaticMesh = GetCelestialBodyStaticMesh();
	if (!IsValid(BodyStaticMesh))
	{
		return nullptr;
	}

	if (UMaterialInstanceDynamic* ExistingDynamicMaterial = Cast<UMaterialInstanceDynamic>(BodyStaticMesh->GetMaterial(0)))
	{
		return ExistingDynamicMaterial;
	}

	return BodyStaticMesh->CreateDynamicMaterialInstance(0);
}
