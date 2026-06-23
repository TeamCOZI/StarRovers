#pragma once

#include "CoreMinimal.h"
#include "Automation/SRResourceDataAsset.h"
#include "Engine/DataAsset.h"
#include "SRFacilityDataAsset.generated.h"

UENUM(BlueprintType)
enum class ESRFacilityRarity : uint8
{
	Basic UMETA(DisplayName = "Basic"),
	Advanced UMETA(DisplayName = "Advanced"),
	HighTech UMETA(DisplayName = "HighTech"),
	Innovation UMETA(DisplayName = "Innovation"),
};

UENUM(BlueprintType)
enum class ESRFacilityOperationKind : uint8
{
	Process UMETA(DisplayName = "Process"),
	Synthesize UMETA(DisplayName = "Synthesize"),
	Split UMETA(DisplayName = "Split"),
	Mine UMETA(DisplayName = "Mine"),
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

UENUM(BlueprintType)
enum class ESRFacilityEffectKind : uint8
{
	AddEnergy UMETA(DisplayName = "AddEnergy"),
	MultiplyEnergy UMETA(DisplayName = "MultiplyEnergy"),
	AddProcessLimit UMETA(DisplayName = "AddProcessLimit"),
	AddTag UMETA(DisplayName = "AddTag"),
	RemoveTag UMETA(DisplayName = "RemoveTag"),
	ProduceResource UMETA(DisplayName = "ProduceResource"),
	SubtractEnergy UMETA(DisplayName = "SubtractEnergy"),
	DivideEnergy UMETA(DisplayName = "DivideEnergy"),
	SubtractProcessLimit UMETA(DisplayName = "SubtractProcessLimit"),
	MultiplyProcessLimit UMETA(DisplayName = "MultiplyProcessLimit"),
	DivideProcessLimit UMETA(DisplayName = "DivideProcessLimit"),
	AddCellTemperature UMETA(DisplayName = "AddCellTemperature"),
	SubtractCellTemperature UMETA(DisplayName = "SubtractCellTemperature"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFacilityEffectSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (DisplayName = "EffectKind"))
	ESRFacilityEffectKind EffectKind = ESRFacilityEffectKind::AddEnergy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (DisplayName = "Value"))
	double Value = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (DisplayName = "Count", ClampMin = "1"))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (DisplayName = "ResourceTag"))
	ESRResourceProcessTag ResourceTag = ESRResourceProcessTag::Responsive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (DisplayName = "ProducedResource"))
	TObjectPtr<USRResourceDataAsset> ProducedResource = nullptr;
};

UCLASS(BlueprintType)
class STARROVERS_API USRFacilityDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	USRFacilityDataAsset();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "FacilityId"))
	FName FacilityId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "Description"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "Rarity"))
	ESRFacilityRarity Rarity = ESRFacilityRarity::Basic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "OperationKind"))
	ESRFacilityOperationKind OperationKind = ESRFacilityOperationKind::Process;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "BaseProcessSeconds", ClampMin = "0.01"))
	float BaseProcessSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "InputCapacity", ClampMin = "1"))
	int32 InputCapacity = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "OutputCapacity", ClampMin = "1"))
	int32 OutputCapacity = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "bRequiresColdTemperature"))
	bool bRequiresColdTemperature = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "bRequiresHotTemperature"))
	bool bRequiresHotTemperature = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "Effects"))
	TArray<FSRFacilityEffectSpec> Effects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "DefaultOutputResource"))
	TObjectPtr<USRResourceDataAsset> DefaultOutputResource = nullptr;
};
