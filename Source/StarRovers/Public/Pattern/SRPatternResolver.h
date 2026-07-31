#pragma once

#include "CoreMinimal.h"
#include "Pattern/SRPatternTypes.h"
#include "SRPatternResolver.generated.h"

UENUM(BlueprintType)
enum class ESRPatternFluidSidePreference : uint8
{
	ClockwiseFirst = 0 UMETA(DisplayName = "ClockwiseFirst"),
	CounterClockwiseFirst = 1 UMETA(DisplayName = "CounterClockwiseFirst"),
};

UENUM(BlueprintType)
enum class ESRGlyphCollisionOutcome : uint8
{
	None = 0 UMETA(DisplayName = "None"),
	SameGlyphBlocked = 1 UMETA(DisplayName = "SameGlyphBlocked"),
	MoverWins = 2 UMETA(DisplayName = "MoverWins"),
	MoverBlocked = 3 UMETA(DisplayName = "MoverBlocked"),
	MoverDestroyed = 4 UMETA(DisplayName = "MoverDestroyed"),
};

UENUM(BlueprintType)
enum class ESRPatternTraceEventKind : uint8
{
	Move = 0 UMETA(DisplayName = "Move"),
	Collision = 1 UMETA(DisplayName = "Collision"),
	Blocked = 2 UMETA(DisplayName = "Blocked"),
	Ejected = 3 UMETA(DisplayName = "Ejected"),
	BoundaryStop = 4 UMETA(DisplayName = "BoundaryStop"),
	FluidDetour = 5 UMETA(DisplayName = "FluidDetour"),
	OrganicGrowth = 6 UMETA(DisplayName = "OrganicGrowth"),
	Separation = 7 UMETA(DisplayName = "Separation"),
};

UENUM(BlueprintType)
enum class ESRPatternResolveFailure : uint8
{
	None = 0 UMETA(DisplayName = "None"),
	InvalidInputPattern = 1 UMETA(DisplayName = "InvalidInputPattern"),
	InvalidSelectionMask = 2 UMETA(DisplayName = "InvalidSelectionMask"),
	InvalidDirection = 3 UMETA(DisplayName = "InvalidDirection"),
	InvalidDistance = 4 UMETA(DisplayName = "InvalidDistance"),
	InvalidOrganicGrowthCount = 5 UMETA(DisplayName = "InvalidOrganicGrowthCount"),
	InvalidFluidSidePreference = 6 UMETA(DisplayName = "InvalidFluidSidePreference"),
	InvalidMoveCommandCount = 7 UMETA(DisplayName = "InvalidMoveCommandCount"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPatternMoveCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern", meta = (DisplayName = "SelectionMask"))
	FSRPatternMask SelectionMask;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern", meta = (DisplayName = "Direction"))
	ESRPatternDirection Direction = ESRPatternDirection::Right;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern", meta = (DisplayName = "Distance", ClampMin = "1", ClampMax = "25"))
	int32 Distance = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern", meta = (DisplayName = "FluidSidePreference"))
	ESRPatternFluidSidePreference FluidSidePreference = ESRPatternFluidSidePreference::ClockwiseFirst;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPatternCycleRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern", meta = (DisplayName = "MoveCommands"))
	TArray<FSRPatternMoveCommand> MoveCommands;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern", meta = (DisplayName = "OrganicGrowthsPerComponent", ClampMin = "0", ClampMax = "4"))
	int32 OrganicGrowthsPerComponent = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern", meta = (DisplayName = "OrganicGrowthPriority"))
	ESRPatternDirection OrganicGrowthPriority = ESRPatternDirection::Right;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPatternTraceEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern", meta = (DisplayName = "Sequence"))
	int32 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern", meta = (DisplayName = "CommandIndex"))
	int32 CommandIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern", meta = (DisplayName = "OutputIndex"))
	int32 OutputIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern", meta = (DisplayName = "EnvironmentEffectIndex"))
	int32 EnvironmentEffectIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern", meta = (DisplayName = "EventKind"))
	ESRPatternTraceEventKind EventKind = ESRPatternTraceEventKind::Move;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern", meta = (DisplayName = "Glyph"))
	ESRGlyphType Glyph = ESRGlyphType::Empty;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern", meta = (DisplayName = "OtherGlyph"))
	ESRGlyphType OtherGlyph = ESRGlyphType::Empty;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern", meta = (DisplayName = "FromRow"))
	int32 FromRow = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern", meta = (DisplayName = "FromColumn"))
	int32 FromColumn = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern", meta = (DisplayName = "ToRow"))
	int32 ToRow = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern", meta = (DisplayName = "ToColumn"))
	int32 ToColumn = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern", meta = (DisplayName = "CollisionOutcome"))
	ESRGlyphCollisionOutcome CollisionOutcome = ESRGlyphCollisionOutcome::None;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPatternResolveResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern", meta = (DisplayName = "Succeeded"))
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern", meta = (DisplayName = "Failure"))
	ESRPatternResolveFailure Failure = ESRPatternResolveFailure::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern", meta = (DisplayName = "FailedCommandIndex"))
	int32 FailedCommandIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern", meta = (DisplayName = "OutputPattern"))
	FSRPattern OutputPattern;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern", meta = (DisplayName = "TraceEvents"))
	TArray<FSRPatternTraceEvent> TraceEvents;

	// Internal execution context used while trace events are appended. It is not serialized or exposed to UI.
	int32 ActiveTraceCommandIndex = INDEX_NONE;
};

class STARROVERS_API FSRPatternResolver final
{
public:
	static constexpr int32 MaxCommandDistance = StarRovers::Pattern::CellCount;
	static constexpr int32 MaxOrganicGrowthsPerComponent = 4;
	static constexpr int32 MaxMoveCommandsPerCycle = 8;

	static FSRPatternResolveResult ResolveCycle(
		const FSRPattern& InputPattern,
		const FSRPatternCycleRequest& Request);

	static FSRPatternResolveResult ResolveMoveCycle(
		const FSRPattern& InputPattern,
		const FSRPatternMoveCommand& Command,
		int32 OrganicGrowthsPerComponent = 1);

	static ESRGlyphCollisionOutcome ResolveCollision(
		ESRGlyphType MovingGlyph,
		ESRGlyphType DefendingGlyph);

	static bool DoesGlyphDefeat(ESRGlyphType AttackingGlyph, ESRGlyphType DefendingGlyph);
};
