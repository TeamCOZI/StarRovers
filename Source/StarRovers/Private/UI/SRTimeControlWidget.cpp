#include "UI/SRTimeControlWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Camera/SRPlayerController.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Celestial/SRStar.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "Styling/SlateColor.h"
#include "Utility/SRLog.h"

namespace
{
	enum class ESRTimeControlIconShape : uint8
	{
		Pause,
		Play,
		FastForward,
		Flask,
		Route,
		Stats,
		Help,
		Codex,
		Settings
	};

	struct FSRTimeControlButtonWidgets
	{
		USizeBox* SizeBox = nullptr;
		UButton* Button = nullptr;
		UImage* Image = nullptr;
	};

	constexpr int32 IconTextureSize = 64;
	constexpr int32 MiniMapTextureSize = 128;
	constexpr float DefaultButtonLength = 54.0f;
	constexpr float IconToButtonRatio = 0.58f;
	constexpr float LayoutRefreshIntervalSeconds = 0.25f;
	constexpr float StateRefreshIntervalSeconds = 0.05f;
	constexpr float LayoutSizeChangeTolerance = 0.5f;

	bool IsInsidePlayTriangle(float NormalizedX, float NormalizedY, float Left, float Right)
	{
		constexpr float Top = 0.20f;
		constexpr float Bottom = 0.80f;
		constexpr float CenterY = 0.50f;

		if (NormalizedX < Left || NormalizedX > Right || NormalizedY < Top || NormalizedY > Bottom)
		{
			return false;
		}

		const float HalfHeight = ((Right - NormalizedX) / FMath::Max(Right - Left, UE_SMALL_NUMBER)) * ((Bottom - Top) * 0.5f);
		return FMath::Abs(NormalizedY - CenterY) <= HalfHeight;
	}

	bool IsInsideIconShape(ESRTimeControlIconShape IconShape, int32 X, int32 Y)
	{
		const float NormalizedX = (static_cast<float>(X) + 0.5f) / static_cast<float>(IconTextureSize);
		const float NormalizedY = (static_cast<float>(Y) + 0.5f) / static_cast<float>(IconTextureSize);
		const float CenterX = NormalizedX - 0.5f;
		const float CenterY = NormalizedY - 0.5f;
		const float DistanceFromCenter = FMath::Sqrt(CenterX * CenterX + CenterY * CenterY);

		switch (IconShape)
		{
		case ESRTimeControlIconShape::Pause:
			return ((NormalizedX >= 0.28f && NormalizedX <= 0.42f) || (NormalizedX >= 0.58f && NormalizedX <= 0.72f))
				&& NormalizedY >= 0.20f
				&& NormalizedY <= 0.80f;

		case ESRTimeControlIconShape::Play:
			return IsInsidePlayTriangle(NormalizedX, NormalizedY, 0.30f, 0.76f);

		case ESRTimeControlIconShape::FastForward:
			return IsInsidePlayTriangle(NormalizedX, NormalizedY, 0.16f, 0.54f)
				|| IsInsidePlayTriangle(NormalizedX, NormalizedY, 0.46f, 0.84f);

		case ESRTimeControlIconShape::Flask:
			return (NormalizedX >= 0.43f && NormalizedX <= 0.57f && NormalizedY >= 0.16f && NormalizedY <= 0.42f)
				|| (NormalizedX >= 0.30f && NormalizedX <= 0.70f && NormalizedY >= 0.68f && NormalizedY <= 0.82f)
				|| (NormalizedY >= 0.38f && NormalizedY <= 0.78f && FMath::Abs(CenterX) <= 0.18f + ((NormalizedY - 0.38f) * 0.50f));

		case ESRTimeControlIconShape::Route:
			return (NormalizedX >= 0.18f && NormalizedX <= 0.76f && NormalizedY >= 0.20f && NormalizedY <= 0.80f && FMath::Abs((NormalizedX * 0.90f + NormalizedY) - 0.88f) <= 0.055f)
				|| (NormalizedX >= 0.62f && NormalizedX <= 0.82f && NormalizedY >= 0.18f && NormalizedY <= 0.38f && FMath::Abs((NormalizedX - 0.72f) + (NormalizedY - 0.28f)) <= 0.07f);

		case ESRTimeControlIconShape::Stats:
			return (DistanceFromCenter >= 0.26f && DistanceFromCenter <= 0.34f)
				|| (FMath::Abs(CenterX) <= 0.06f && NormalizedY >= 0.14f && NormalizedY <= 0.86f)
				|| (FMath::Abs(CenterY) <= 0.06f && NormalizedX >= 0.14f && NormalizedX <= 0.86f);

		case ESRTimeControlIconShape::Help:
			return (DistanceFromCenter >= 0.30f && DistanceFromCenter <= 0.40f)
				|| (NormalizedX >= 0.46f && NormalizedX <= 0.54f && NormalizedY >= 0.46f && NormalizedY <= 0.62f)
				|| (DistanceFromCenter <= 0.055f && NormalizedY >= 0.70f);

		case ESRTimeControlIconShape::Codex:
			return (NormalizedX >= 0.25f && NormalizedX <= 0.75f && NormalizedY >= 0.18f && NormalizedY <= 0.82f)
				&& !(NormalizedX >= 0.36f && NormalizedX <= 0.68f && NormalizedY >= 0.26f && NormalizedY <= 0.70f)
				|| (NormalizedX >= 0.30f && NormalizedX <= 0.40f && NormalizedY >= 0.16f && NormalizedY <= 0.84f);

		case ESRTimeControlIconShape::Settings:
			return (DistanceFromCenter >= 0.24f && DistanceFromCenter <= 0.34f)
				|| DistanceFromCenter <= 0.11f
				|| ((FMath::Abs(CenterX) <= 0.08f || FMath::Abs(CenterY) <= 0.08f) && DistanceFromCenter >= 0.30f && DistanceFromCenter <= 0.44f);
		}

		return false;
	}

	UTexture2D* CreateIconTexture(ESRTimeControlIconShape IconShape, const FName TextureName)
	{
		UTexture2D* IconTexture = UTexture2D::CreateTransient(IconTextureSize, IconTextureSize, PF_B8G8R8A8, TextureName);
		if (!IconTexture)
		{
			return nullptr;
		}

		TArray<FColor> Pixels;
		Pixels.SetNumZeroed(IconTextureSize * IconTextureSize);
		for (int32 Y = 0; Y < IconTextureSize; ++Y)
		{
			for (int32 X = 0; X < IconTextureSize; ++X)
			{
				if (IsInsideIconShape(IconShape, X, Y))
				{
					Pixels[Y * IconTextureSize + X] = FColor::White;
				}
			}
		}

		FTexturePlatformData* PlatformData = IconTexture->GetPlatformData();
		if (!PlatformData || PlatformData->Mips.Num() == 0)
		{
			return IconTexture;
		}

		FTexture2DMipMap& Mip = PlatformData->Mips[0];
		void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
		if (TextureData)
		{
			FMemory::Memcpy(TextureData, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
		}
		Mip.BulkData.Unlock();

		IconTexture->Filter = TF_Bilinear;
		IconTexture->SRGB = true;
		IconTexture->UpdateResource();
		return IconTexture;
	}

	void ApplyMiniMapOvalMask(TArray<FColor>& Pixels, int32 TextureSize, float MaskHeightToWidthRatio, int32 OutlineThicknessPixels)
	{
		if (TextureSize <= 0 || Pixels.Num() != TextureSize * TextureSize)
		{
			return;
		}

		const float SafeMaskHeightToWidthRatio = FMath::Clamp(MaskHeightToWidthRatio, 0.1f, 1.0f);
		const float RadiusX = 0.49f;
		const float RadiusY = RadiusX * SafeMaskHeightToWidthRatio;
		const float OutlineThicknessNormalized = OutlineThicknessPixels > 0
			? static_cast<float>(OutlineThicknessPixels) / (static_cast<float>(TextureSize) * RadiusX)
			: 0.0f;
		const float OutlineInnerDistance = FMath::Clamp(1.0f - OutlineThicknessNormalized, 0.0f, 1.0f);
		const float OutlineInnerDistanceSquared = OutlineInnerDistance * OutlineInnerDistance;

		for (int32 Y = 0; Y < TextureSize; ++Y)
		{
			for (int32 X = 0; X < TextureSize; ++X)
			{
				const float NormalizedX = (static_cast<float>(X) + 0.5f) / static_cast<float>(TextureSize);
				const float NormalizedY = (static_cast<float>(Y) + 0.5f) / static_cast<float>(TextureSize);
				const float EllipseX = (NormalizedX - 0.5f) / RadiusX;
				const float EllipseY = (NormalizedY - 0.5f) / RadiusY;
				const float EllipseDistanceSquared = (EllipseX * EllipseX) + (EllipseY * EllipseY);
				FColor& Pixel = Pixels[Y * TextureSize + X];

				if (EllipseDistanceSquared > 1.0f)
				{
					Pixel = FColor(0, 0, 0, 0);
				}
				else if (OutlineThicknessPixels > 0 && EllipseDistanceSquared >= OutlineInnerDistanceSquared)
				{
					Pixel = FColor(245, 248, 252, 255);
				}
			}
		}
	}

	UTexture2D* CreateMiniMapPlaceholderTexture(const FName TextureName, float MaskHeightToWidthRatio, int32 OutlineThicknessPixels)
	{
		UTexture2D* MiniMapTexture = UTexture2D::CreateTransient(MiniMapTextureSize, MiniMapTextureSize, PF_B8G8R8A8, TextureName);
		if (!MiniMapTexture)
		{
			return nullptr;
		}

		TArray<FColor> Pixels;
		Pixels.SetNumZeroed(MiniMapTextureSize * MiniMapTextureSize);
		for (int32 Y = 0; Y < MiniMapTextureSize; ++Y)
		{
			for (int32 X = 0; X < MiniMapTextureSize; ++X)
			{
				const float NormalizedX = (static_cast<float>(X) + 0.5f) / static_cast<float>(MiniMapTextureSize);
				const float NormalizedY = (static_cast<float>(Y) + 0.5f) / static_cast<float>(MiniMapTextureSize);
				const float CenterX = NormalizedX - 0.5f;
				const float CenterY = NormalizedY - 0.5f;
				const float DistanceFromCenter = FMath::Sqrt(CenterX * CenterX + CenterY * CenterY);

				FColor PixelColor = FColor(0, 0, 0, 0);
				if (DistanceFromCenter <= 0.48f)
				{
					const bool bLand =
						FMath::Sin((NormalizedX * 14.0f) + (NormalizedY * 7.0f)) > 0.22f
						|| FMath::Cos((NormalizedX * 8.0f) - (NormalizedY * 16.0f)) > 0.58f;
					PixelColor = bLand
						? FColor(135, 185, 235, 255)
						: FColor(67, 105, 215, 255);

					const uint8 Shade = static_cast<uint8>(FMath::Clamp((0.55f - DistanceFromCenter) * 120.0f, 0.0f, 40.0f));
					PixelColor.R = FMath::Clamp(static_cast<int32>(PixelColor.R) + Shade, 0, 255);
					PixelColor.G = FMath::Clamp(static_cast<int32>(PixelColor.G) + Shade, 0, 255);
					PixelColor.B = FMath::Clamp(static_cast<int32>(PixelColor.B) + Shade, 0, 255);
				}

				Pixels[Y * MiniMapTextureSize + X] = PixelColor;
			}
		}

		ApplyMiniMapOvalMask(Pixels, MiniMapTextureSize, MaskHeightToWidthRatio, OutlineThicknessPixels);

		FTexturePlatformData* PlatformData = MiniMapTexture->GetPlatformData();
		if (!PlatformData || PlatformData->Mips.Num() == 0)
		{
			return MiniMapTexture;
		}

		FTexture2DMipMap& Mip = PlatformData->Mips[0];
		void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
		if (TextureData)
		{
			FMemory::Memcpy(TextureData, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
		}
		Mip.BulkData.Unlock();

		MiniMapTexture->Filter = TF_Bilinear;
		MiniMapTexture->SRGB = true;
		MiniMapTexture->UpdateResource();
		return MiniMapTexture;
	}

	void SetImageBrush(UImage* IconImage, const FSlateBrush& ConfiguredBrush, UTexture2D* DefaultTexture)
	{
		if (!IconImage)
		{
			return;
		}

		if (ConfiguredBrush.GetResourceObject())
		{
			IconImage->SetBrush(ConfiguredBrush);
		}
		else if (DefaultTexture)
		{
			IconImage->SetBrushFromTexture(DefaultTexture, true);
		}

		IconImage->SetColorAndOpacity(FLinearColor::White);
	}

	void SetMiniMapImageBrush(UImage* TargetImage, const FSlateBrush& ConfiguredBrush, UTexture2D* DefaultTexture)
	{
		if (!TargetImage)
		{
			return;
		}

		if (ConfiguredBrush.GetResourceObject())
		{
			TargetImage->SetBrush(ConfiguredBrush);
		}
		else if (DefaultTexture)
		{
			TargetImage->SetBrushFromTexture(DefaultTexture, true);
		}

		TargetImage->SetColorAndOpacity(FLinearColor::White);
	}

	bool WriteTexturePixels(UTexture2D* Texture, const TArray<FColor>& Pixels)
	{
		if (!Texture || Pixels.IsEmpty())
		{
			return false;
		}

		FTexturePlatformData* PlatformData = Texture->GetPlatformData();
		if (!PlatformData || PlatformData->Mips.Num() == 0)
		{
			return false;
		}

		FTexture2DMipMap& Mip = PlatformData->Mips[0];
		const int64 RequiredBytes = static_cast<int64>(Pixels.Num()) * sizeof(FColor);
		if (Mip.BulkData.GetBulkDataSize() < RequiredBytes)
		{
			return false;
		}

		void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
		if (!TextureData)
		{
			return false;
		}

		FMemory::Memcpy(TextureData, Pixels.GetData(), RequiredBytes);
		Mip.BulkData.Unlock();
		Texture->UpdateResource();
		return true;
	}

	FColor GetMiniMapCellColor(const FSRPlanetSurfaceGridCell& Cell)
	{
		switch (Cell.WaterRole)
		{
		case ESRBiomeWaterRole::Ocean:
			return FColor(44, 86, 178, 255);
		case ESRBiomeWaterRole::Coast:
			return FColor(184, 205, 124, 255);
		case ESRBiomeWaterRole::River:
			return FColor(70, 150, 220, 255);
		case ESRBiomeWaterRole::Lake:
			return FColor(62, 130, 205, 255);
		case ESRBiomeWaterRole::None:
			break;
		}

		switch (Cell.Biome)
		{
		case ESRPlanetBiome::Ocean:
			return FColor(44, 86, 178, 255);
		case ESRPlanetBiome::Coast:
			return FColor(184, 205, 124, 255);
		case ESRPlanetBiome::Snow:
			return FColor(220, 236, 244, 255);
		case ESRPlanetBiome::Plains:
		default:
			break;
		}

		uint8 TemperatureTint = 20;
		switch (Cell.TemperatureState)
		{
		case ESRFacilityTemperatureState::Frozen:
			TemperatureTint = 0;
			break;
		case ESRFacilityTemperatureState::Cold:
			TemperatureTint = 10;
			break;
		case ESRFacilityTemperatureState::Hot:
			TemperatureTint = 30;
			break;
		case ESRFacilityTemperatureState::Overheated:
			TemperatureTint = 40;
			break;
		case ESRFacilityTemperatureState::Normal:
		default:
			TemperatureTint = 20;
			break;
		}
		return FColor(
			FMath::Clamp(82 + TemperatureTint, 0, 255),
			FMath::Clamp(150 - (TemperatureTint / 3), 0, 255),
			FMath::Clamp(74 - (TemperatureTint / 2), 0, 255),
			255);
	}

	void DrawMiniMapPoint(
		TArray<FColor>& Pixels,
		int32 TextureSize,
		int32 CenterX,
		int32 CenterY,
		int32 Radius,
		const FColor& Color,
		bool bWrapX)
	{
		if (TextureSize <= 0)
		{
			return;
		}

		for (int32 OffsetY = -Radius; OffsetY <= Radius; ++OffsetY)
		{
			const int32 PixelY = CenterY + OffsetY;
			if (PixelY < 0 || PixelY >= TextureSize)
			{
				continue;
			}

			for (int32 OffsetX = -Radius; OffsetX <= Radius; ++OffsetX)
			{
				int32 PixelX = CenterX + OffsetX;
				if (bWrapX)
				{
					PixelX = (PixelX % TextureSize + TextureSize) % TextureSize;
				}
				else if (PixelX < 0 || PixelX >= TextureSize)
				{
					continue;
				}

				Pixels[PixelY * TextureSize + PixelX] = Color;
			}
		}
	}

	bool ProjectSurfaceCellToMiniMapPixel(const FSRPlanetSurfaceGridCell& Cell, int32 TextureSize, int32& OutPixelX, int32& OutPixelY)
	{
		if (TextureSize <= 0 || Cell.LocalCenter.IsNearlyZero())
		{
			return false;
		}

		const FVector UnitDirection = Cell.LocalCenter.GetSafeNormal();
		const float Longitude = FMath::Atan2(UnitDirection.Y, UnitDirection.X);
		const float Latitude = FMath::Asin(FMath::Clamp(UnitDirection.Z, -1.0, 1.0));
		const float NormalizedX = (Longitude + UE_PI) / (2.0f * UE_PI);
		const float NormalizedY = 0.5f - (Latitude / UE_PI);

		OutPixelX = FMath::Clamp(FMath::RoundToInt(NormalizedX * static_cast<float>(TextureSize - 1)), 0, TextureSize - 1);
		OutPixelY = FMath::Clamp(FMath::RoundToInt(NormalizedY * static_cast<float>(TextureSize - 1)), 0, TextureSize - 1);
		return true;
	}

	bool RenderSurfaceGridMiniMapTexture(
		UTexture2D* Texture,
		const USRPlanetSurfaceGrid* SurfaceGrid,
		float MaskHeightToWidthRatio,
		int32 OutlineThicknessPixels)
	{
		if (!Texture || !SurfaceGrid)
		{
			return false;
		}

		FTexturePlatformData* PlatformData = Texture->GetPlatformData();
		if (!PlatformData || PlatformData->SizeX <= 0 || PlatformData->SizeX != PlatformData->SizeY)
		{
			return false;
		}

		const int32 TextureSize = PlatformData->SizeX;
		TArray<FColor> Pixels;
		Pixels.Init(FColor(18, 25, 28, 255), TextureSize * TextureSize);

		const TArray<FSRPlanetSurfaceGridCell>& Cells = SurfaceGrid->GetCellsRef();
		if (Cells.IsEmpty())
		{
			return WriteTexturePixels(Texture, Pixels);
		}

		const int32 FaceResolution = FMath::Max(1, SurfaceGrid->GetFaceResolution());
		const int32 CellPixelRadius = FMath::Clamp(FMath::CeilToInt(static_cast<float>(TextureSize) / (static_cast<float>(FaceResolution) * 2.25f)), 1, 3);

		for (const FSRPlanetSurfaceGridCell& Cell : Cells)
		{
			int32 PixelX = 0;
			int32 PixelY = 0;
			if (ProjectSurfaceCellToMiniMapPixel(Cell, TextureSize, PixelX, PixelY))
			{
				DrawMiniMapPoint(Pixels, TextureSize, PixelX, PixelY, CellPixelRadius, GetMiniMapCellColor(Cell), true);
			}
		}

		ApplyMiniMapOvalMask(Pixels, TextureSize, MaskHeightToWidthRatio, OutlineThicknessPixels);
		return WriteTexturePixels(Texture, Pixels);
	}

	FSRTimeControlButtonWidgets AddTopBarButton(
		UWidgetTree* WidgetTree,
		UCanvasPanel* ParentCanvasPanel,
		const FName SizeBoxName,
		const FName ButtonName,
		const FName ImageName,
		const FText& ToolTipText)
	{
		FSRTimeControlButtonWidgets ButtonWidgets;
		if (!WidgetTree || !ParentCanvasPanel)
		{
			return ButtonWidgets;
		}

		ButtonWidgets.SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), SizeBoxName);
		ButtonWidgets.Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		ButtonWidgets.Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), ImageName);
		if (!ButtonWidgets.SizeBox || !ButtonWidgets.Button || !ButtonWidgets.Image)
		{
			return ButtonWidgets;
		}

		ButtonWidgets.Button->SetBackgroundColor(FLinearColor(0.12f, 0.13f, 0.15f, 0.92f));
		ButtonWidgets.Button->SetToolTipText(ToolTipText);
		ButtonWidgets.Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		ButtonWidgets.Button->AddChild(ButtonWidgets.Image);
		if (UButtonSlot* ButtonImageSlot = Cast<UButtonSlot>(ButtonWidgets.Image->Slot))
		{
			ButtonImageSlot->SetPadding(FMargin(0.0f));
			ButtonImageSlot->SetHorizontalAlignment(HAlign_Center);
			ButtonImageSlot->SetVerticalAlignment(VAlign_Center);
		}

		ButtonWidgets.SizeBox->AddChild(ButtonWidgets.Button);
		if (USizeBoxSlot* SizeBoxButtonSlot = Cast<USizeBoxSlot>(ButtonWidgets.Button->Slot))
		{
			SizeBoxButtonSlot->SetPadding(FMargin(0.0f));
			SizeBoxButtonSlot->SetHorizontalAlignment(HAlign_Fill);
			SizeBoxButtonSlot->SetVerticalAlignment(VAlign_Fill);
		}

		if (UCanvasPanelSlot* ButtonSlot = ParentCanvasPanel->AddChildToCanvas(ButtonWidgets.SizeBox))
		{
			ButtonSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
			ButtonSlot->SetAlignment(FVector2D(0.0f, 0.0f));
			ButtonSlot->SetPosition(FVector2D::ZeroVector);
			ButtonSlot->SetSize(FVector2D(DefaultButtonLength, DefaultButtonLength));
		}

		return ButtonWidgets;
	}

	void AddProgressBarToContainer(UCanvasPanel* ParentCanvasPanel, UProgressBar* ProgressBar, float ProgressBarHeightRatio)
	{
		if (!ParentCanvasPanel || !ProgressBar)
		{
			return;
		}

		const float SafeProgressBarHeightRatio = FMath::Clamp(ProgressBarHeightRatio, 0.0f, 1.0f);
		const float ProgressBarTop = (1.0f - SafeProgressBarHeightRatio) * 0.5f;
		const float ProgressBarBottom = ProgressBarTop + SafeProgressBarHeightRatio;
		if (UCanvasPanelSlot* ProgressBarSlot = ParentCanvasPanel->AddChildToCanvas(ProgressBar))
		{
			ProgressBarSlot->SetAnchors(FAnchors(0.0f, ProgressBarTop, 1.0f, ProgressBarBottom));
			ProgressBarSlot->SetAlignment(FVector2D(0.0f, 0.0f));
			ProgressBarSlot->SetOffsets(FMargin(0.0f));
		}
	}

	bool TryGetPrimaryStarFuelState(const UWorld* World, FSRStellarFuelState& OutFuelState)
	{
		if (!World)
		{
			return false;
		}

		const USRCelestialBodyRegistrySubsystem* RegistrySubsystem = World->GetSubsystem<USRCelestialBodyRegistrySubsystem>();
		if (!IsValid(RegistrySubsystem))
		{
			return false;
		}

		const ASRStar* PrimaryStar = Cast<ASRStar>(RegistrySubsystem->GetPrimaryStarActor());
		if (!IsValid(PrimaryStar))
		{
			return false;
		}

		OutFuelState = PrimaryStar->GetStellarFuelState();
		return true;
	}

	FString FormatStarFuelAmount(double FuelAmount)
	{
		return FString::Printf(TEXT("%.0f"), FMath::Max(0.0, FuelAmount));
	}

	FString BuildStarFuelText(const FSRStellarFuelState& FuelState)
	{
		return FString::Printf(
			TEXT("%s / %s"),
			*FormatStarFuelAmount(FuelState.StoredFuel),
			*FormatStarFuelAmount(FuelState.InitialStageFuel));
	}
}

TSharedRef<SWidget> USRTimeControlWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	BuildTimeControlWidgetTree();
	return Super::RebuildWidget();
}

void USRTimeControlWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildTimeControlWidgetTree();
	BindTimeControlButtonHandlers();
	RefreshButtonIconBrushes();
	SynchronizeTopBarLayout();
	RefreshTimeControlState();
	RefreshProgressState();
	RefreshFocusedBodyState();
	if (bMiniMapEnabled)
	{
		RefreshMiniMapTextureFromFocusedBody(true);
	}
}

void USRTimeControlWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	BuildTimeControlWidgetTree();
	RefreshButtonIconBrushes();
	SynchronizeTopBarLayout();
	RefreshTimeControlState();
	RefreshProgressState();
	RefreshFocusedBodyState();
	if (bMiniMapEnabled)
	{
		RefreshMiniMapTextureFromFocusedBody(true);
	}
}

void USRTimeControlWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const float SafeDeltaTime = FMath::Max(0.0f, InDeltaTime);
	const FVector2D WidgetLocalSize = GetCachedGeometry().GetLocalSize();
	const FVector2D TopBarLocalSize = TopBarContainerBorder
		? TopBarContainerBorder->GetCachedGeometry().GetLocalSize()
		: FVector2D::ZeroVector;

	LayoutRefreshAccumulator += SafeDeltaTime;
	const bool bLayoutSizeChanged = !bHasLayoutState
		|| !WidgetLocalSize.Equals(LastLayoutWidgetSize, LayoutSizeChangeTolerance)
		|| !TopBarLocalSize.Equals(LastLayoutTopBarSize, LayoutSizeChangeTolerance);
	if (bLayoutSizeChanged || LayoutRefreshAccumulator >= LayoutRefreshIntervalSeconds)
	{
		LayoutRefreshAccumulator = 0.0f;
		LastLayoutWidgetSize = WidgetLocalSize;
		LastLayoutTopBarSize = TopBarLocalSize;
		bHasLayoutState = true;
		SynchronizeTopBarLayout();
	}

	StateRefreshAccumulator += SafeDeltaTime;
	if (StateRefreshAccumulator >= StateRefreshIntervalSeconds)
	{
		StateRefreshAccumulator = 0.0f;
		RefreshProgressState();
		RefreshFocusedBodyState();
	}

	if (bMiniMapEnabled)
	{
		MiniMapRefreshAccumulator += SafeDeltaTime;
		if (MiniMapRefreshInterval <= 0.0f || MiniMapRefreshAccumulator >= MiniMapRefreshInterval)
		{
			MiniMapRefreshAccumulator = 0.0f;
			RefreshMiniMapTextureFromFocusedBody(false);
		}
	}
}

FReply USRTimeControlWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
	if (IsScreenPositionOverTimeControlPanel(ScreenPosition))
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: TimeControl NativeOnMouseButtonDown handled Mouse=(%.1f, %.1f)"),
			ScreenPosition.X,
			ScreenPosition.Y);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USRTimeControlWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
	if (IsScreenPositionOverTimeControlPanel(ScreenPosition))
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: TimeControl NativeOnMouseButtonUp handled Mouse=(%.1f, %.1f)"),
			ScreenPosition.X,
			ScreenPosition.Y);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USRTimeControlWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsScreenPositionOverTimeControlPanel(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

bool USRTimeControlWidget::IsPointerOverTimeControlPanel() const
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	return IsScreenPositionOverTimeControlPanel(FSlateApplication::Get().GetCursorPos());
}

void USRTimeControlWidget::SetPauseButtonIcon(const FSlateBrush& NewIconBrush)
{
	PauseButtonIconBrush = NewIconBrush;
	RefreshButtonIconBrushes();
}

void USRTimeControlWidget::SetPlayButtonIcon(const FSlateBrush& NewIconBrush)
{
	PlayButtonIconBrush = NewIconBrush;
	RefreshButtonIconBrushes();
}

void USRTimeControlWidget::SetFastForwardButtonIcon(const FSlateBrush& NewIconBrush)
{
	FastForwardButtonIconBrush = NewIconBrush;
	RefreshButtonIconBrushes();
}

void USRTimeControlWidget::SetFlaskButtonIcon(const FSlateBrush& NewIconBrush)
{
	FlaskButtonIconBrush = NewIconBrush;
	RefreshButtonIconBrushes();
}

void USRTimeControlWidget::SetRouteButtonIcon(const FSlateBrush& NewIconBrush)
{
	RouteButtonIconBrush = NewIconBrush;
	RefreshButtonIconBrushes();
}

void USRTimeControlWidget::SetStatsButtonIcon(const FSlateBrush& NewIconBrush)
{
	StatsButtonIconBrush = NewIconBrush;
	RefreshButtonIconBrushes();
}

void USRTimeControlWidget::SetHelpButtonIcon(const FSlateBrush& NewIconBrush)
{
	HelpButtonIconBrush = NewIconBrush;
	RefreshButtonIconBrushes();
}

void USRTimeControlWidget::SetCodexButtonIcon(const FSlateBrush& NewIconBrush)
{
	CodexButtonIconBrush = NewIconBrush;
	RefreshButtonIconBrushes();
}

void USRTimeControlWidget::SetSettingsButtonIcon(const FSlateBrush& NewIconBrush)
{
	SettingsButtonIconBrush = NewIconBrush;
	RefreshButtonIconBrushes();
}

void USRTimeControlWidget::SetMiniMapImage(const FSlateBrush& NewImageBrush)
{
	MiniMapImageBrush = NewImageBrush;
	RefreshButtonIconBrushes();
}

void USRTimeControlWidget::BuildTimeControlWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* TimeControlCanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TimeControlCanvasPanel"));
	WidgetTree->RootWidget = TimeControlCanvasPanel;
	TimeControlCanvasPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	TopBarContainerBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TopBarContainerBorder"));
	TopBarContainerBorder->SetPadding(FMargin(0.0f));
	TopBarContainerBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));

	if (UCanvasPanelSlot* TopBarContainerSlot = TimeControlCanvasPanel->AddChildToCanvas(TopBarContainerBorder))
	{
		TopBarContainerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, TopBarHeightViewportRatio));
		TopBarContainerSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		TopBarContainerSlot->SetOffsets(FMargin(0.0f));
	}

	TopBarCanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TopBarCanvasPanel"));
	TopBarCanvasPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	TopBarContainerBorder->SetContent(TopBarCanvasPanel);

	TopLeftControlsCanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TopLeftControlsCanvasPanel"));
	TopLeftControlsCanvasPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UCanvasPanelSlot* TopLeftControlsSlot = TopBarCanvasPanel->AddChildToCanvas(TopLeftControlsCanvasPanel))
	{
		TopLeftControlsSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		TopLeftControlsSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		TopLeftControlsSlot->SetOffsets(FMargin(0.0f));
	}

	FuelSupplyProgressContainer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FuelSupplyProgressContainer"));
	FuelSupplyProgressContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UCanvasPanelSlot* FuelSupplyProgressContainerSlot = TopBarCanvasPanel->AddChildToCanvas(FuelSupplyProgressContainer))
	{
		FuelSupplyProgressContainerSlot->SetAnchors(FAnchors(0.25f, 0.0f, 0.75f, 0.5f));
		FuelSupplyProgressContainerSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		FuelSupplyProgressContainerSlot->SetOffsets(FMargin(0.0f));
	}

	FuelSupplyProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("FuelSupplyProgressBar"));
	if (FuelSupplyProgressBar)
	{
		FuelSupplyProgressBar->SetVisibility(ESlateVisibility::HitTestInvisible);
		FuelSupplyProgressBar->SetFillColorAndOpacity(FLinearColor(1.0f, 0.68f, 0.08f, 1.0f));
		FuelSupplyProgressBar->SetPercent(0.0f);
		AddProgressBarToContainer(FuelSupplyProgressContainer, FuelSupplyProgressBar, ProgressBarHeightRatio);
	}

	FuelSupplyProgressTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FuelSupplyProgressTextBlock"));
	USizeBox* FuelSupplyProgressTextSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("FuelSupplyProgressTextSizeBox"));
	if (FuelSupplyProgressTextSizeBox && FuelSupplyProgressTextBlock)
	{
		FSlateFontInfo FuelSupplyProgressFont = FuelSupplyProgressTextBlock->GetFont();
		FuelSupplyProgressFont.Size = 13;
		FuelSupplyProgressTextBlock->SetFont(FuelSupplyProgressFont);
		FuelSupplyProgressTextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
		FuelSupplyProgressTextBlock->SetJustification(ETextJustify::Center);
		FuelSupplyProgressTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		FuelSupplyProgressTextBlock->SetAutoWrapText(false);
		FuelSupplyProgressTextBlock->SetText(FText::FromString(TEXT("0 / 0")));
		FuelSupplyProgressTextSizeBox->AddChild(FuelSupplyProgressTextBlock);
		if (USizeBoxSlot* FuelSupplyProgressTextBlockSlot = Cast<USizeBoxSlot>(FuelSupplyProgressTextBlock->Slot))
		{
			FuelSupplyProgressTextBlockSlot->SetHorizontalAlignment(HAlign_Center);
			FuelSupplyProgressTextBlockSlot->SetVerticalAlignment(VAlign_Center);
			FuelSupplyProgressTextBlockSlot->SetPadding(FMargin(0.0f));
		}
		if (UCanvasPanelSlot* FuelSupplyProgressTextSlot = FuelSupplyProgressContainer->AddChildToCanvas(FuelSupplyProgressTextSizeBox))
		{
			FuelSupplyProgressTextSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			FuelSupplyProgressTextSlot->SetAlignment(FVector2D(0.0f, 0.0f));
			FuelSupplyProgressTextSlot->SetOffsets(FMargin(0.0f));
		}
	}

	CycleProgressContainer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CycleProgressContainer"));
	CycleProgressContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UCanvasPanelSlot* CycleProgressContainerSlot = TopBarCanvasPanel->AddChildToCanvas(CycleProgressContainer))
	{
		CycleProgressContainerSlot->SetAnchors(FAnchors(0.25f, 0.5f, 0.75f, 1.0f));
		CycleProgressContainerSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		CycleProgressContainerSlot->SetOffsets(FMargin(0.0f));
	}

	CycleProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("CycleProgressBar"));
	if (CycleProgressBar)
	{
		CycleProgressBar->SetVisibility(ESlateVisibility::HitTestInvisible);
		CycleProgressBar->SetFillColorAndOpacity(FLinearColor::White);
		CycleProgressBar->SetPercent(0.0f);
		AddProgressBarToContainer(CycleProgressContainer, CycleProgressBar, ProgressBarHeightRatio);
	}

	TopRightControlsCanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TopRightControlsCanvasPanel"));
	TopRightControlsCanvasPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UCanvasPanelSlot* TopRightControlsSlot = TopBarCanvasPanel->AddChildToCanvas(TopRightControlsCanvasPanel))
	{
		TopRightControlsSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		TopRightControlsSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		TopRightControlsSlot->SetOffsets(FMargin(0.0f));
	}

	const FSRTimeControlButtonWidgets PauseWidgets = AddTopBarButton(
		WidgetTree,
		TopLeftControlsCanvasPanel,
		TEXT("PauseButtonSizeBox"),
		TEXT("PauseButton"),
		TEXT("PauseButtonImage"),
		NSLOCTEXT("StarRoversTimeControl", "PauseButtonToolTip", "Pause"));
	PauseButtonSizeBox = PauseWidgets.SizeBox;
	PauseButton = PauseWidgets.Button;
	PauseButtonImage = PauseWidgets.Image;

	const FSRTimeControlButtonWidgets PlayWidgets = AddTopBarButton(
		WidgetTree,
		TopLeftControlsCanvasPanel,
		TEXT("PlayButtonSizeBox"),
		TEXT("PlayButton"),
		TEXT("PlayButtonImage"),
		NSLOCTEXT("StarRoversTimeControl", "PlayButtonToolTip", "Play"));
	PlayButtonSizeBox = PlayWidgets.SizeBox;
	PlayButton = PlayWidgets.Button;
	PlayButtonImage = PlayWidgets.Image;

	const FSRTimeControlButtonWidgets FastForwardWidgets = AddTopBarButton(
		WidgetTree,
		TopLeftControlsCanvasPanel,
		TEXT("FastForwardButtonSizeBox"),
		TEXT("FastForwardButton"),
		TEXT("FastForwardButtonImage"),
		NSLOCTEXT("StarRoversTimeControl", "FastForwardButtonToolTip", "Fast Forward"));
	FastForwardButtonSizeBox = FastForwardWidgets.SizeBox;
	FastForwardButton = FastForwardWidgets.Button;
	FastForwardButtonImage = FastForwardWidgets.Image;

	const FSRTimeControlButtonWidgets FlaskWidgets = AddTopBarButton(
		WidgetTree,
		TopRightControlsCanvasPanel,
		TEXT("FlaskButtonSizeBox"),
		TEXT("FlaskButton"),
		TEXT("FlaskButtonImage"),
		NSLOCTEXT("StarRoversTimeControl", "FlaskButtonToolTip", "Augments and Technologies"));
	FlaskButtonSizeBox = FlaskWidgets.SizeBox;
	FlaskButton = FlaskWidgets.Button;
	FlaskButtonImage = FlaskWidgets.Image;

	const FSRTimeControlButtonWidgets RouteWidgets = AddTopBarButton(
		WidgetTree,
		TopRightControlsCanvasPanel,
		TEXT("RouteButtonSizeBox"),
		TEXT("RouteButton"),
		TEXT("RouteButtonImage"),
		NSLOCTEXT("StarRoversTimeControl", "RouteButtonToolTip", "Routes"));
	RouteButtonSizeBox = RouteWidgets.SizeBox;
	RouteButton = RouteWidgets.Button;
	RouteButtonImage = RouteWidgets.Image;

	const FSRTimeControlButtonWidgets StatsWidgets = AddTopBarButton(
		WidgetTree,
		TopRightControlsCanvasPanel,
		TEXT("StatsButtonSizeBox"),
		TEXT("StatsButton"),
		TEXT("StatsButtonImage"),
		NSLOCTEXT("StarRoversTimeControl", "StatsButtonToolTip", "Statistics"));
	StatsButtonSizeBox = StatsWidgets.SizeBox;
	StatsButton = StatsWidgets.Button;
	StatsButtonImage = StatsWidgets.Image;

	const FSRTimeControlButtonWidgets HelpWidgets = AddTopBarButton(
		WidgetTree,
		TopRightControlsCanvasPanel,
		TEXT("HelpButtonSizeBox"),
		TEXT("HelpButton"),
		TEXT("HelpButtonImage"),
		NSLOCTEXT("StarRoversTimeControl", "HelpButtonToolTip", "Help"));
	HelpButtonSizeBox = HelpWidgets.SizeBox;
	HelpButton = HelpWidgets.Button;
	HelpButtonImage = HelpWidgets.Image;

	const FSRTimeControlButtonWidgets CodexWidgets = AddTopBarButton(
		WidgetTree,
		TopRightControlsCanvasPanel,
		TEXT("CodexButtonSizeBox"),
		TEXT("CodexButton"),
		TEXT("CodexButtonImage"),
		NSLOCTEXT("StarRoversTimeControl", "CodexButtonToolTip", "Codex"));
	CodexButtonSizeBox = CodexWidgets.SizeBox;
	CodexButton = CodexWidgets.Button;
	CodexButtonImage = CodexWidgets.Image;

	const FSRTimeControlButtonWidgets SettingsWidgets = AddTopBarButton(
		WidgetTree,
		TopRightControlsCanvasPanel,
		TEXT("SettingsButtonSizeBox"),
		TEXT("SettingsButton"),
		TEXT("SettingsButtonImage"),
		NSLOCTEXT("StarRoversTimeControl", "SettingsButtonToolTip", "Settings"));
	SettingsButtonSizeBox = SettingsWidgets.SizeBox;
	SettingsButton = SettingsWidgets.Button;
	SettingsButtonImage = SettingsWidgets.Image;

	CycleCountContainerBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CycleCountContainerBorder"));
	CycleCountContainerBorder->SetPadding(FMargin(0.0f));
	CycleCountContainerBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));

	if (UCanvasPanelSlot* CycleCountContainerSlot = TimeControlCanvasPanel->AddChildToCanvas(CycleCountContainerBorder))
	{
		CycleCountContainerSlot->SetAnchors(FAnchors(0.0f, TopBarHeightViewportRatio, 1.0f, TopBarHeightViewportRatio + CycleCountHeightViewportRatio));
		CycleCountContainerSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		CycleCountContainerSlot->SetOffsets(FMargin(0.0f));
	}

	CycleCountCanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CycleCountCanvasPanel"));
	CycleCountCanvasPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	CycleCountContainerBorder->SetContent(CycleCountCanvasPanel);

	CycleCountTextSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CycleCountTextSizeBox"));
	CycleCountTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CycleCountTextBlock"));
	if (CycleCountTextSizeBox && CycleCountTextBlock)
	{
		FSlateFontInfo CycleCountFont = CycleCountTextBlock->GetFont();
		CycleCountFont.Size = 20;
		CycleCountTextBlock->SetFont(CycleCountFont);
		CycleCountTextBlock->SetJustification(ETextJustify::Center);
		CycleCountTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		CycleCountTextBlock->SetAutoWrapText(false);
		CycleCountTextBlock->SetText(FText::FromString(TEXT("0\uC8FC\uAE30")));
		CycleCountTextSizeBox->AddChild(CycleCountTextBlock);
		if (USizeBoxSlot* CycleCountTextSlot = Cast<USizeBoxSlot>(CycleCountTextBlock->Slot))
		{
			CycleCountTextSlot->SetHorizontalAlignment(HAlign_Center);
			CycleCountTextSlot->SetVerticalAlignment(VAlign_Center);
			CycleCountTextSlot->SetPadding(FMargin(0.0f));
		}

		if (UCanvasPanelSlot* CycleCountTextSizeBoxSlot = CycleCountCanvasPanel->AddChildToCanvas(CycleCountTextSizeBox))
		{
			CycleCountTextSizeBoxSlot->SetAnchors(FAnchors(0.425f, 0.0f, 0.575f, 1.0f));
			CycleCountTextSizeBoxSlot->SetAlignment(FVector2D(0.0f, 0.0f));
			CycleCountTextSizeBoxSlot->SetOffsets(FMargin(0.0f));
		}
	}

	BottomFocusNameContainerBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BottomFocusNameContainerBorder"));
	BottomFocusNameContainerBorder->SetPadding(FMargin(0.0f));
	BottomFocusNameContainerBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
	if (UCanvasPanelSlot* BottomFocusNameContainerSlot = TimeControlCanvasPanel->AddChildToCanvas(BottomFocusNameContainerBorder))
	{
		BottomFocusNameContainerSlot->SetAnchors(FAnchors(0.0f, 1.0f - BottomFocusNameHeightViewportRatio, 1.0f, 1.0f));
		BottomFocusNameContainerSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		BottomFocusNameContainerSlot->SetOffsets(FMargin(0.0f));
	}

	BottomFocusNameCanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BottomFocusNameCanvasPanel"));
	BottomFocusNameCanvasPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BottomFocusNameContainerBorder->SetContent(BottomFocusNameCanvasPanel);

	BottomFocusNameTextSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BottomFocusNameTextSizeBox"));
	BottomFocusNameTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BottomFocusNameTextBlock"));
	if (BottomFocusNameTextSizeBox && BottomFocusNameTextBlock)
	{
		FSlateFontInfo BottomFocusNameFont = BottomFocusNameTextBlock->GetFont();
		BottomFocusNameFont.Size = 18;
		BottomFocusNameTextBlock->SetFont(BottomFocusNameFont);
		BottomFocusNameTextBlock->SetJustification(ETextJustify::Center);
		BottomFocusNameTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		BottomFocusNameTextBlock->SetAutoWrapText(false);
		BottomFocusNameTextBlock->SetText(FText::FromString(TEXT("Planet_Name")));
		BottomFocusNameTextSizeBox->AddChild(BottomFocusNameTextBlock);
		if (USizeBoxSlot* BottomFocusNameTextSlot = Cast<USizeBoxSlot>(BottomFocusNameTextBlock->Slot))
		{
			BottomFocusNameTextSlot->SetHorizontalAlignment(HAlign_Center);
			BottomFocusNameTextSlot->SetVerticalAlignment(VAlign_Center);
			BottomFocusNameTextSlot->SetPadding(FMargin(0.0f));
		}

		if (UCanvasPanelSlot* BottomFocusNameTextSizeBoxSlot = BottomFocusNameCanvasPanel->AddChildToCanvas(BottomFocusNameTextSizeBox))
		{
			BottomFocusNameTextSizeBoxSlot->SetAnchors(FAnchors(0.0f, 0.0f, BottomFocusWidthViewportRatio, 1.0f));
			BottomFocusNameTextSizeBoxSlot->SetAlignment(FVector2D(0.0f, 0.0f));
			BottomFocusNameTextSizeBoxSlot->SetOffsets(FMargin(0.0f));
		}
	}

	MiniMapContainerBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MiniMapContainerBorder"));
	MiniMapContainerBorder->SetPadding(FMargin(0.0f));
	MiniMapContainerBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
	MiniMapContainerBorder->SetVisibility(bMiniMapEnabled ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* MiniMapContainerSlot = TimeControlCanvasPanel->AddChildToCanvas(MiniMapContainerBorder))
	{
		MiniMapContainerSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
		MiniMapContainerSlot->SetAlignment(FVector2D(0.0f, 1.0f));
		MiniMapContainerSlot->SetPosition(FVector2D(0.0f, 0.0f));
		MiniMapContainerSlot->SetSize(FVector2D(DefaultButtonLength * 3.0f, DefaultButtonLength * 3.0f));
	}

	MiniMapCanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MiniMapCanvasPanel"));
	MiniMapCanvasPanel->SetVisibility(bMiniMapEnabled ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	MiniMapContainerBorder->SetContent(MiniMapCanvasPanel);

	MiniMapImageSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MiniMapImageSizeBox"));
	MiniMapImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MiniMapImage"));
	if (MiniMapImageSizeBox && MiniMapImage)
	{
		MiniMapImage->SetVisibility(bMiniMapEnabled ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		MiniMapImageSizeBox->AddChild(MiniMapImage);
		if (USizeBoxSlot* MiniMapImageSlot = Cast<USizeBoxSlot>(MiniMapImage->Slot))
		{
			MiniMapImageSlot->SetHorizontalAlignment(HAlign_Center);
			MiniMapImageSlot->SetVerticalAlignment(VAlign_Center);
			MiniMapImageSlot->SetPadding(FMargin(0.0f));
		}

		if (UCanvasPanelSlot* MiniMapImageSizeBoxSlot = MiniMapCanvasPanel->AddChildToCanvas(MiniMapImageSizeBox))
		{
			MiniMapImageSizeBoxSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			MiniMapImageSizeBoxSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			MiniMapImageSizeBoxSlot->SetPosition(FVector2D(0.0f, 0.0f));
			MiniMapImageSizeBoxSlot->SetSize(FVector2D(DefaultButtonLength * 2.7f, DefaultButtonLength * 2.7f));
		}
	}

	RefreshButtonIconBrushes();
	SynchronizeTopBarLayout();
	RefreshProgressState();
	RefreshFocusedBodyState();
	if (bMiniMapEnabled)
	{
		RefreshMiniMapTextureFromFocusedBody(true);
	}
}

void USRTimeControlWidget::BindTimeControlButtonHandlers()
{
	if (PauseButton)
	{
		PauseButton->OnClicked.RemoveAll(this);
		PauseButton->OnClicked.AddDynamic(this, &USRTimeControlWidget::HandlePauseClicked);
	}

	if (PlayButton)
	{
		PlayButton->OnClicked.RemoveAll(this);
		PlayButton->OnClicked.AddDynamic(this, &USRTimeControlWidget::HandlePlayClicked);
	}

	if (FastForwardButton)
	{
		FastForwardButton->OnClicked.RemoveAll(this);
		FastForwardButton->OnClicked.AddDynamic(this, &USRTimeControlWidget::HandleFastForwardClicked);
	}
}

void USRTimeControlWidget::RefreshTimeControlState()
{
	const USRTimeControlSubsystem* TimeControlSubsystem = GetTimeControlSubsystem();
	const bool bHasTimeControlSubsystem = IsValid(TimeControlSubsystem);
	const bool bIsPaused = bHasTimeControlSubsystem ? TimeControlSubsystem->IsSimulationPaused() : false;
	const float CurrentTimeScale = bHasTimeControlSubsystem ? TimeControlSubsystem->GetTimeScale() : 1.0f;
	const float SafeFastForwardTimeScale = FMath::Max(0.0f, FastForwardTimeScale);

	const bool bIs1xActive = !bIsPaused && FMath::IsNearlyEqual(CurrentTimeScale, 1.0f);
	const bool bIsFastForwardActive = !bIsPaused && FMath::IsNearlyEqual(CurrentTimeScale, SafeFastForwardTimeScale);

	UpdateButtonStyle(PauseButton, bHasTimeControlSubsystem && bIsPaused);
	UpdateButtonStyle(PlayButton, bHasTimeControlSubsystem && bIs1xActive);
	UpdateButtonStyle(FastForwardButton, bHasTimeControlSubsystem && bIsFastForwardActive);

	if (PauseButton)
	{
		PauseButton->SetIsEnabled(bHasTimeControlSubsystem && !bIsPaused);
	}

	if (PlayButton)
	{
		PlayButton->SetIsEnabled(bHasTimeControlSubsystem);
	}

	if (FastForwardButton)
	{
		FastForwardButton->SetIsEnabled(bHasTimeControlSubsystem);
	}

	OnTimeControlChanged(bIsPaused, CurrentTimeScale);
	RefreshProgressState();
}

void USRTimeControlWidget::RefreshProgressState()
{
	const USRTimeControlSubsystem* TimeControlSubsystem = GetTimeControlSubsystem();
	const bool bHasTimeControlSubsystem = IsValid(TimeControlSubsystem);
	FSRStellarFuelState PrimaryStarFuelState;
	const bool bHasPrimaryStarFuelState = TryGetPrimaryStarFuelState(GetWorld(), PrimaryStarFuelState);
	const double MaxStarFuel = bHasPrimaryStarFuelState ? FMath::Max(0.0, PrimaryStarFuelState.InitialStageFuel) : 0.0;
	const float FuelSupplyProgressRatio = MaxStarFuel > UE_DOUBLE_SMALL_NUMBER
		? FMath::Clamp(static_cast<float>(PrimaryStarFuelState.StoredFuel / MaxStarFuel), 0.0f, 1.0f)
		: 0.0f;
	const FString FuelSupplyTextString = bHasPrimaryStarFuelState
		? BuildStarFuelText(PrimaryStarFuelState)
		: FString(TEXT("0 / 0"));
	const float CycleProgressRatio = bHasTimeControlSubsystem ? TimeControlSubsystem->GetCycleProgressRatio() : 0.0f;
	const int32 CurrentCycleIndex = bHasTimeControlSubsystem ? TimeControlSubsystem->GetCurrentCycleIndex() : 0;

	if (FuelSupplyProgressBar && !FMath::IsNearlyEqual(LastFuelSupplyProgressRatio, FuelSupplyProgressRatio, 0.0001f))
	{
		FuelSupplyProgressBar->SetPercent(FuelSupplyProgressRatio);
		LastFuelSupplyProgressRatio = FuelSupplyProgressRatio;
	}

	if (FuelSupplyProgressTextBlock && LastFuelSupplyTextString != FuelSupplyTextString)
	{
		FuelSupplyProgressTextBlock->SetText(FText::FromString(FuelSupplyTextString));
		LastFuelSupplyTextString = FuelSupplyTextString;
	}

	if (CycleProgressBar && !FMath::IsNearlyEqual(LastCycleProgressRatio, CycleProgressRatio, 0.0001f))
	{
		CycleProgressBar->SetPercent(CycleProgressRatio);
		LastCycleProgressRatio = CycleProgressRatio;
	}

	if (CycleCountTextBlock && LastCycleIndex != CurrentCycleIndex)
	{
		CycleCountTextBlock->SetText(FText::FromString(FString::Printf(TEXT("%d\uC8FC\uAE30"), CurrentCycleIndex)));
		LastCycleIndex = CurrentCycleIndex;
	}
}

void USRTimeControlWidget::RefreshFocusedBodyState()
{
	FText FocusedBodyName = FText::FromString(TEXT("Planet_Name"));
	if (const ASRPlayerController* PlayerController = Cast<ASRPlayerController>(GetOwningPlayer()))
	{
		const FSRCelestialBodyFocusInfo FocusInfo = PlayerController->GetSelectedActorFocusInfo();
		if (FocusInfo.bIsValid && !FocusInfo.VariableName.IsEmpty())
		{
			FocusedBodyName = FocusInfo.VariableName;
		}
	}

	const FString FocusedBodyNameString = FocusedBodyName.ToString();
	if (BottomFocusNameTextBlock && LastFocusedBodyNameString != FocusedBodyNameString)
	{
		BottomFocusNameTextBlock->SetText(FocusedBodyName);
		LastFocusedBodyNameString = FocusedBodyNameString;
	}
}

void USRTimeControlWidget::RefreshMiniMapTextureFromFocusedBody(bool bForceRefresh)
{
	if (!bMiniMapEnabled)
	{
		MiniMapRefreshAccumulator = 0.0f;
		LastMiniMapSourceActor = nullptr;
		if (MiniMapContainerBorder)
		{
			MiniMapContainerBorder->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (MiniMapCanvasPanel)
		{
			MiniMapCanvasPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (MiniMapImage)
		{
			MiniMapImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (bForceRefresh)
	{
		MiniMapRefreshAccumulator = 0.0f;
	}

	if (MiniMapContainerBorder)
	{
		MiniMapContainerBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	if (MiniMapCanvasPanel)
	{
		MiniMapCanvasPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	if (MiniMapImage)
	{
		MiniMapImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	AActor* FocusedActor = nullptr;
	if (const ASRPlayerController* PlayerController = Cast<ASRPlayerController>(GetOwningPlayer()))
	{
		const FSRCelestialBodyFocusInfo FocusInfo = PlayerController->GetSelectedActorFocusInfo();
		FocusedActor = FocusInfo.bIsValid ? FocusInfo.Actor.Get() : PlayerController->GetSelectedActor();
	}

	if (bMiniMapEnabled && !DefaultMiniMapTexture)
	{
		DefaultMiniMapTexture = CreateMiniMapPlaceholderTexture(
			TEXT("SR_TempMiniMap"),
			MiniMapMaskHeightToWidthRatio,
			MiniMapOutlineThicknessPixels);
	}

	const USRPlanetSurfaceGrid* SurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(FocusedActor);
	if (!bForceRefresh && LastMiniMapSourceActor.Get() == FocusedActor && !SurfaceGrid)
	{
		return;
	}

	LastMiniMapSourceActor = FocusedActor;
	if (SurfaceGrid && DefaultMiniMapTexture)
	{
		RenderSurfaceGridMiniMapTexture(
			DefaultMiniMapTexture,
			SurfaceGrid,
			MiniMapMaskHeightToWidthRatio,
			MiniMapOutlineThicknessPixels);
	}

	SetMiniMapImageBrush(MiniMapImage, MiniMapImageBrush, DefaultMiniMapTexture);
}

void USRTimeControlWidget::RefreshButtonIconBrushes()
{
	if (!DefaultPauseIconTexture)
	{
		DefaultPauseIconTexture = CreateIconTexture(ESRTimeControlIconShape::Pause, TEXT("SR_TempPauseIcon"));
	}

	if (!DefaultPlayIconTexture)
	{
		DefaultPlayIconTexture = CreateIconTexture(ESRTimeControlIconShape::Play, TEXT("SR_TempPlayIcon"));
	}

	if (!DefaultFastForwardIconTexture)
	{
		DefaultFastForwardIconTexture = CreateIconTexture(ESRTimeControlIconShape::FastForward, TEXT("SR_TempFastForwardIcon"));
	}

	if (!DefaultFlaskIconTexture)
	{
		DefaultFlaskIconTexture = CreateIconTexture(ESRTimeControlIconShape::Flask, TEXT("SR_TempFlaskIcon"));
	}

	if (!DefaultRouteIconTexture)
	{
		DefaultRouteIconTexture = CreateIconTexture(ESRTimeControlIconShape::Route, TEXT("SR_TempRouteIcon"));
	}

	if (!DefaultStatsIconTexture)
	{
		DefaultStatsIconTexture = CreateIconTexture(ESRTimeControlIconShape::Stats, TEXT("SR_TempStatsIcon"));
	}

	if (!DefaultHelpIconTexture)
	{
		DefaultHelpIconTexture = CreateIconTexture(ESRTimeControlIconShape::Help, TEXT("SR_TempHelpIcon"));
	}

	if (!DefaultCodexIconTexture)
	{
		DefaultCodexIconTexture = CreateIconTexture(ESRTimeControlIconShape::Codex, TEXT("SR_TempCodexIcon"));
	}

	if (!DefaultSettingsIconTexture)
	{
		DefaultSettingsIconTexture = CreateIconTexture(ESRTimeControlIconShape::Settings, TEXT("SR_TempSettingsIcon"));
	}

	if (bMiniMapEnabled && !DefaultMiniMapTexture)
	{
		DefaultMiniMapTexture = CreateMiniMapPlaceholderTexture(
			TEXT("SR_TempMiniMap"),
			MiniMapMaskHeightToWidthRatio,
			MiniMapOutlineThicknessPixels);
	}

	SetImageBrush(PauseButtonImage, PauseButtonIconBrush, DefaultPauseIconTexture);
	SetImageBrush(PlayButtonImage, PlayButtonIconBrush, DefaultPlayIconTexture);
	SetImageBrush(FastForwardButtonImage, FastForwardButtonIconBrush, DefaultFastForwardIconTexture);
	SetImageBrush(FlaskButtonImage, FlaskButtonIconBrush, DefaultFlaskIconTexture);
	SetImageBrush(RouteButtonImage, RouteButtonIconBrush, DefaultRouteIconTexture);
	SetImageBrush(StatsButtonImage, StatsButtonIconBrush, DefaultStatsIconTexture);
	SetImageBrush(HelpButtonImage, HelpButtonIconBrush, DefaultHelpIconTexture);
	SetImageBrush(CodexButtonImage, CodexButtonIconBrush, DefaultCodexIconTexture);
	SetImageBrush(SettingsButtonImage, SettingsButtonIconBrush, DefaultSettingsIconTexture);

	if (bMiniMapEnabled)
	{
		SetMiniMapImageBrush(MiniMapImage, MiniMapImageBrush, DefaultMiniMapTexture);
	}
	else
	{
		if (MiniMapContainerBorder)
		{
			MiniMapContainerBorder->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (MiniMapCanvasPanel)
		{
			MiniMapCanvasPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (MiniMapImage)
		{
			MiniMapImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void USRTimeControlWidget::SynchronizeTopBarLayout()
{
	const float SafeTopBarRatio = FMath::Clamp(TopBarHeightViewportRatio, 0.0f, 1.0f);
	const float SafeCycleCountRatio = FMath::Clamp(CycleCountHeightViewportRatio, 0.0f, 1.0f);
	const float SafeBottomFocusNameRatio = FMath::Clamp(BottomFocusNameHeightViewportRatio, 0.0f, 1.0f);
	const float SafeBottomFocusWidthRatio = FMath::Clamp(BottomFocusWidthViewportRatio, 0.0f, 1.0f);
	const float SafeMiniMapInnerRatio = FMath::Clamp(MiniMapInnerSizeRatio, 0.0f, 1.0f);
	const float SafeTopCenterWidthRatio = FMath::Clamp(TopCenterWidthViewportRatio, 0.0f, 1.0f);
	const float SafeProgressBarHeightRatio = FMath::Clamp(ProgressBarHeightRatio, 0.0f, 1.0f);
	const float SafeCycleCountWidthRatio = FMath::Clamp(CycleCountWidthViewportRatio, 0.0f, 1.0f);
	const float SafeGapRatio = FMath::Max(0.0f, ButtonGapToWidthRatio);

	float ButtonLength = 0.0f;
	if (TopBarContainerBorder)
	{
		ButtonLength = TopBarContainerBorder->GetCachedGeometry().GetLocalSize().Y;
	}

	if (ButtonLength <= UE_SMALL_NUMBER)
	{
		if (const APlayerController* OwningPlayer = GetOwningPlayer())
		{
			int32 ViewportSizeX = 0;
			int32 ViewportSizeY = 0;
			OwningPlayer->GetViewportSize(ViewportSizeX, ViewportSizeY);
			if (ViewportSizeY > 0)
			{
				ButtonLength = static_cast<float>(ViewportSizeY) * SafeTopBarRatio;
			}
		}
	}

	if (ButtonLength <= UE_SMALL_NUMBER)
	{
		ButtonLength = DefaultButtonLength;
	}

	ButtonLength = FMath::Max(1.0f, ButtonLength);
	const float ButtonGap = ButtonLength * SafeGapRatio;
	const float IconLength = FMath::Max(1.0f, ButtonLength * IconToButtonRatio);
	FVector2D ViewportLocalSize = GetCachedGeometry().GetLocalSize();
	if (ViewportLocalSize.X <= UE_SMALL_NUMBER || ViewportLocalSize.Y <= UE_SMALL_NUMBER)
	{
		if (const APlayerController* OwningPlayer = GetOwningPlayer())
		{
			int32 ViewportSizeX = 0;
			int32 ViewportSizeY = 0;
			OwningPlayer->GetViewportSize(ViewportSizeX, ViewportSizeY);
			ViewportLocalSize = FVector2D(static_cast<float>(ViewportSizeX), static_cast<float>(ViewportSizeY));
		}
	}

	if (ViewportLocalSize.X <= UE_SMALL_NUMBER)
	{
		ViewportLocalSize.X = ButtonLength * 20.0f;
	}

	if (ViewportLocalSize.Y <= UE_SMALL_NUMBER)
	{
		ViewportLocalSize.Y = ButtonLength * 20.0f;
	}

	if (TopBarContainerBorder)
	{
		if (UCanvasPanelSlot* TopBarContainerSlot = Cast<UCanvasPanelSlot>(TopBarContainerBorder->Slot))
		{
			TopBarContainerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, SafeTopBarRatio));
			TopBarContainerSlot->SetOffsets(FMargin(0.0f));
		}
	}

	if (CycleCountContainerBorder)
	{
		if (UCanvasPanelSlot* CycleCountContainerSlot = Cast<UCanvasPanelSlot>(CycleCountContainerBorder->Slot))
		{
			CycleCountContainerSlot->SetAnchors(FAnchors(0.0f, SafeTopBarRatio, 1.0f, FMath::Clamp(SafeTopBarRatio + SafeCycleCountRatio, 0.0f, 1.0f)));
			CycleCountContainerSlot->SetOffsets(FMargin(0.0f));
		}
	}

	const float TopCenterLeft = (1.0f - SafeTopCenterWidthRatio) * 0.5f;
	const float TopCenterRight = TopCenterLeft + SafeTopCenterWidthRatio;
	auto ApplyTopCenterContainerLayout = [TopCenterLeft, TopCenterRight](UCanvasPanel* Container, float TopAnchor, float BottomAnchor)
	{
		if (Container)
		{
			if (UCanvasPanelSlot* ContainerSlot = Cast<UCanvasPanelSlot>(Container->Slot))
			{
				ContainerSlot->SetAnchors(FAnchors(TopCenterLeft, TopAnchor, TopCenterRight, BottomAnchor));
				ContainerSlot->SetOffsets(FMargin(0.0f));
			}
		}
	};

	auto ApplyProgressBarLayout = [SafeProgressBarHeightRatio](UProgressBar* ProgressBar)
	{
		if (ProgressBar)
		{
			if (UCanvasPanelSlot* ProgressBarSlot = Cast<UCanvasPanelSlot>(ProgressBar->Slot))
			{
				const float ProgressBarTop = (1.0f - SafeProgressBarHeightRatio) * 0.5f;
				const float ProgressBarBottom = ProgressBarTop + SafeProgressBarHeightRatio;
				ProgressBarSlot->SetAnchors(FAnchors(0.0f, ProgressBarTop, 1.0f, ProgressBarBottom));
				ProgressBarSlot->SetOffsets(FMargin(0.0f));
			}
		}
	};

	ApplyTopCenterContainerLayout(FuelSupplyProgressContainer, 0.0f, 0.5f);
	ApplyTopCenterContainerLayout(CycleProgressContainer, 0.5f, 1.0f);
	ApplyProgressBarLayout(FuelSupplyProgressBar);
	ApplyProgressBarLayout(CycleProgressBar);

	if (CycleCountTextSizeBox)
	{
		if (UCanvasPanelSlot* CycleCountTextSizeBoxSlot = Cast<UCanvasPanelSlot>(CycleCountTextSizeBox->Slot))
		{
			const float CycleCountLeft = (1.0f - SafeCycleCountWidthRatio) * 0.5f;
			const float CycleCountRight = CycleCountLeft + SafeCycleCountWidthRatio;
			CycleCountTextSizeBoxSlot->SetAnchors(FAnchors(CycleCountLeft, 0.0f, CycleCountRight, 1.0f));
			CycleCountTextSizeBoxSlot->SetOffsets(FMargin(0.0f));
		}
	}

	if (CycleCountTextBlock && CycleCountContainerBorder)
	{
		float CycleCountHeight = CycleCountContainerBorder->GetCachedGeometry().GetLocalSize().Y;
		if (CycleCountHeight <= UE_SMALL_NUMBER)
		{
			CycleCountHeight = ButtonLength;
		}

		FSlateFontInfo CycleCountFont = CycleCountTextBlock->GetFont();
		CycleCountFont.Size = FMath::Max(10, FMath::RoundToInt(CycleCountHeight * 0.56f));
		CycleCountTextBlock->SetFont(CycleCountFont);
	}

	if (BottomFocusNameContainerBorder)
	{
		if (UCanvasPanelSlot* BottomFocusNameContainerSlot = Cast<UCanvasPanelSlot>(BottomFocusNameContainerBorder->Slot))
		{
			BottomFocusNameContainerSlot->SetAnchors(FAnchors(0.0f, 1.0f - SafeBottomFocusNameRatio, 1.0f, 1.0f));
			BottomFocusNameContainerSlot->SetOffsets(FMargin(0.0f));
		}
	}

	if (BottomFocusNameTextSizeBox)
	{
		if (UCanvasPanelSlot* BottomFocusNameTextSizeBoxSlot = Cast<UCanvasPanelSlot>(BottomFocusNameTextSizeBox->Slot))
		{
			BottomFocusNameTextSizeBoxSlot->SetAnchors(FAnchors(0.0f, 0.0f, SafeBottomFocusWidthRatio, 1.0f));
			BottomFocusNameTextSizeBoxSlot->SetOffsets(FMargin(0.0f));
		}
	}

	const float BottomFocusNameHeight = ViewportLocalSize.Y * SafeBottomFocusNameRatio;
	if (BottomFocusNameTextBlock)
	{
		FSlateFontInfo BottomFocusNameFont = BottomFocusNameTextBlock->GetFont();
		BottomFocusNameFont.Size = FMath::Max(10, FMath::RoundToInt(BottomFocusNameHeight * 0.56f));
		BottomFocusNameTextBlock->SetFont(BottomFocusNameFont);
	}

	if (!bMiniMapEnabled)
	{
		if (MiniMapContainerBorder)
		{
			MiniMapContainerBorder->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (MiniMapImage)
		{
			MiniMapImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else
	{
		const float MiniMapLength = FMath::Max(1.0f, ViewportLocalSize.X * SafeBottomFocusWidthRatio);
		const float MiniMapInnerLength = FMath::Max(1.0f, MiniMapLength * SafeMiniMapInnerRatio);
		if (MiniMapContainerBorder)
		{
			MiniMapContainerBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			if (UCanvasPanelSlot* MiniMapContainerSlot = Cast<UCanvasPanelSlot>(MiniMapContainerBorder->Slot))
			{
				MiniMapContainerSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
				MiniMapContainerSlot->SetAlignment(FVector2D(0.0f, 1.0f));
				MiniMapContainerSlot->SetPosition(FVector2D(0.0f, -BottomFocusNameHeight));
				MiniMapContainerSlot->SetSize(FVector2D(MiniMapLength, MiniMapLength));
			}
		}

		if (MiniMapImageSizeBox)
		{
			MiniMapImageSizeBox->SetWidthOverride(MiniMapInnerLength);
			MiniMapImageSizeBox->SetHeightOverride(MiniMapInnerLength);
			MiniMapImageSizeBox->SetMinDesiredWidth(MiniMapInnerLength);
			MiniMapImageSizeBox->SetMinDesiredHeight(MiniMapInnerLength);
			MiniMapImageSizeBox->SetMaxDesiredWidth(MiniMapInnerLength);
			MiniMapImageSizeBox->SetMaxDesiredHeight(MiniMapInnerLength);

			if (UCanvasPanelSlot* MiniMapImageSizeBoxSlot = Cast<UCanvasPanelSlot>(MiniMapImageSizeBox->Slot))
			{
				MiniMapImageSizeBoxSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
				MiniMapImageSizeBoxSlot->SetAlignment(FVector2D(0.5f, 0.5f));
				MiniMapImageSizeBoxSlot->SetPosition(FVector2D(0.0f, 0.0f));
				MiniMapImageSizeBoxSlot->SetSize(FVector2D(MiniMapInnerLength, MiniMapInnerLength));
			}
		}

		if (MiniMapImage)
		{
			MiniMapImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			MiniMapImage->SetDesiredSizeOverride(FVector2D(MiniMapInnerLength, MiniMapInnerLength));
		}
	}

	auto ApplyButtonLayout = [ButtonLength, IconLength](USizeBox* ButtonSizeBox, UImage* ButtonImage)
	{
		if (ButtonSizeBox)
		{
			ButtonSizeBox->SetWidthOverride(ButtonLength);
			ButtonSizeBox->SetHeightOverride(ButtonLength);
			ButtonSizeBox->SetMinDesiredWidth(ButtonLength);
			ButtonSizeBox->SetMinDesiredHeight(ButtonLength);
			ButtonSizeBox->SetMaxDesiredWidth(ButtonLength);
			ButtonSizeBox->SetMaxDesiredHeight(ButtonLength);
		}

		if (ButtonImage)
		{
			ButtonImage->SetDesiredSizeOverride(FVector2D(IconLength, IconLength));
		}
	};

	auto ApplyButtonSlot = [ButtonLength, ButtonGap](USizeBox* ButtonSizeBox, int32 ButtonIndex)
	{
		if (ButtonSizeBox)
		{
			if (UCanvasPanelSlot* ButtonSlot = Cast<UCanvasPanelSlot>(ButtonSizeBox->Slot))
			{
				const float PositionX = static_cast<float>(ButtonIndex) * (ButtonLength + ButtonGap);
				ButtonSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
				ButtonSlot->SetAlignment(FVector2D(0.0f, 0.0f));
				ButtonSlot->SetPosition(FVector2D(PositionX, 0.0f));
				ButtonSlot->SetSize(FVector2D(ButtonLength, ButtonLength));
			}
		}
	};

	auto ApplyRightButtonSlot = [ButtonLength, ButtonGap](USizeBox* ButtonSizeBox, int32 ButtonIndexFromRight)
	{
		if (ButtonSizeBox)
		{
			if (UCanvasPanelSlot* ButtonSlot = Cast<UCanvasPanelSlot>(ButtonSizeBox->Slot))
			{
				const float PositionX = -ButtonLength - (static_cast<float>(ButtonIndexFromRight) * (ButtonLength + ButtonGap));
				ButtonSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
				ButtonSlot->SetAlignment(FVector2D(0.0f, 0.0f));
				ButtonSlot->SetPosition(FVector2D(PositionX, 0.0f));
				ButtonSlot->SetSize(FVector2D(ButtonLength, ButtonLength));
			}
		}
	};

	ApplyButtonLayout(PauseButtonSizeBox, PauseButtonImage);
	ApplyButtonLayout(PlayButtonSizeBox, PlayButtonImage);
	ApplyButtonLayout(FastForwardButtonSizeBox, FastForwardButtonImage);
	ApplyButtonLayout(FlaskButtonSizeBox, FlaskButtonImage);
	ApplyButtonLayout(RouteButtonSizeBox, RouteButtonImage);
	ApplyButtonLayout(StatsButtonSizeBox, StatsButtonImage);
	ApplyButtonLayout(HelpButtonSizeBox, HelpButtonImage);
	ApplyButtonLayout(CodexButtonSizeBox, CodexButtonImage);
	ApplyButtonLayout(SettingsButtonSizeBox, SettingsButtonImage);

	ApplyButtonSlot(PauseButtonSizeBox, 0);
	ApplyButtonSlot(PlayButtonSizeBox, 1);
	ApplyButtonSlot(FastForwardButtonSizeBox, 2);
	ApplyRightButtonSlot(SettingsButtonSizeBox, 0);
	ApplyRightButtonSlot(CodexButtonSizeBox, 1);
	ApplyRightButtonSlot(HelpButtonSizeBox, 2);
	ApplyRightButtonSlot(StatsButtonSizeBox, 3);
	ApplyRightButtonSlot(RouteButtonSizeBox, 4);
	ApplyRightButtonSlot(FlaskButtonSizeBox, 5);
}

USRTimeControlSubsystem* USRTimeControlWidget::GetTimeControlSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USRTimeControlSubsystem>() : nullptr;
}

void USRTimeControlWidget::UpdateButtonStyle(UButton* Button, bool bIsActive) const
{
	if (!Button)
	{
		return;
	}

	Button->SetBackgroundColor(
		bIsActive
			? FLinearColor(0.15f, 0.42f, 0.30f, 1.0f)
			: FLinearColor(0.12f, 0.13f, 0.15f, 0.92f)
	);
}

bool USRTimeControlWidget::IsScreenPositionOverTimeControlPanel(const FVector2D& ScreenPosition) const
{
	return IsVisible()
		&& TopBarContainerBorder
		&& TopBarContainerBorder->GetCachedGeometry().IsUnderLocation(ScreenPosition);
}

void USRTimeControlWidget::HandlePauseClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: TimeControl PauseButton OnClicked"));

	if (USRTimeControlSubsystem* TimeControlSubsystem = GetTimeControlSubsystem())
	{
		TimeControlSubsystem->PauseSimulation();
	}

	RefreshTimeControlState();
}

void USRTimeControlWidget::HandlePlayClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: TimeControl PlayButton OnClicked"));

	if (USRTimeControlSubsystem* TimeControlSubsystem = GetTimeControlSubsystem())
	{
		TimeControlSubsystem->ResumeSimulation();
	}

	RefreshTimeControlState();
}

void USRTimeControlWidget::HandleFastForwardClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: TimeControl FastForwardButton OnClicked"));

	if (USRTimeControlSubsystem* TimeControlSubsystem = GetTimeControlSubsystem())
	{
		TimeControlSubsystem->SetSimulationSpeedPreset(FMath::Max(0.0f, FastForwardTimeScale));
	}

	RefreshTimeControlState();
}
