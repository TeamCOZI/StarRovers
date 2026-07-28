#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRStructureInstanceSaveData.generated.h"

class USRResourceDataAsset;
class USRStructureDataAsset;

/**
 * The persistence kind is explicit from schema 2 onward.  MAX_int32 remains
 * the runtime sentinel, but it is no longer the only way a save distinguishes
 * a Legacy infinite deposit from a Resource V2 finite deposit.
 */
UENUM(BlueprintType)
enum class ESRResourceDepositPersistenceKind : uint8
{
	Finite UMETA(DisplayName = "Finite"),
	LegacyInfinite UMETA(DisplayName = "Legacy Infinite"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPlacedStructureSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Save")
	FName OccupantId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Save")
	FName StructureId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Save")
	TSoftObjectPtr<USRStructureDataAsset> StructureDataAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Save")
	FSRPlanetSurfaceGridCellId OriginCellId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Save")
	TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Save")
	int32 PlacementRotationSteps = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Save")
	bool bNaturalStructure = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Save")
	bool bUseStaticMeshMaterials = false;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceDepositSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource Deposit|Save")
	FName OccupantId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource Deposit|Save")
	FName StructureId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource Deposit|Save")
	FName ResourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource Deposit|Save")
	TSoftObjectPtr<USRResourceDataAsset> ResourceDataAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource Deposit|Save")
	ESRResourceDepositPersistenceKind PersistenceKind =
		ESRResourceDepositPersistenceKind::Finite;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource Deposit|Save")
	int32 TotalAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource Deposit|Save")
	int32 RemainingAmount = 0;
};

/** Versioned snapshot owned by one celestial body's Structure Manager. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRStructureInstanceManagerSaveData
{
	GENERATED_BODY()

	static constexpr int32 InitialVersion = 1;
	static constexpr int32 FiniteResourceEconomyVersion = 2;
	static constexpr int32 CurrentVersion = FiniteResourceEconomyVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Save")
	int32 Version = CurrentVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Save")
	int32 NextStructureInstanceSequence = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Save")
	TArray<FSRPlacedStructureSaveData> PlacedStructures;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Structure|Save")
	TArray<FSRResourceDepositSaveData> ResourceDeposits;

	bool IsSupportedVersion() const
	{
		return Version >= InitialVersion && Version <= CurrentVersion;
	}
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStructureSaveImportReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure|Save")
	int32 SourceVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure|Save")
	int32 RestoredStructureCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure|Save")
	int32 RestoredDepositCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure|Save")
	int32 MigratedLegacyPlaceholderCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Structure|Save")
	int32 MigratedLegacyInfiniteToFiniteCount = 0;
};

/** Pure amount migration shared by the runtime importer and automation tests. */
class STARROVERS_API FSRResourceDepositSaveMigration final
{
public:
	static constexpr int32 LegacyPlaceholderTotalAmount = 50000;

	static bool MigrateAmount(
		int32 SourceVersion,
		const FSRResourceDepositSaveData& SavedDeposit,
		bool bRuntimeResourceV2,
		int32 RuntimeAuthoredTotalAmount,
		FSRResourceDepositSaveData& OutMigratedDeposit,
		bool& bOutMigratedLegacyPlaceholder,
		bool& bOutMigratedLegacyInfiniteToFinite,
		FString* OutFailureReason = nullptr);
};
