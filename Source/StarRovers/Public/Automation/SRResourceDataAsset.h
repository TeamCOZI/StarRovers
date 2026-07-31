#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Pattern/SRPatternTypes.h"
#include "SRResourceDataAsset.generated.h"

class USRResourceDataAsset;

USTRUCT(BlueprintType)
struct STARROVERS_API FSRSourceGlyphCount
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource|Source Pattern", meta = (DisplayName = "Glyph"))
	ESRGlyphType Glyph = ESRGlyphType::Metal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource|Source Pattern", meta = (DisplayName = "Count", ClampMin = "1", ClampMax = "25"))
	int32 Count = 1;
};

namespace StarRovers::ResourcePatterns
{
	STARROVERS_API bool TryGenerateSourcePattern(
		TConstArrayView<FSRSourceGlyphCount> GlyphCounts,
		int32 PatternSeed,
		FSRPattern& OutPattern);

	STARROVERS_API int32 MakeStableSourcePatternSeed(
		FName SourcePatternId,
		FName ResourceId,
		int32 SeedSalt = 0);
}

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource|Pattern", meta = (DisplayName = "Pattern"))
	FSRPattern Pattern;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource|Pattern", meta = (DisplayName = "SourcePatternId"))
	FName SourcePatternId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource|Pattern", meta = (DisplayName = "SourcePatternSeed"))
	int32 SourcePatternSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "StackCount", ClampMin = "1"))
	int32 StackCount = 1;
};

namespace StarRovers::ResourcePatterns
{
	STARROVERS_API bool ArePatternPayloadsEquivalent(
		const FSRResourceInstance& Left,
		const FSRResourceInstance& Right);
}

UCLASS(BlueprintType)
class STARROVERS_API USRResourceDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	USRResourceDataAsset();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Resource|Source Pattern")
	bool TryGenerateSourcePattern(int32 PatternSeed, FSRPattern& OutPattern) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Resource|Source Pattern")
	FSRResourceInstance BuildSourceInstance(int32 PatternSeed, FName SourcePatternId) const;

	FSRResourceInstance BuildInstanceFromPattern(
		const FSRPattern& SourcePattern,
		int32 PatternSeed,
		FName SourcePatternId) const;

	// Deterministic preview/debug entry point. Mining uses its deposit's stored Pattern instead.
	UFUNCTION(BlueprintPure, Category = "StarRovers|Resource")
	FSRResourceInstance BuildDefaultInstance() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "ResourceId"))
	FName ResourceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "Description"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource|Source Pattern", meta = (DisplayName = "SourceGlyphCounts", TitleProperty = "Glyph"))
	TArray<FSRSourceGlyphCount> SourceGlyphCounts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource|Source Pattern", meta = (DisplayName = "SourcePatternSeedSalt"))
	int32 SourcePatternSeedSalt = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource|Source Pattern", meta = (DisplayName = "DefaultPreviewPatternSeed"))
	int32 DefaultPreviewPatternSeed = 1337;

};
