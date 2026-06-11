#include "Celestial/SRDynamicMeshBaseDataAsset.h"

int32 USRDynamicMeshBaseDataAsset::GetClampedFaceResolution() const
{
	return FMath::Clamp(FaceResolution, 1, 512);
}

float USRDynamicMeshBaseDataAsset::GetSafeBaseRadius(float FallbackRadius) const
{
	return FMath::Max(1.0f, BaseRadius > KINDA_SMALL_NUMBER ? BaseRadius : FallbackRadius);
}
