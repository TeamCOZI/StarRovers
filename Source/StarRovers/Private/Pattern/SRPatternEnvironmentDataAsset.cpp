#include "Pattern/SRPatternEnvironmentDataAsset.h"

USRPatternEnvironmentDataAsset::USRPatternEnvironmentDataAsset()
{
	EnvironmentId = FName(TEXT("Neutral"));
	DisplayName = FText::FromString(TEXT("Neutral"));
	Description = FText::FromString(TEXT("Does not alter a Pattern after facility processing."));
}

FSRPatternEnvironmentSpec USRPatternEnvironmentDataAsset::BuildEnvironmentSpec() const
{
	FSRPatternEnvironmentSpec Result;
	Result.EnvironmentId = EnvironmentId;
	Result.Effects = Effects;
	Result.Normalize();
	return Result;
}

bool USRPatternEnvironmentDataAsset::IsEnvironmentValid() const
{
	return !EnvironmentId.IsNone()
		&& FSRPatternEnvironmentResolver::IsValidEnvironmentSpec(BuildEnvironmentSpec());
}
