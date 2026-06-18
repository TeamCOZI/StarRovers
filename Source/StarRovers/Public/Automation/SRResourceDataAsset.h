#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SRResourceDataAsset.generated.h"

class USRResourceDataAsset;

UENUM(BlueprintType)
enum class ESRResourceKind : uint8
{
	Energy UMETA(DisplayName = "Energy"),
	Catalyst UMETA(DisplayName = "Catalyst"),
};

UENUM(BlueprintType)
enum class ESRResourceCatalystOperator : uint8
{
	None UMETA(DisplayName = "None"),
	Add UMETA(DisplayName = "Add"),
	Multiply UMETA(DisplayName = "Multiply"),
	Subtract UMETA(DisplayName = "Subtract"),
};

UENUM(BlueprintType)
enum class ESRResourceProcessTag : uint8
{
	Responsive UMETA(DisplayName = "Responsive"),
	Waste UMETA(DisplayName = "Waste"),
	HalfLife UMETA(DisplayName = "HalfLife"),
	Volatile UMETA(DisplayName = "Volatile"),
	Singularity UMETA(DisplayName = "Singularity"),
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "ResourceKind"))
	ESRResourceKind ResourceKind = ESRResourceKind::Energy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "EnergyValue"))
	double EnergyValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "CatalystOperator"))
	ESRResourceCatalystOperator CatalystOperator = ESRResourceCatalystOperator::None;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource", meta = (DisplayName = "ResourceKind"))
	ESRResourceKind ResourceKind = ESRResourceKind::Energy;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource", meta = (DisplayName = "BaseEnergyValue"))
	double BaseEnergyValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource", meta = (DisplayName = "CatalystOperator"))
	ESRResourceCatalystOperator CatalystOperator = ESRResourceCatalystOperator::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource", meta = (DisplayName = "BaseProcessLimit", ClampMin = "0"))
	int32 BaseProcessLimit = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource", meta = (DisplayName = "DefaultTags"))
	TArray<FSRResourceTagStack> DefaultTags;
};
