#pragma once

#include "CoreMinimal.h"
#include "Automation/SRResourceDataAsset.h"
#include "Engine/DataAsset.h"
#include "Pattern/SRPatternFacilityResolver.h"
#include "SRFacilityDataAsset.generated.h"

UENUM(BlueprintType)
enum class ESRFacilityKind : uint8
{
	Standard = 0 UMETA(DisplayName = "Standard"),
	Hub = 1 UMETA(DisplayName = "Hub"),
};

UENUM(BlueprintType)
enum class ESRFacilityRarity : uint8
{
	Basic = 0 UMETA(DisplayName = "Basic"),
	Advanced = 1 UMETA(DisplayName = "Advanced"),
	HighTech = 2 UMETA(DisplayName = "HighTech"),
	Innovation = 3 UMETA(DisplayName = "Innovation"),
	Starting = 4 UMETA(DisplayName = "Starting"),
};

UENUM(BlueprintType)
enum class ESRFacilityOperationKind : uint8
{
	// The serialized identifier is retained; the operation is a Pattern transform.
	Process = 0 UMETA(DisplayName = "Transform"),
	Synthesize = 1 UMETA(DisplayName = "Synthesize"),
	Separate = 2 UMETA(DisplayName = "Separate"),
	Mine = 3 UMETA(DisplayName = "Mine"),
};

UENUM(BlueprintType)
enum class ESRFacilityTemperatureState : uint8
{
	Frozen UMETA(DisplayName = "Frozen"),
	Cold UMETA(DisplayName = "Cold"),
	Normal UMETA(DisplayName = "Normal"),
	Hot UMETA(DisplayName = "Hot"),
	Overheated UMETA(DisplayName = "Overheated"),
};

UENUM(BlueprintType)
enum class ESRFacilityPortKind : uint8
{
	Input UMETA(DisplayName = "Input"),
	Output UMETA(DisplayName = "Output"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFacilityInventorySpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Inventory", meta = (DisplayName = "SlotCount", ClampMin = "0"))
	int32 SlotCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Inventory", meta = (DisplayName = "SlotCapacity", ClampMin = "1"))
	int32 SlotCapacity = 8;
};

UCLASS(BlueprintType)
class STARROVERS_API USRFacilityDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	USRFacilityDataAsset();

	virtual void PostLoad() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "FacilityId"))
	FName FacilityId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "Description"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "FacilityKind"))
	ESRFacilityKind FacilityKind = ESRFacilityKind::Standard;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "Rarity", EditCondition = "FacilityKind == ESRFacilityKind::Standard", EditConditionHides))
	ESRFacilityRarity Rarity = ESRFacilityRarity::Basic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "OperationKind", EditCondition = "FacilityKind == ESRFacilityKind::Standard", EditConditionHides))
	ESRFacilityOperationKind OperationKind = ESRFacilityOperationKind::Process;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "BaseProcessSeconds", ClampMin = "0.01"))
	float BaseProcessSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility|Pattern", meta = (
		DisplayName = "TransformOperator",
		EditCondition = "FacilityKind == ESRFacilityKind::Standard && OperationKind == ESRFacilityOperationKind::Process",
		EditConditionHides))
	FSRPatternTransformOperatorSpec TransformOperator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility|Pattern", meta = (
		DisplayName = "SynthesisOutputResource",
		EditCondition = "FacilityKind == ESRFacilityKind::Standard && OperationKind == ESRFacilityOperationKind::Synthesize",
		EditConditionHides))
	TObjectPtr<USRResourceDataAsset> SynthesisOutputResource = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility|Pattern", meta = (
		DisplayName = "SeparationOperator",
		EditCondition = "FacilityKind == ESRFacilityKind::Standard && OperationKind == ESRFacilityOperationKind::Separate",
		EditConditionHides))
	FSRPatternSeparationOperatorSpec SeparationOperator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility|Inventory", meta = (DisplayName = "InputInventory"))
	FSRFacilityInventorySpec InputInventory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility|Inventory", meta = (DisplayName = "OutputInventory"))
	FSRFacilityInventorySpec OutputInventory;
};
