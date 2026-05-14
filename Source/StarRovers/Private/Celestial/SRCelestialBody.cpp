#include "Celestial/SRCelestialBody.h"

#include "Components/LineBatchComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gravity/SRGravityParent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Components/SphereComponent.h"
#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

namespace
{
	FProperty* FindPropertyInClassHierarchy(const UClass* InClass, const FName PropertyName)
	{
		for (const UStruct* Struct = InClass; Struct; Struct = Struct->GetSuperStruct())
		{
			if (FProperty* Property = Struct->FindPropertyByName(PropertyName))
			{
				return Property;
			}
		}

		return nullptr;
	}

	bool CopyPropertyValueByName(UObject* DestinationObject, const UObject* SourceObject, const FName PropertyName)
	{
		if (!IsValid(DestinationObject) || !IsValid(SourceObject))
		{
			return false;
		}

		FProperty* DestinationProperty = FindPropertyInClassHierarchy(DestinationObject->GetClass(), PropertyName);
		FProperty* SourceProperty = FindPropertyInClassHierarchy(SourceObject->GetClass(), PropertyName);
		if (!DestinationProperty || !SourceProperty || !DestinationProperty->SameType(SourceProperty))
		{
			return false;
		}

		void* DestinationValuePtr = DestinationProperty->ContainerPtrToValuePtr<void>(DestinationObject);
		const void* SourceValuePtr = SourceProperty->ContainerPtrToValuePtr<void>(SourceObject);
		DestinationProperty->CopyCompleteValue(DestinationValuePtr, SourceValuePtr);
		return true;
	}

	bool TryGetFloatPropertyValue(const UObject* SourceObject, const FName PropertyName, float& OutValue)
	{
		if (!IsValid(SourceObject))
		{
			return false;
		}

		FProperty* SourceProperty = FindPropertyInClassHierarchy(SourceObject->GetClass(), PropertyName);
		if (!SourceProperty)
		{
			return false;
		}

		if (const FFloatProperty* FloatProperty = CastField<FFloatProperty>(SourceProperty))
		{
			OutValue = FloatProperty->GetPropertyValue_InContainer(SourceObject);
			return true;
		}

		if (const FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(SourceProperty))
		{
			OutValue = static_cast<float>(DoubleProperty->GetPropertyValue_InContainer(SourceObject));
			return true;
		}

		if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(SourceProperty))
		{
			const void* ValuePtr = SourceProperty->ContainerPtrToValuePtr<void>(SourceObject);
			OutValue = NumericProperty->IsFloatingPoint()
				? static_cast<float>(NumericProperty->GetFloatingPointPropertyValue(ValuePtr))
				: static_cast<float>(NumericProperty->GetSignedIntPropertyValue(ValuePtr));
			return true;
		}

		return false;
	}

	bool TryGetActorPropertyValue(const UObject* SourceObject, const FName PropertyName, AActor*& OutValue)
	{
		OutValue = nullptr;

		if (!IsValid(SourceObject))
		{
			return false;
		}

		FProperty* SourceProperty = FindPropertyInClassHierarchy(SourceObject->GetClass(), PropertyName);
		const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(SourceProperty);
		if (!ObjectProperty)
		{
			return false;
		}

		OutValue = Cast<AActor>(ObjectProperty->GetObjectPropertyValue_InContainer(SourceObject));
		return true;
	}

	float ComputeScaledMeshRadius(const UStaticMeshComponent* MeshComponent, const float BodyScale)
	{
		const UStaticMesh* MeshAsset = IsValid(MeshComponent) && IsValid(MeshComponent->GetStaticMesh())
			? MeshComponent->GetStaticMesh()
			: nullptr;
		if (!IsValid(MeshAsset))
		{
			return 0.0f;
		}

		const float MeshRadius = MeshAsset->GetBounds().SphereRadius;
		const FVector AppliedScale = IsValid(MeshComponent)
			? MeshComponent->GetRelativeScale3D()
			: FVector(FMath::Max(0.0f, BodyScale));
		const float UniformScale = FMath::Max3(
			FMath::Abs(AppliedScale.X),
			FMath::Abs(AppliedScale.Y),
			FMath::Abs(AppliedScale.Z));

		return FMath::Max(0.0f, MeshRadius * UniformScale);
	}
}

DEFINE_LOG_CATEGORY_STATIC(LogStarRoversCelestial, Log, All);

FSRCelestialBodySpec::FSRCelestialBodySpec()
{
	DisplayName = FText::FromString(TEXT("Primary Star"));
	BodyCategory = ESRCelestialBodyCategory::Star;
	OrbitPeriodInPeriods = 0.0f;
	ShadowCasterScaleMultiplier = 0.95f;
	ConstructionHeightOffset = 15.0f;
	bHasOcean = false;
	OceanMesh = nullptr;
	OceanMaterial = nullptr;
	OceanScaleMultiplier = 0.97f;
}

FSRCelestialBodyBiomeSpec::FSRCelestialBodyBiomeSpec()
{
	DisplayName = FText::FromString(TEXT("Biome"));
	bUseProceduralTerrain = true;
	TerrainSeed = 1337;
	TerrainHeight = 0.0f;
	TerrainFrequency = 3.0f;
	TerrainOctaves = 4;
	TerrainPersistence = 0.5f;
	bHasOcean = false;
	OceanMesh = nullptr;
	OceanMaterial = nullptr;
	OceanScaleMultiplier = 0.97f;
	SurfaceGridSurfaceOffset = 8.0f;
	ShadowCasterScaleMultiplier = 0.95f;
	OrbitSpeed = 1.0f;
	StarPointLightIntensity = -1.0f;
	StarMaterialEmissiveStrength = -1.0f;
	StarPointLightColor = FLinearColor(1.0f, 0.956f, 0.84f, 1.0f);
}

ASRCelestialBody::ASRCelestialBody()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CelestialBodyStaticMesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CelestialBodyStaticMesh"));
	CelestialBodyStaticMesh_->SetupAttachment(SceneRoot);
	CelestialBodyStaticMesh_->SetMobility(EComponentMobility::Movable);
	CelestialBodyStaticMesh_->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CelestialBodyStaticMesh_->SetCollisionResponseToAllChannels(ECR_Block);
	CelestialBodyStaticMesh_->SetGenerateOverlapEvents(false);

	ClickSphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("ClickSphereCollision"));
	ClickSphereCollision->SetupAttachment(SceneRoot);
	ClickSphereCollision->SetMobility(EComponentMobility::Movable);
	ClickSphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ClickSphereCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	ClickSphereCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ClickSphereCollision->SetGenerateOverlapEvents(false);
	ClickSphereCollision->SetCanEverAffectNavigation(false);

	GravityParent = CreateDefaultSubobject<USRGravityParent>(TEXT("GravityParent"));

	GravityLineBatch = CreateDefaultSubobject<ULineBatchComponent>(TEXT("Gravity Line Batch"));
	GravityLineBatch->SetupAttachment(SceneRoot);
	GravityLineBatch->SetMobility(EComponentMobility::Movable);
	GravityLineBatch->SetUsingAbsoluteLocation(true);
	GravityLineBatch->SetUsingAbsoluteRotation(true);
	GravityLineBatch->SetUsingAbsoluteScale(true);
	GravityLineBatch->ComponentTags.AddUnique(TEXT("StarRovers.GravityLine"));
	GravityLineBatch->ComponentTags.AddUnique(TEXT("StarRovers.GravityLineRoot"));

	DisplayName = FText::FromString(TEXT("Celestial Body"));
	BodyCategory = ESRCelestialBodyCategory::Unknown;
	FocusZoomMultiplier = 3.0f;
	BodyScale = 1000.0f;
	ApproximateRadius = 50000.0f;
	Mass = 2000.0f;
	GenerationSeed = 1000;
	GravityRatio = 1.0f;
	GravityRadiusRatio = 10.0f;
	bShowGravityLine = true;
	GravityLineColor = FLinearColor(0.45f, 1.0f, 0.45f, 1.0f);
	GravityLineOpacity = 0.85f;
	GravityLineSegments = 96;
	GravityLineThickness = 20.0f;
}

void ASRCelestialBody::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyConfiguredBodyState();
}

void ASRCelestialBody::BeginPlay()
{
	Super::BeginPlay();

	if (!bHasAppliedSpec)
	{
		LogMissingSpecErrorOnce(TEXT("BeginPlay"));
		SetActorTickEnabled(false);
		return;
	}

	SetActorTickEnabled(true);

	if (USRCelestialBodyRegistrySubsystem* CelestialRegistry = FindCelestialRegistry())
	{
		CelestialRegistry->RegisterCelestialBody(this);
	}

}

void ASRCelestialBody::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USRCelestialBodyRegistrySubsystem* CelestialRegistry = FindCelestialRegistry())
	{
		CelestialRegistry->UnregisterCelestialBody(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ASRCelestialBody::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

#if WITH_EDITOR
void ASRCelestialBody::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ApplyConfiguredBodyState();
}
#endif

void ASRCelestialBody::ApplySpec(const FSRCelestialBodySpec& NewSpec)
{
	bHasAppliedSpec = true;
	bHasLoggedMissingSpecError = false;
	DisplayName = NewSpec.DisplayName;
	BodyCategory = NewSpec.BodyCategory;
	FocusZoomMultiplier = NewSpec.FocusZoomMultiplier;
	GenerationSeed = NewSpec.TerrainSeed;
	BodyScale = NewSpec.BodyScale;
	Mass = NewSpec.Mass;
	GravityRatio = NewSpec.GravityRatio;
	GravityRadiusRatio = NewSpec.GravityRadiusRatio;
	bShowGravityLine = NewSpec.bShowGravityLine;
	GravityLineColor = NewSpec.GravityLineColor;
	GravityLineOpacity = NewSpec.GravityLineOpacity;
	GravityLineSegments = NewSpec.GravityLineSegments;
	GravityLineThickness = NewSpec.GravityLineThickness;

	if (HasActorBegunPlay() && GetWorld() && GetWorld()->IsGameWorld() && IsValid(CelestialBodyStaticMesh) && IsValid(CelestialBodyMaterial))
	{
		ApplyConfiguredBodyState();
	}
}

void ASRCelestialBody::ApplyBiomeSpec(const FSRCelestialBodyBiomeSpec& NewBiomeSpec)
{
	bHasAppliedSpec = true;
	bHasLoggedMissingSpecError = false;

	DisplayName = NewBiomeSpec.DisplayName;
	GenerationSeed = NewBiomeSpec.TerrainSeed;

	ApplyConfiguredBodyState();
}

void ASRCelestialBody::ApplyConfiguredBodyState()
{
	if (!bHasAppliedSpec && GetWorld() && GetWorld()->IsGameWorld())
	{
		LogMissingSpecErrorOnce(TEXT("ApplyConfiguredBodyState"));
		return;
	}

	if (PrimaryActorTick.bCanEverTick)
	{
		SetActorTickEnabled(true);
	}

	BodyScale = FMath::Max(0.0f, BodyScale);
	Mass = FMath::Max(0.0f, Mass);
	GravityRatio = FMath::Max(0.0f, GravityRatio);
	GravityRadiusRatio = FMath::Max(0.0f, GravityRadiusRatio);
	GravityLineOpacity = FMath::Clamp(GravityLineOpacity, 0.0f, 1.0f);
	GravityLineSegments = FMath::Max(3, GravityLineSegments);
	GravityLineThickness = FMath::Max(0.0f, GravityLineThickness);
	FocusZoomMultiplier = FMath::Max(0.0f, FocusZoomMultiplier);

	SetActorScale3D(FVector::OneVector);
	if (IsValid(CelestialBodyStaticMesh_))
	{
		CelestialBodyStaticMesh_->SetRelativeLocation(FVector::ZeroVector);
		CelestialBodyStaticMesh_->SetRelativeRotation(FRotator::ZeroRotator);
		CelestialBodyStaticMesh_->SetRelativeScale3D(FVector(BodyScale));
	}

	EnsureCelestialBodyStaticMeshVisuals();

	if (IsValid(ClickSphereCollision))
	{
		ClickSphereCollision->SetRelativeLocation(FVector::ZeroVector);
		ClickSphereCollision->SetRelativeRotation(FRotator::ZeroRotator);
		ClickSphereCollision->SetRelativeScale3D(FVector::OneVector);
		ClickSphereCollision->SetSphereRadius(FMath::Max(ComputeCelestialBodyStaticMeshRadius(), 1.0f));
	}
	ApplyGravityLineSettings();
	SyncApproximateRadiusFromCelestialBodyStaticMesh();
}

void ASRCelestialBody::SyncApproximateRadiusFromCelestialBodyStaticMesh()
{
	ApproximateRadius = ComputeCelestialBodyStaticMeshRadius();
}

FSRCelestialBodySpec ASRCelestialBody::GetSpec() const
{
	FSRCelestialBodySpec CurrentSpec;
	CurrentSpec.DisplayName = DisplayName;
	CurrentSpec.BodyCategory = BodyCategory;
	CurrentSpec.FocusZoomMultiplier = FocusZoomMultiplier;
	CurrentSpec.TerrainSeed = GenerationSeed;
	CurrentSpec.BodyScale = BodyScale;
	CurrentSpec.ApproximateRadius = ApproximateRadius;
	CurrentSpec.Mass = Mass;
	CurrentSpec.GravityRatio = GravityRatio;
	CurrentSpec.GravityRadiusRatio = GravityRadiusRatio;
	CurrentSpec.bShowGravityLine = bShowGravityLine;
	CurrentSpec.GravityLineColor = GravityLineColor;
	CurrentSpec.GravityLineOpacity = GravityLineOpacity;
	CurrentSpec.GravityLineSegments = GravityLineSegments;
	CurrentSpec.GravityLineThickness = GravityLineThickness;
	return CurrentSpec;
}

float ASRCelestialBody::ConvertPeriodsToSeconds(float PeriodValue) const
{
	return FMath::Max(0.0f, PeriodValue) * FMath::Max(0.0f, ResolveSecondsPerPeriod());
}

UStaticMeshComponent* ASRCelestialBody::GetCelestialBodyStaticMesh() const
{
	return CelestialBodyStaticMesh_;
}

ESRCelestialBodyCategory ASRCelestialBody::GetBodyCategory() const
{
	return BodyCategory;
}

USRGravityParent* ASRCelestialBody::GetGravityParent() const
{
	return GravityParent;
}

USROrbit* ASRCelestialBody::GetOrbitComponent() const
{
	return nullptr;
}

USRPlanetSurfaceGrid* ASRCelestialBody::GetSurfaceGrid() const
{
	return nullptr;
}

void ASRCelestialBody::SetCelestialBodyAssets(UStaticMesh* NewCelestialBodyStaticMesh, UMaterialInterface* NewCelestialBodyMaterial)
{
	CelestialBodyStaticMesh = NewCelestialBodyStaticMesh;
	CelestialBodyMaterial = NewCelestialBodyMaterial;
	if (bHasAppliedSpec && HasActorBegunPlay())
	{
		ApplyConfiguredBodyState();
	}
}

void ASRCelestialBody::ApplyGravityLineSettings()
{
	if (!IsValid(GravityParent))
	{
		return;
	}

	static const FName GravityPropertyNames[] =
	{
		TEXT("Mass"),
		TEXT("GravityRatio"),
		TEXT("GravityRadiusRatio"),
		TEXT("bShowGravityLine"),
		TEXT("GravityLineColor"),
		TEXT("GravityLineOpacity"),
		TEXT("GravityLineSegments"),
		TEXT("GravityLineThickness")
	};

	for (const FName PropertyName : GravityPropertyNames)
	{
		CopyPropertyValueByName(GravityParent, this, PropertyName);
	}

	GravityParent->RecomputeDerivedValues();
}

void ASRCelestialBody::EnsureCelestialBodyStaticMeshVisuals()
{
	if (!IsValid(CelestialBodyStaticMesh_))
	{
		return;
	}

	UStaticMesh* DesiredMesh = nullptr;
	if (IsValid(CelestialBodyStaticMesh))
	{
		DesiredMesh = CelestialBodyStaticMesh.Get();
	}
	if (!IsValid(DesiredMesh))
	{
		UE_LOG(LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires CelestialBodyStaticMesh."), *GetName());
		return;
	}

	if (IsValid(DesiredMesh) && CelestialBodyStaticMesh_->GetStaticMesh() != DesiredMesh)
	{
		CelestialBodyStaticMesh_->SetStaticMesh(DesiredMesh);
	}

	UMaterialInterface* DesiredBaseMaterial = CelestialBodyMaterial;
	UMaterialInterface* CurrentAssignedMaterial = CelestialBodyStaticMesh_->GetMaterial(0);

	if (!IsValid(DesiredBaseMaterial))
	{
		if (IsStellarBody() && IsValid(CurrentAssignedMaterial))
		{
			DesiredBaseMaterial = CurrentAssignedMaterial;
		}
	}

	if (!IsValid(DesiredBaseMaterial))
	{
		UE_LOG(LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires CelestialBodyMaterial."), *GetName());
		return;
	}

	UMaterialInstanceDynamic* ActiveDynamicMaterial = GetActiveBodyDynamicMaterial();
	const UMaterialInstance* ActiveMaterialInstance = ActiveDynamicMaterial;
	if (!IsValid(ActiveDynamicMaterial) || ActiveMaterialInstance->Parent != DesiredBaseMaterial)
	{
		ActiveDynamicMaterial = UMaterialInstanceDynamic::Create(DesiredBaseMaterial, this);
		if (IsValid(ActiveDynamicMaterial))
		{
			CelestialBodyStaticMesh_->SetMaterial(0, ActiveDynamicMaterial);
		}
		else
		{
			CelestialBodyStaticMesh_->SetMaterial(0, DesiredBaseMaterial);
		}
	}
}

UMaterialInstanceDynamic* ASRCelestialBody::GetActiveBodyDynamicMaterial() const
{
	return IsValid(CelestialBodyStaticMesh_)
		? Cast<UMaterialInstanceDynamic>(CelestialBodyStaticMesh_->GetMaterial(0))
		: nullptr;
}

float ASRCelestialBody::ComputeCelestialBodyStaticMeshRadius() const
{
	const float CelestialBodyStaticMeshRadius = ComputeScaledMeshRadius(CelestialBodyStaticMesh_, BodyScale);
	if (CelestialBodyStaticMeshRadius <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires a valid CelestialBodyStaticMesh to compute approximate radius."), *GetName());
	}

	return CelestialBodyStaticMeshRadius;
}

void ASRCelestialBody::LogMissingSpecErrorOnce(const TCHAR* Context) const
{
	if (bHasLoggedMissingSpecError)
	{
		return;
	}

	bHasLoggedMissingSpecError = true;
	UE_LOG(
		LogStarRoversCelestial,
		Error,
		TEXT("%s '%s' requires a data-driven body spec before runtime use. ApplySpec() was never called. Configure it through a data asset-driven spawn path instead of Blueprint defaults."),
		Context ? Context : TEXT("ASRCelestialBody"),
		*GetName());
}

USRCelestialBodyRegistrySubsystem* ASRCelestialBody::FindCelestialRegistry() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USRCelestialBodyRegistrySubsystem>() : nullptr;
}

USRTimeControlSubsystem* ASRCelestialBody::FindTimeControlSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USRTimeControlSubsystem>() : nullptr;
}

bool ASRCelestialBody::IsStellarBody() const
{
	return BodyCategory == ESRCelestialBodyCategory::Star;
}

float ASRCelestialBody::ResolveSecondsPerPeriod() const
{
	if (const USRTimeControlSubsystem* TimeControlSubsystem = FindTimeControlSubsystem())
	{
		return FMath::Max(0.0f, TimeControlSubsystem->GetSecondsPerPeriod());
	}

	return 20.0f;
}
