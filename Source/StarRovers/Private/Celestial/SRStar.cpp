#include "Celestial/SRStar.h"

#include "Celestial/SRCelestialBodyCategory.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

ASRStar::ASRStar()
{
	BodyCategory = ESRCelestialBodyCategory::Star;
	StarPointLightIntensity = 100.0f;
	StarPointLightColor = FLinearColor(1.0f, 0.956f, 0.84f, 1.0f);

	StarPointLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StarPointLight"));
	StarPointLight->SetupAttachment(SceneRoot);
	StarPointLight->SetMobility(EComponentMobility::Movable);
	StarPointLight->SetVisibility(true);
	StarPointLight->SetUseInverseSquaredFalloff(false);
}

void ASRStar::SetData(const FSRCelestialBodyData& NewData)
{
	StarPointLightIntensity = NewData.StarPointLightIntensity;
	StarPointLightColor = NewData.StarPointLightColor;

	Super::SetData(NewData);
	ApplyStarAppearance();
}

void ASRStar::ApplyData()
{
	Super::ApplyData();
	ApplyStarAppearance();
}

void ASRStar::ApplyStarAppearance()
{
	StarPointLightIntensity = FMath::Max(0.0f, StarPointLightIntensity);

	if (IsValid(CelestialBodyStaticMesh.Get()))
	{
		CelestialBodyStaticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	for (UDynamicMeshComponent* DynamicMeshComponent : CelestialBodyDynamicMeshFaces)
	{
		if (IsValid(DynamicMeshComponent))
		{
			DynamicMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	if (IsValid(ClickSphereCollision.Get()))
	{
		ClickSphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		ClickSphereCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
		ClickSphereCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		ClickSphereCollision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
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
