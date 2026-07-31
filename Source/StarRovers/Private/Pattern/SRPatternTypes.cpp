#include "Pattern/SRPatternTypes.h"

namespace
{
	constexpr uint32 FnvOffsetBasis = 2166136261u;
	constexpr uint32 FnvPrime = 16777619u;

	uint32 AppendStableHashByte(uint32 Hash, uint8 Value)
	{
		return (Hash ^ static_cast<uint32>(Value)) * FnvPrime;
	}

	TCHAR GetGlyphCompactCharacter(ESRGlyphType Glyph)
	{
		switch (Glyph)
		{
		case ESRGlyphType::Empty:
			return TEXT('.');
		case ESRGlyphType::Metal:
			return TEXT('M');
		case ESRGlyphType::Organic:
			return TEXT('O');
		case ESRGlyphType::Crystal:
			return TEXT('C');
		case ESRGlyphType::Fluid:
			return TEXT('F');
		case ESRGlyphType::Plasma:
			return TEXT('P');
		default:
			return TEXT('?');
		}
	}
}

bool StarRovers::Pattern::IsValidGlyph(ESRGlyphType Glyph)
{
	switch (Glyph)
	{
	case ESRGlyphType::Empty:
	case ESRGlyphType::Metal:
	case ESRGlyphType::Organic:
	case ESRGlyphType::Crystal:
	case ESRGlyphType::Fluid:
	case ESRGlyphType::Plasma:
		return true;
	default:
		return false;
	}
}

bool StarRovers::Pattern::IsValidDirection(ESRPatternDirection Direction)
{
	switch (Direction)
	{
	case ESRPatternDirection::Up:
	case ESRPatternDirection::Right:
	case ESRPatternDirection::Down:
	case ESRPatternDirection::Left:
		return true;
	default:
		return false;
	}
}

bool StarRovers::Pattern::IsValidCoordinate(int32 Row, int32 Column)
{
	return Row >= 0 && Row < GridSize && Column >= 0 && Column < GridSize;
}

bool StarRovers::Pattern::TryCoordinateToIndex(int32 Row, int32 Column, int32& OutIndex)
{
	OutIndex = INDEX_NONE;
	if (!IsValidCoordinate(Row, Column))
	{
		return false;
	}

	OutIndex = Row * GridSize + Column;
	return true;
}

bool StarRovers::Pattern::TryIndexToCoordinate(int32 Index, int32& OutRow, int32& OutColumn)
{
	OutRow = INDEX_NONE;
	OutColumn = INDEX_NONE;
	if (Index < 0 || Index >= CellCount)
	{
		return false;
	}

	OutRow = Index / GridSize;
	OutColumn = Index % GridSize;
	return true;
}

bool StarRovers::Pattern::TryGetDirectionDelta(
	ESRPatternDirection Direction,
	int32& OutRowDelta,
	int32& OutColumnDelta)
{
	OutRowDelta = 0;
	OutColumnDelta = 0;

	switch (Direction)
	{
	case ESRPatternDirection::Up:
		OutRowDelta = -1;
		return true;
	case ESRPatternDirection::Right:
		OutColumnDelta = 1;
		return true;
	case ESRPatternDirection::Down:
		OutRowDelta = 1;
		return true;
	case ESRPatternDirection::Left:
		OutColumnDelta = -1;
		return true;
	default:
		return false;
	}
}

FSRPattern::FSRPattern()
{
	Reset();
}

FSRPattern::FSRPattern(ESRGlyphType FillGlyph)
{
	Reset(FillGlyph);
}

bool FSRPattern::IsCanonical() const
{
	if (Cells.Num() != StarRovers::Pattern::CellCount)
	{
		return false;
	}

	for (const ESRGlyphType Glyph : Cells)
	{
		if (!StarRovers::Pattern::IsValidGlyph(Glyph))
		{
			return false;
		}
	}

	return true;
}

bool FSRPattern::Normalize()
{
	bool bChanged = Cells.Num() != StarRovers::Pattern::CellCount;
	Cells.SetNum(StarRovers::Pattern::CellCount);

	for (ESRGlyphType& Glyph : Cells)
	{
		if (!StarRovers::Pattern::IsValidGlyph(Glyph))
		{
			Glyph = ESRGlyphType::Empty;
			bChanged = true;
		}
	}

	return bChanged;
}

void FSRPattern::Reset(ESRGlyphType FillGlyph)
{
	const ESRGlyphType SafeFillGlyph = StarRovers::Pattern::IsValidGlyph(FillGlyph)
		? FillGlyph
		: ESRGlyphType::Empty;
	Cells.Init(SafeFillGlyph, StarRovers::Pattern::CellCount);
}

bool FSRPattern::TryGetGlyph(int32 Row, int32 Column, ESRGlyphType& OutGlyph) const
{
	OutGlyph = ESRGlyphType::Empty;
	int32 Index = INDEX_NONE;
	if (!IsCanonical() || !StarRovers::Pattern::TryCoordinateToIndex(Row, Column, Index))
	{
		return false;
	}

	OutGlyph = Cells[Index];
	return true;
}

ESRGlyphType FSRPattern::GetGlyph(int32 Row, int32 Column) const
{
	ESRGlyphType Glyph = ESRGlyphType::Empty;
	TryGetGlyph(Row, Column, Glyph);
	return Glyph;
}

bool FSRPattern::SetGlyph(int32 Row, int32 Column, ESRGlyphType Glyph)
{
	int32 Index = INDEX_NONE;
	if (!StarRovers::Pattern::IsValidGlyph(Glyph)
		|| !StarRovers::Pattern::TryCoordinateToIndex(Row, Column, Index))
	{
		return false;
	}

	Normalize();
	Cells[Index] = Glyph;
	return true;
}

int32 FSRPattern::CountGlyph(ESRGlyphType Glyph) const
{
	if (!IsCanonical() || !StarRovers::Pattern::IsValidGlyph(Glyph))
	{
		return 0;
	}

	int32 Count = 0;
	for (const ESRGlyphType CellGlyph : Cells)
	{
		if (CellGlyph == Glyph)
		{
			++Count;
		}
	}
	return Count;
}

int32 FSRPattern::GetOccupiedCellCount() const
{
	return IsCanonical()
		? StarRovers::Pattern::CellCount - CountGlyph(ESRGlyphType::Empty)
		: 0;
}

bool FSRPattern::IsEmpty() const
{
	return IsCanonical() && GetOccupiedCellCount() == 0;
}

uint32 FSRPattern::GetStableHash() const
{
	uint32 Hash = FnvOffsetBasis;
	Hash = AppendStableHashByte(Hash, static_cast<uint8>(Cells.Num() & 0xff));
	for (const ESRGlyphType Glyph : Cells)
	{
		Hash = AppendStableHashByte(Hash, static_cast<uint8>(Glyph));
	}
	return Hash;
}

FString FSRPattern::ToCompactString() const
{
	if (!IsCanonical())
	{
		return FString::Printf(TEXT("InvalidPattern(%d)"), Cells.Num());
	}

	FString Result;
	Result.Reserve(StarRovers::Pattern::CellCount + StarRovers::Pattern::GridSize - 1);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (Index > 0 && Index % StarRovers::Pattern::GridSize == 0)
		{
			Result.AppendChar(TEXT('/'));
		}
		Result.AppendChar(GetGlyphCompactCharacter(Cells[Index]));
	}
	return Result;
}

bool FSRPattern::operator==(const FSRPattern& Other) const
{
	return Cells == Other.Cells;
}

bool FSRPattern::operator!=(const FSRPattern& Other) const
{
	return !(*this == Other);
}

uint32 GetTypeHash(const FSRPattern& Pattern)
{
	return Pattern.GetStableHash();
}

FSRPatternMask::FSRPatternMask()
{
	Reset();
}

FSRPatternMask::FSRPatternMask(bool bFillActive)
{
	Reset(bFillActive);
}

bool FSRPatternMask::IsCanonical() const
{
	return ActiveCells.Num() == StarRovers::Pattern::CellCount;
}

bool FSRPatternMask::Normalize()
{
	const bool bChanged = ActiveCells.Num() != StarRovers::Pattern::CellCount;
	ActiveCells.SetNum(StarRovers::Pattern::CellCount);
	return bChanged;
}

void FSRPatternMask::Reset(bool bFillActive)
{
	ActiveCells.Init(bFillActive, StarRovers::Pattern::CellCount);
}

bool FSRPatternMask::IsCellActive(int32 Row, int32 Column) const
{
	int32 Index = INDEX_NONE;
	return IsCanonical()
		&& StarRovers::Pattern::TryCoordinateToIndex(Row, Column, Index)
		&& ActiveCells[Index];
}

bool FSRPatternMask::SetCellActive(int32 Row, int32 Column, bool bActive)
{
	int32 Index = INDEX_NONE;
	if (!StarRovers::Pattern::TryCoordinateToIndex(Row, Column, Index))
	{
		return false;
	}

	Normalize();
	ActiveCells[Index] = bActive;
	return true;
}

int32 FSRPatternMask::GetActiveCellCount() const
{
	if (!IsCanonical())
	{
		return 0;
	}

	int32 Count = 0;
	for (const bool bActive : ActiveCells)
	{
		Count += bActive ? 1 : 0;
	}
	return Count;
}

bool FSRPatternMask::HasAnyActiveCell() const
{
	return GetActiveCellCount() > 0;
}

bool FSRPatternMask::AreAllCellsActive() const
{
	return IsCanonical() && GetActiveCellCount() == StarRovers::Pattern::CellCount;
}

uint32 FSRPatternMask::GetStableHash() const
{
	uint32 Hash = FnvOffsetBasis;
	Hash = AppendStableHashByte(Hash, static_cast<uint8>(ActiveCells.Num() & 0xff));
	for (const bool bActive : ActiveCells)
	{
		Hash = AppendStableHashByte(Hash, bActive ? 1u : 0u);
	}
	return Hash;
}

bool FSRPatternMask::operator==(const FSRPatternMask& Other) const
{
	return ActiveCells == Other.ActiveCells;
}

bool FSRPatternMask::operator!=(const FSRPatternMask& Other) const
{
	return !(*this == Other);
}

uint32 GetTypeHash(const FSRPatternMask& PatternMask)
{
	return PatternMask.GetStableHash();
}
