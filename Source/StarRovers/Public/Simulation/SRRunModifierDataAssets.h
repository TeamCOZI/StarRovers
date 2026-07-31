#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Simulation/SRRunModifierTypes.h"
#include "SRRunModifierDataAssets.generated.h"

UENUM(BlueprintType)
enum class ESRRunAugmentRarity : uint8
{
	Common = 0 UMETA(DisplayName = "Common"),
	Rare = 1 UMETA(DisplayName = "Rare"),
	Epic = 2 UMETA(DisplayName = "Epic"),
};

UENUM(BlueprintType)
enum class ESRRunAugmentOfferRole : uint8
{
	Immediate = 0 UMETA(DisplayName = "Immediate"),
	Synergy = 1 UMETA(DisplayName = "Synergy"),
	Pivot = 2 UMETA(DisplayName = "Pivot"),
};

UCLASS(BlueprintType)
class STARROVERS_API USRTechnologyDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Technology", meta = (DisplayName = "TechnologyId"))
	FName TechnologyId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Technology", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Technology", meta = (DisplayName = "Description"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Technology", meta = (DisplayName = "UnlockedByDefault"))
	bool bUnlockedByDefault = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Technology", meta = (DisplayName = "PrerequisiteTechnologyIds"))
	TArray<FName> PrerequisiteTechnologyIds;

	// Technology is the sole progression authority for guaranteed facility access.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Technology", meta = (DisplayName = "UnlockedStructureIds"))
	TArray<FName> UnlockedStructureIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Technology|Modifier", meta = (DisplayName = "Priority"))
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Technology|Modifier", meta = (DisplayName = "Effects"))
	TArray<FSRRunModifierEffect> Effects;
};

UCLASS(BlueprintType)
class STARROVERS_API USRRunAugmentDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Augment", meta = (DisplayName = "AugmentId"))
	FName AugmentId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Augment", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Augment", meta = (DisplayName = "Description"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Augment", meta = (DisplayName = "Rarity"))
	ESRRunAugmentRarity Rarity = ESRRunAugmentRarity::Common;

	// Offer generation tries to expose Immediate, Synergy, and Pivot choices when
	// the registered pool permits it; this role is not itself a gameplay effect.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Augment", meta = (DisplayName = "OfferRole"))
	ESRRunAugmentOfferRole OfferRole = ESRRunAugmentOfferRole::Immediate;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Augment", meta = (DisplayName = "MaximumStacks", ClampMin = "1", ClampMax = "16"))
	int32 MaximumStacks = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Augment|Modifier", meta = (DisplayName = "Priority"))
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Augment|Modifier", meta = (DisplayName = "Effects"))
	TArray<FSRRunModifierEffect> Effects;
};

UCLASS(BlueprintType)
class STARROVERS_API USRTrialDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Trial", meta = (DisplayName = "TrialId"))
	FName TrialId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Trial", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Trial", meta = (DisplayName = "Description"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Trial", meta = (DisplayName = "DurationCycles", ClampMin = "1"))
	int32 DurationCycles = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Trial|Modifier", meta = (DisplayName = "Priority"))
	int32 Priority = 0;

	// Risk and reward use the same effect list. For example, a Trial may increase
	// environment intensity and required score while also multiplying bonus score.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Trial|Modifier", meta = (DisplayName = "Effects"))
	TArray<FSRRunModifierEffect> Effects;
};
