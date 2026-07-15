#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SRResourceDataAsset.generated.h"

class USRResourceDataAsset;

UENUM(BlueprintType)
enum class ESRResourceProcessTag : uint8
{
	Responsive = 0 UMETA(DisplayName = "HeatResponsive"),
	Waste = 1 UMETA(DisplayName = "DeprecatedWaste"),
	HalfLife = 2 UMETA(DisplayName = "HalfLife"),
	Volatile = 3 UMETA(DisplayName = "Volatile"),
	Singularity = 4 UMETA(DisplayName = "Singularity"),
	Supercooled = 5 UMETA(DisplayName = "Supercooled"),
	HighActivity = 6 UMETA(DisplayName = "HighActivity"),
	Charge = 7 UMETA(DisplayName = "Charge"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceTagStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "Tag"))
	ESRResourceProcessTag Tag = ESRResourceProcessTag::Responsive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "StackCount", ClampMin = "1"))
	int32 StackCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "RemainingCycles", ClampMin = "0"))
	int32 RemainingCycles = 0;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceInstance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "ResourceInstanceId"))
	FName ResourceInstanceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "ResourceDataAsset"))
	TObjectPtr<USRResourceDataAsset> ResourceDataAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "ResourceId"))
	FName ResourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "EnergyValue"))
	double EnergyValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "RemainingProcessLimit", ClampMin = "0"))
	int32 RemainingProcessLimit = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "ProcessCount", ClampMin = "0"))
	int32 ProcessCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "Tags"))
	TArray<FSRResourceTagStack> Tags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "StackCount", ClampMin = "1"))
	int32 StackCount = 1;
};

UCLASS(BlueprintType)
class STARROVERS_API USRResourceDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	USRResourceDataAsset();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Resource")
	FSRResourceInstance BuildDefaultInstance() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "ResourceId"))
	FName ResourceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "Description"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource", meta = (DisplayName = "BaseEnergyValue"))
	double BaseEnergyValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource", meta = (DisplayName = "BaseProcessLimit", ClampMin = "0"))
	int32 BaseProcessLimit = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource", meta = (DisplayName = "DefaultTags"))
	TArray<FSRResourceTagStack> DefaultTags;
};
