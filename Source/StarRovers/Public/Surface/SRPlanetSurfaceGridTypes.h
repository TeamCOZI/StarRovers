#pragma once

#include "CoreMinimal.h"
#include "SRPlanetSurfaceGridTypes.generated.h"

UENUM(BlueprintType)
enum class ESRCubeSphereFace : uint8
{
    PositiveX UMETA(DisplayName = "+X"),
    NegativeX UMETA(DisplayName = "-X"),
    PositiveY UMETA(DisplayName = "+Y"),
    NegativeY UMETA(DisplayName = "-Y"),
    PositiveZ UMETA(DisplayName = "+Z"),
    NegativeZ UMETA(DisplayName = "-Z"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPlanetSurfaceGridCellId
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface")
    ESRCubeSphereFace Face = ESRCubeSphereFace::PositiveX;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (ClampMin = "0"))
    int32 CellX = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (ClampMin = "0"))
    int32 CellY = 0;

    bool operator==(const FSRPlanetSurfaceGridCellId& Other) const
    {
        return Face == Other.Face && CellX == Other.CellX && CellY == Other.CellY;
    }

    bool IsValid(int32 Resolution) const
    {
        return Resolution > 0
            && CellX >= 0
            && CellY >= 0
            && CellX < Resolution
            && CellY < Resolution;
    }
};

FORCEINLINE uint32 GetTypeHash(const FSRPlanetSurfaceGridCellId& CellId)
{
    return HashCombine(
        HashCombine(::GetTypeHash(static_cast<uint8>(CellId.Face)), ::GetTypeHash(CellId.CellX)),
        ::GetTypeHash(CellId.CellY));
}

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPlanetSurfaceGridCellNeighbors
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface")
    FSRPlanetSurfaceGridCellId NegativeU;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface")
    FSRPlanetSurfaceGridCellId PositiveU;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface")
    FSRPlanetSurfaceGridCellId NegativeV;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface")
    FSRPlanetSurfaceGridCellId PositiveV;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPlanetSurfaceGridCell
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface")
    FSRPlanetSurfaceGridCellId CellId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface")
    FVector LocalCenter = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface")
    FVector LocalNormal = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface")
    FVector Corner00 = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface")
    FVector Corner10 = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface")
    FVector Corner11 = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface")
    FVector Corner01 = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface")
    FVector2D FaceUVMin = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface")
    FVector2D FaceUVMax = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface")
    float ApproxSurfaceArea = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface")
    FSRPlanetSurfaceGridCellNeighbors Neighbors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface")
    bool bOccupied = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface")
    FName OccupantId = NAME_None;
};
