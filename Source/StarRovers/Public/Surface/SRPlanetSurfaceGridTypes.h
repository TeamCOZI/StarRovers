#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityDataAsset.h"
#include "Surface/SRPlanetBiomeTypes.h"
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "Face"))
    ESRCubeSphereFace Face = ESRCubeSphereFace::PositiveX;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "CellX", ClampMin = "0"))
    int32 CellX = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "CellY", ClampMin = "0"))
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "NegativeU"))
    FSRPlanetSurfaceGridCellId NegativeU;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "PositiveU"))
    FSRPlanetSurfaceGridCellId PositiveU;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "NegativeV"))
    FSRPlanetSurfaceGridCellId NegativeV;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "PositiveV"))
    FSRPlanetSurfaceGridCellId PositiveV;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPlanetSurfaceGridLineSegment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "LocalPointA"))
    FVector LocalPointA = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "LocalPointB"))
    FVector LocalPointB = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "bHasAdjacentCell"))
    bool bHasAdjacentCell = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "AdjacentCellId"))
    FSRPlanetSurfaceGridCellId AdjacentCellId;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPlanetSurfaceGridSideFace
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "LocalPoint0"))
    FVector LocalPoint0 = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "LocalPoint1"))
    FVector LocalPoint1 = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "LocalPoint2"))
    FVector LocalPoint2 = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "LocalPoint3"))
    FVector LocalPoint3 = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "bHasAdjacentCell"))
    bool bHasAdjacentCell = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "AdjacentCellId"))
    FSRPlanetSurfaceGridCellId AdjacentCellId;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPlanetSurfaceGridCell
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "CellId"))
    FSRPlanetSurfaceGridCellId CellId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "LocalCenter"))
    FVector LocalCenter = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "LocalNormal"))
    FVector LocalNormal = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "Corner00"))
    FVector Corner00 = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "Corner10"))
    FVector Corner10 = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "Corner11"))
    FVector Corner11 = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "Corner01"))
    FVector Corner01 = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "FaceUVMin"))
    FVector2D FaceUVMin = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "FaceUVMax"))
    FVector2D FaceUVMax = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "ApproxSurfaceArea"))
    float ApproxSurfaceArea = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "Biome"))
    ESRPlanetBiome Biome = ESRPlanetBiome::Plains;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "BiomeId"))
    FName BiomeId = FName(TEXT("Plains"));

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "WaterRole"))
    ESRBiomeWaterRole WaterRole = ESRBiomeWaterRole::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface|Temperature", meta = (DisplayName = "SurfaceTemperature", ClampMin = "0.0", ClampMax = "1.0"))
    float SurfaceTemperature = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface|Temperature", meta = (DisplayName = "TemperatureState"))
    ESRFacilityTemperatureState TemperatureState = ESRFacilityTemperatureState::Normal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "Neighbors"))
    FSRPlanetSurfaceGridCellNeighbors Neighbors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "SideLineSegments"))
    TArray<FSRPlanetSurfaceGridLineSegment> SideLineSegments;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "SideFaces"))
    TArray<FSRPlanetSurfaceGridSideFace> SideFaces;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "bOccupied"))
    bool bOccupied = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "OccupantId"))
    FName OccupantId = NAME_None;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPlanetSurfaceGridCellInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "CellId"))
    FSRPlanetSurfaceGridCellId CellId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "FaceResolution"))
    int32 FaceResolution = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "FaceCellIndex"))
    int32 FaceCellIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "DisplayCellX"))
    int32 DisplayCellX = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "DisplayCellY"))
    int32 DisplayCellY = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "DisplayCellIndex"))
    int32 DisplayCellIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "FaceUVMin"))
    FVector2D FaceUVMin = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "FaceUVMax"))
    FVector2D FaceUVMax = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "FaceUVCenter"))
    FVector2D FaceUVCenter = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "LatitudeDegrees"))
    float LatitudeDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "LocalCenter"))
    FVector LocalCenter = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "WorldCenter"))
    FVector WorldCenter = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "LocalNormal"))
    FVector LocalNormal = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "WorldNormal"))
    FVector WorldNormal = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "ApproxSurfaceArea"))
    float ApproxSurfaceArea = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "Biome"))
    ESRPlanetBiome Biome = ESRPlanetBiome::Plains;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "BiomeId"))
    FName BiomeId = FName(TEXT("Plains"));

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "WaterRole"))
    ESRBiomeWaterRole WaterRole = ESRBiomeWaterRole::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface|Temperature", meta = (DisplayName = "SurfaceTemperature", ClampMin = "0.0", ClampMax = "1.0"))
    float SurfaceTemperature = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface|Temperature", meta = (DisplayName = "TemperatureState"))
    ESRFacilityTemperatureState TemperatureState = ESRFacilityTemperatureState::Normal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "Neighbors"))
    FSRPlanetSurfaceGridCellNeighbors Neighbors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "bCanConstruct"))
    bool bCanConstruct = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "bOccupied"))
    bool bOccupied = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "OccupantId"))
    FName OccupantId = NAME_None;
};
