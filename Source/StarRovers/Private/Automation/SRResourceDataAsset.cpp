#include "Automation/SRResourceDataAsset.h"

#include "Misc/Crc.h"
#include "Pattern/SRPatternRoutingFilter.h"

namespace
{
	uint32 HashNameStable(FName Name)
	{
		return Name.IsNone() ? 0u : FCrc::StrCrc32(*Name.ToString());
	}
}

bool StarRovers::ResourcePatterns::TryGenerateSourcePattern(
	TConstArrayView<FSRSourceGlyphCount> GlyphCounts,
	int32 PatternSeed,
	FSRPattern& OutPattern)
{
	OutPattern.Reset();
	if (GlyphCounts.IsEmpty())
	{
		return false;
	}

	TArray<FSRSourceGlyphCount> CanonicalGlyphCounts;
	CanonicalGlyphCounts.Reserve(GlyphCounts.Num());
	TSet<ESRGlyphType> SeenGlyphs;
	int32 OccupiedCellCount = 0;
	for (const FSRSourceGlyphCount& GlyphCount : GlyphCounts)
	{
		if (GlyphCount.Glyph == ESRGlyphType::Empty
			|| !StarRovers::Pattern::IsValidGlyph(GlyphCount.Glyph)
			|| GlyphCount.Count <= 0
			|| SeenGlyphs.Contains(GlyphCount.Glyph))
		{
			return false;
		}

		SeenGlyphs.Add(GlyphCount.Glyph);
		OccupiedCellCount += GlyphCount.Count;
		if (OccupiedCellCount > StarRovers::Pattern::CellCount)
		{
			return false;
		}
		CanonicalGlyphCounts.Add(GlyphCount);
	}

	CanonicalGlyphCounts.Sort([](const FSRSourceGlyphCount& Left, const FSRSourceGlyphCount& Right)
	{
		return static_cast<uint8>(Left.Glyph) < static_cast<uint8>(Right.Glyph);
	});

	TArray<int32> ShuffledCellIndices;
	ShuffledCellIndices.Reserve(StarRovers::Pattern::CellCount);
	for (int32 CellIndex = 0; CellIndex < StarRovers::Pattern::CellCount; ++CellIndex)
	{
		ShuffledCellIndices.Add(CellIndex);
	}

	FRandomStream RandomStream(PatternSeed);
	for (int32 CellIndex = ShuffledCellIndices.Num() - 1; CellIndex > 0; --CellIndex)
	{
		const int32 SwapIndex = RandomStream.RandRange(0, CellIndex);
		ShuffledCellIndices.Swap(CellIndex, SwapIndex);
	}

	int32 ShuffledIndex = 0;
	for (const FSRSourceGlyphCount& GlyphCount : CanonicalGlyphCounts)
	{
		for (int32 GlyphIndex = 0; GlyphIndex < GlyphCount.Count; ++GlyphIndex)
		{
			OutPattern.Cells[ShuffledCellIndices[ShuffledIndex++]] = GlyphCount.Glyph;
		}
	}

	return OutPattern.IsCanonical()
		&& OutPattern.GetOccupiedCellCount() == OccupiedCellCount;
}

int32 StarRovers::ResourcePatterns::MakeStableSourcePatternSeed(
	FName SourcePatternId,
	FName ResourceId,
	int32 SeedSalt)
{
	uint32 Hash = HashCombineFast(HashNameStable(SourcePatternId), HashNameStable(ResourceId));
	Hash = HashCombineFast(Hash, GetTypeHash(SeedSalt));
	return static_cast<int32>(Hash & static_cast<uint32>(MAX_int32));
}

bool StarRovers::ResourcePatterns::ArePatternPayloadsEquivalent(
	const FSRResourceInstance& Left,
	const FSRResourceInstance& Right)
{
	return StarRovers::PatternRouting::IsValidPatternPayload(Left)
		&& StarRovers::PatternRouting::IsValidPatternPayload(Right)
		&& Left.ResourceId == Right.ResourceId
		&& Left.Pattern == Right.Pattern;
}

USRResourceDataAsset::USRResourceDataAsset()
{
	ResourceId = FName(TEXT("Resource"));
	DisplayName = NSLOCTEXT("StarRoversResource", "DefaultResourceDisplayName", "Resource");
	Description = NSLOCTEXT("StarRoversResource", "DefaultResourceDescription", "Automation resource.");

	FSRSourceGlyphCount MetalCount;
	MetalCount.Glyph = ESRGlyphType::Metal;
	MetalCount.Count = 2;
	SourceGlyphCounts.Add(MetalCount);

	FSRSourceGlyphCount CrystalCount;
	CrystalCount.Glyph = ESRGlyphType::Crystal;
	CrystalCount.Count = 2;
	SourceGlyphCounts.Add(CrystalCount);

}

bool USRResourceDataAsset::TryGenerateSourcePattern(int32 PatternSeed, FSRPattern& OutPattern) const
{
	return StarRovers::ResourcePatterns::TryGenerateSourcePattern(
		SourceGlyphCounts,
		PatternSeed,
		OutPattern);
}

FSRResourceInstance USRResourceDataAsset::BuildSourceInstance(
	int32 PatternSeed,
	FName SourcePatternId) const
{
	FSRPattern SourcePattern;
	if (!TryGenerateSourcePattern(PatternSeed, SourcePattern))
	{
		return FSRResourceInstance();
	}

	return BuildInstanceFromPattern(SourcePattern, PatternSeed, SourcePatternId);
}

FSRResourceInstance USRResourceDataAsset::BuildInstanceFromPattern(
	const FSRPattern& SourcePattern,
	int32 PatternSeed,
	FName SourcePatternId) const
{
	FSRResourceInstance Result;
	Result.ResourceDataAsset = const_cast<USRResourceDataAsset*>(this);
	Result.ResourceId = ResourceId;
	Result.Pattern = SourcePattern;
	Result.Pattern.Normalize();
	Result.SourcePatternId = SourcePatternId;
	Result.SourcePatternSeed = PatternSeed;

	Result.StackCount = 1;
	return Result;
}

FSRResourceInstance USRResourceDataAsset::BuildDefaultInstance() const
{
	const FName PreviewSourceId(*FString::Printf(TEXT("%s_Preview"), *ResourceId.ToString()));
	const int32 PreviewSeedSalt = static_cast<int32>(HashCombineFast(
		GetTypeHash(DefaultPreviewPatternSeed),
		GetTypeHash(SourcePatternSeedSalt)) & static_cast<uint32>(MAX_int32));
	const int32 PreviewSeed = StarRovers::ResourcePatterns::MakeStableSourcePatternSeed(
		PreviewSourceId,
		ResourceId,
		PreviewSeedSalt);
	return BuildSourceInstance(PreviewSeed, PreviewSourceId);
}
