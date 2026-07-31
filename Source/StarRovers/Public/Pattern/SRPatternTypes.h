#pragma once

#include "CoreMinimal.h"
#include "SRPatternTypes.generated.h"

UENUM(BlueprintType)
enum class ESRGlyphType : uint8
{
	Empty = 0 UMETA(DisplayName = "Empty"),
	Metal = 1 UMETA(DisplayName = "Metal"),
	Organic = 2 UMETA(DisplayName = "Organic"),
	Crystal = 3 UMETA(DisplayName = "Crystal"),
	Fluid = 4 UMETA(DisplayName = "Fluid"),
	Plasma = 5 UMETA(DisplayName = "Plasma"),
};

UENUM(BlueprintType)
enum class ESRPatternDirection : uint8
{
	Up = 0 UMETA(DisplayName = "Up"),
	Right = 1 UMETA(DisplayName = "Right"),
	Down = 2 UMETA(DisplayName = "Down"),
	Left = 3 UMETA(DisplayName = "Left"),
};

namespace StarRovers::Pattern
{
	inline constexpr int32 GridSize = 5;
	inline constexpr int32 CellCount = GridSize * GridSize;

	STARROVERS_API bool IsValidGlyph(ESRGlyphType Glyph);
	STARROVERS_API bool IsValidDirection(ESRPatternDirection Direction);
	STARROVERS_API bool IsValidCoordinate(int32 Row, int32 Column);
	STARROVERS_API bool TryCoordinateToIndex(int32 Row, int32 Column, int32& OutIndex);
	STARROVERS_API bool TryIndexToCoordinate(int32 Index, int32& OutRow, int32& OutColumn);
	STARROVERS_API bool TryGetDirectionDelta(
		ESRPatternDirection Direction,
		int32& OutRowDelta,
		int32& OutColumnDelta);
}

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPattern
{
	GENERATED_BODY()

	FSRPattern();
	explicit FSRPattern(ESRGlyphType FillGlyph);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Pattern", meta = (DisplayName = "Cells", EditFixedSize))
	TArray<ESRGlyphType> Cells;

	bool IsCanonical() const;
	bool Normalize();
	void Reset(ESRGlyphType FillGlyph = ESRGlyphType::Empty);

	bool TryGetGlyph(int32 Row, int32 Column, ESRGlyphType& OutGlyph) const;
	ESRGlyphType GetGlyph(int32 Row, int32 Column) const;
	bool SetGlyph(int32 Row, int32 Column, ESRGlyphType Glyph);

	int32 CountGlyph(ESRGlyphType Glyph) const;
	int32 GetOccupiedCellCount() const;
	bool IsEmpty() const;
	uint32 GetStableHash() const;
	FString ToCompactString() const;

	bool operator==(const FSRPattern& Other) const;
	bool operator!=(const FSRPattern& Other) const;
};

STARROVERS_API uint32 GetTypeHash(const FSRPattern& Pattern);

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPatternMask
{
	GENERATED_BODY()

	FSRPatternMask();
	explicit FSRPatternMask(bool bFillActive);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Pattern", meta = (DisplayName = "ActiveCells", EditFixedSize))
	TArray<bool> ActiveCells;

	bool IsCanonical() const;
	bool Normalize();
	void Reset(bool bFillActive = false);

	bool IsCellActive(int32 Row, int32 Column) const;
	bool SetCellActive(int32 Row, int32 Column, bool bActive);
	int32 GetActiveCellCount() const;
	bool HasAnyActiveCell() const;
	bool AreAllCellsActive() const;
	uint32 GetStableHash() const;

	bool operator==(const FSRPatternMask& Other) const;
	bool operator!=(const FSRPatternMask& Other) const;
};

STARROVERS_API uint32 GetTypeHash(const FSRPatternMask& PatternMask);
