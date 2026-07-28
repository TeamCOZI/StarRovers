#pragma once

#include "CoreMinimal.h"
#include "SRNaturalStructureSpawnTypes.generated.h"

class USRStructureDataAsset;

USTRUCT(BlueprintType)
struct STARROVERS_API FSRProfileNaturalStructureSpawnRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Natural Structures", meta = (DisplayName = "RuleId"))
	FName RuleId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Natural Structures", meta = (DisplayName = "bEnabled"))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Natural Structures", meta = (DisplayName = "StructureDataAsset"))
	TObjectPtr<USRStructureDataAsset> StructureDataAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Natural Structures", meta = (DisplayName = "SpawnChancePerCell", ClampMin = "0.0", ClampMax = "1.0"))
	float SpawnChancePerCell = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Natural Structures", meta = (DisplayName = "MaxCount", ClampMin = "0"))
	int32 MaxCount = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Natural Structures", meta = (DisplayName = "MinimumGuaranteedCount", ClampMin = "0", ToolTip = "Ignores the random spawn roll after the normal pass until this many structures have been placed, while still respecting valid cells and spacing. Zero disables the guarantee."))
	int32 MinimumGuaranteedCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Natural Structures", meta = (DisplayName = "MinCellSpacing", ClampMin = "0"))
	int32 MinCellSpacing = 2;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRNaturalStructureSpawnRuleOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Natural Structures", meta = (DisplayName = "RuleId"))
	FName RuleId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Natural Structures", meta = (DisplayName = "bEnabled"))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Natural Structures", meta = (DisplayName = "SpawnChancePerCell", ClampMin = "0.0", ClampMax = "1.0"))
	float SpawnChancePerCell = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Natural Structures", meta = (DisplayName = "MaxCount", ClampMin = "0"))
	int32 MaxCount = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Natural Structures", meta = (DisplayName = "MinimumGuaranteedCount", ClampMin = "0", ToolTip = "Overrides the profile minimum guarantee. The value is clamped to MaxCount when MaxCount is non-zero."))
	int32 MinimumGuaranteedCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Natural Structures", meta = (DisplayName = "MinCellSpacing", ClampMin = "0"))
	int32 MinCellSpacing = 2;
};
