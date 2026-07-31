#include "SRFacilityRunModifierResolver.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Simulation/SRRunModifierSubsystem.h"

namespace
{
	ESRRunModifierFacilityScope ResolveFacilityScope(const USRFacilityDataAsset* FacilityDataAsset)
	{
		if (!IsValid(FacilityDataAsset))
		{
			return ESRRunModifierFacilityScope::Any;
		}
		switch (FacilityDataAsset->OperationKind)
		{
		case ESRFacilityOperationKind::Process:
			return ESRRunModifierFacilityScope::Transform;
		case ESRFacilityOperationKind::Synthesize:
			return ESRRunModifierFacilityScope::Synthesis;
		case ESRFacilityOperationKind::Separate:
			return ESRRunModifierFacilityScope::Separation;
		case ESRFacilityOperationKind::Mine:
			return ESRRunModifierFacilityScope::Mining;
		default:
			return ESRRunModifierFacilityScope::Any;
		}
	}

	ESRGlyphType ResolveDominantGlyph(const TArray<FSRResourceInstance>& InputResources)
	{
		ESRGlyphType DominantGlyph = ESRGlyphType::Empty;
		int32 DominantCount = 0;
		for (int32 GlyphValue = static_cast<int32>(ESRGlyphType::Metal);
			GlyphValue <= static_cast<int32>(ESRGlyphType::Plasma);
			++GlyphValue)
		{
			const ESRGlyphType Glyph = static_cast<ESRGlyphType>(GlyphValue);
			int32 GlyphCount = 0;
			for (const FSRResourceInstance& ResourceInstance : InputResources)
			{
				if (ResourceInstance.Pattern.IsCanonical())
				{
					GlyphCount += ResourceInstance.Pattern.CountGlyph(Glyph);
				}
			}
			// Enum order is the deterministic tie-break because only strict gains win.
			if (GlyphCount > DominantCount)
			{
				DominantCount = GlyphCount;
				DominantGlyph = Glyph;
			}
		}
		return DominantGlyph;
	}
}

void FSRFacilityRunModifierResolver::SnapshotCurrentContext(
	const UObject* WorldContextObject,
	FSRFacilityInstance& FacilityInstance)
{
	FacilityInstance.RunModifierContext = USRRunModifierSubsystem::GetContextForObject(WorldContextObject);
}

FSRRunModifierQuery FSRFacilityRunModifierResolver::BuildQuery(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& InputResources)
{
	FSRRunModifierQuery Query;
	Query.FacilityScope = ResolveFacilityScope(FacilityInstance.FacilityDataAsset.Get());
	Query.DominantGlyph = ResolveDominantGlyph(InputResources);
	return Query;
}

FSRResolvedRunModifiers FSRFacilityRunModifierResolver::Resolve(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& InputResources)
{
	return FSRRunModifierResolver::Resolve(
		FacilityInstance.RunModifierContext,
		BuildQuery(FacilityInstance, InputResources));
}

FSRPatternEnvironmentSpec FSRFacilityRunModifierResolver::ResolveEnvironmentSpec(
	const FSRPatternEnvironmentSpec& BaseEnvironment,
	int32 IntensityDelta)
{
	FSRPatternEnvironmentSpec Result = BaseEnvironment;
	for (int32 EffectIndex = Result.Effects.Num() - 1; EffectIndex >= 0; --EffectIndex)
	{
		FSRPatternEnvironmentEffectSpec& Effect = Result.Effects[EffectIndex];
		int32* Intensity = nullptr;
		switch (Effect.EffectKind)
		{
		case ESRPatternEnvironmentEffectKind::DirectionalPull:
			Intensity = &Effect.Distance;
			break;
		case ESRPatternEnvironmentEffectKind::ContinuousDrift:
			Intensity = &Effect.MaxDriftSteps;
			break;
		case ESRPatternEnvironmentEffectKind::OrganicBloom:
			Intensity = &Effect.OrganicGrowthsPerComponent;
			break;
		default:
			break;
		}
		if (!Intensity)
		{
			continue;
		}
		const int32 AdjustedIntensity = *Intensity + IntensityDelta;
		if (AdjustedIntensity <= 0)
		{
			Result.Effects.RemoveAt(EffectIndex, 1, EAllowShrinking::No);
		}
		else
		{
			*Intensity = FMath::Clamp(AdjustedIntensity, 1, StarRovers::Pattern::GridSize - 1);
		}
	}
	return Result;
}
