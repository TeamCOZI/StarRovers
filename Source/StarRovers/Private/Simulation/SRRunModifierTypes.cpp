#include "Simulation/SRRunModifierTypes.h"

namespace
{
	bool NameLess(const FName Left, const FName Right)
	{
		return Left.ToString() < Right.ToString();
	}

	bool EffectLess(const FSRRunModifierEffect& Left, const FSRRunModifierEffect& Right)
	{
		if (Left.EffectKind != Right.EffectKind)
		{
			return Left.EffectKind < Right.EffectKind;
		}
		if (Left.FacilityScope != Right.FacilityScope)
		{
			return Left.FacilityScope < Right.FacilityScope;
		}
		if (Left.AffectedGlyph != Right.AffectedGlyph)
		{
			return Left.AffectedGlyph < Right.AffectedGlyph;
		}
		if (Left.ContractId != Right.ContractId)
		{
			return NameLess(Left.ContractId, Right.ContractId);
		}
		return NameLess(Left.EffectId, Right.EffectId);
	}

	bool SourceLess(const FSRRunModifierSource& Left, const FSRRunModifierSource& Right)
	{
		if (Left.SourceKind != Right.SourceKind)
		{
			return Left.SourceKind < Right.SourceKind;
		}
		if (Left.Priority != Right.Priority)
		{
			return Left.Priority < Right.Priority;
		}
		return NameLess(Left.SourceId, Right.SourceId);
	}

	bool DoesEffectMatch(const FSRRunModifierEffect& Effect, const FSRRunModifierQuery& Query)
	{
		return (Effect.FacilityScope == ESRRunModifierFacilityScope::Any
				|| Effect.FacilityScope == Query.FacilityScope)
			&& (Effect.AffectedGlyph == ESRGlyphType::Empty
				|| Effect.AffectedGlyph == Query.DominantGlyph)
			&& (Effect.ContractId.IsNone() || Effect.ContractId == Query.ContractId);
	}

	double StackedMultiplier(double Magnitude, int32 StackCount)
	{
		return FMath::Pow(Magnitude, static_cast<double>(FMath::Max(1, StackCount)));
	}
}

bool FSRRunModifierResolver::IsMultiplierEffect(ESRRunModifierEffectKind EffectKind)
{
	return EffectKind != ESRRunModifierEffectKind::TransformOrganicGrowthDelta
		&& EffectKind != ESRRunModifierEffectKind::EnvironmentIntensityDelta;
}

bool FSRRunModifierResolver::ValidateEffect(const FSRRunModifierEffect& Effect, FString& OutFailureReason)
{
	OutFailureReason.Reset();
	if (Effect.EffectId.IsNone())
	{
		OutFailureReason = TEXT("Run modifier effect ID must not be None.");
		return false;
	}
	if (static_cast<uint8>(Effect.EffectKind) > static_cast<uint8>(ESRRunModifierEffectKind::LogisticsTravelTimeMultiplier)
		|| static_cast<uint8>(Effect.FacilityScope) > static_cast<uint8>(ESRRunModifierFacilityScope::Mining)
		|| !StarRovers::Pattern::IsValidGlyph(Effect.AffectedGlyph))
	{
		OutFailureReason = FString::Printf(TEXT("Run modifier effect '%s' contains an invalid enum value."), *Effect.EffectId.ToString());
		return false;
	}
	if (!FMath::IsFinite(Effect.Magnitude))
	{
		OutFailureReason = FString::Printf(TEXT("Run modifier effect '%s' has a non-finite magnitude."), *Effect.EffectId.ToString());
		return false;
	}
	if (IsMultiplierEffect(Effect.EffectKind))
	{
		if (Effect.Magnitude < 0.01 || Effect.Magnitude > 10.0)
		{
			OutFailureReason = FString::Printf(TEXT("Multiplier effect '%s' must be in [0.01, 10]."), *Effect.EffectId.ToString());
			return false;
		}
	}
	else
	{
		const double RoundedMagnitude = FMath::RoundToDouble(Effect.Magnitude);
		if (!FMath::IsNearlyEqual(Effect.Magnitude, RoundedMagnitude)
			|| RoundedMagnitude < MinimumDelta
			|| RoundedMagnitude > MaximumDelta)
		{
			OutFailureReason = FString::Printf(TEXT("Delta effect '%s' must be an integer in [-4, 4]."), *Effect.EffectId.ToString());
			return false;
		}
	}
	return true;
}

bool FSRRunModifierResolver::ValidateSource(const FSRRunModifierSource& Source, FString& OutFailureReason)
{
	OutFailureReason.Reset();
	if (Source.SourceId.IsNone())
	{
		OutFailureReason = TEXT("Run modifier source ID must not be None.");
		return false;
	}
	if (static_cast<uint8>(Source.SourceKind) > static_cast<uint8>(ESRRunModifierSourceKind::Trial))
	{
		OutFailureReason = FString::Printf(TEXT("Run modifier source '%s' has an invalid source kind."), *Source.SourceId.ToString());
		return false;
	}
	if (Source.StackCount < 1 || Source.StackCount > MaximumSourceStacks)
	{
		OutFailureReason = FString::Printf(TEXT("Run modifier source '%s' stack count must be in [1, 16]."), *Source.SourceId.ToString());
		return false;
	}
	if (Source.Effects.IsEmpty())
	{
		OutFailureReason = FString::Printf(TEXT("Run modifier source '%s' has no effects."), *Source.SourceId.ToString());
		return false;
	}

	TSet<FName> EffectIds;
	for (const FSRRunModifierEffect& Effect : Source.Effects)
	{
		if (!ValidateEffect(Effect, OutFailureReason))
		{
			return false;
		}
		bool bAlreadyPresent = false;
		EffectIds.Add(Effect.EffectId, &bAlreadyPresent);
		if (bAlreadyPresent)
		{
			OutFailureReason = FString::Printf(TEXT("Run modifier source '%s' has duplicate effect ID '%s'."), *Source.SourceId.ToString(), *Effect.EffectId.ToString());
			return false;
		}
	}
	return true;
}

bool FSRRunModifierResolver::BuildContext(
	const TArray<FSRRunModifierSource>& Sources,
	int32 Revision,
	FSRRunModifierContext& OutContext,
	FString& OutFailureReason)
{
	OutContext = FSRRunModifierContext();
	OutFailureReason.Reset();

	TSet<FString> SourceKeys;
	TArray<FSRRunModifierSource> CanonicalSources = Sources;
	for (FSRRunModifierSource& Source : CanonicalSources)
	{
		if (!ValidateSource(Source, OutFailureReason))
		{
			return false;
		}

		const FString SourceKey = FString::Printf(TEXT("%d:%s"), static_cast<int32>(Source.SourceKind), *Source.SourceId.ToString());
		bool bAlreadyPresent = false;
		SourceKeys.Add(SourceKey, &bAlreadyPresent);
		if (bAlreadyPresent)
		{
			OutFailureReason = FString::Printf(TEXT("Run modifier context has duplicate source '%s'."), *SourceKey);
			return false;
		}
		Source.Effects.Sort(EffectLess);
	}

	CanonicalSources.Sort(SourceLess);
	OutContext.Revision = FMath::Max(0, Revision);
	OutContext.ActiveSources = MoveTemp(CanonicalSources);
	return true;
}

FSRResolvedRunModifiers FSRRunModifierResolver::Resolve(
	const FSRRunModifierContext& Context,
	const FSRRunModifierQuery& Query)
{
	FSRResolvedRunModifiers Result;
	double OrganicGrowthDelta = 0.0;
	double EnvironmentIntensityDelta = 0.0;

	for (const FSRRunModifierSource& Source : Context.ActiveSources)
	{
		for (const FSRRunModifierEffect& Effect : Source.Effects)
		{
			if (!DoesEffectMatch(Effect, Query))
			{
				continue;
			}

			const double Scale = IsMultiplierEffect(Effect.EffectKind)
				? StackedMultiplier(Effect.Magnitude, Source.StackCount)
				: 1.0;
			const double Delta = Effect.Magnitude * static_cast<double>(Source.StackCount);
			switch (Effect.EffectKind)
			{
			case ESRRunModifierEffectKind::FacilityProcessTimeMultiplier:
				Result.FacilityProcessTimeMultiplier *= Scale;
				break;
			case ESRRunModifierEffectKind::TransformOrganicGrowthDelta:
				OrganicGrowthDelta += Delta;
				break;
			case ESRRunModifierEffectKind::EnvironmentIntensityDelta:
				EnvironmentIntensityDelta += Delta;
				break;
			case ESRRunModifierEffectKind::StellarBaseScoreMultiplier:
				Result.StellarBaseScoreMultiplier *= Scale;
				break;
			case ESRRunModifierEffectKind::StellarBonusScoreMultiplier:
				Result.StellarBonusScoreMultiplier *= Scale;
				break;
			case ESRRunModifierEffectKind::StellarRequiredScoreMultiplier:
				Result.StellarRequiredScoreMultiplier *= Scale;
				break;
			case ESRRunModifierEffectKind::StellarHealthDamageMultiplier:
				Result.StellarHealthDamageMultiplier *= Scale;
				break;
			case ESRRunModifierEffectKind::StellarHealthRecoveryMultiplier:
				Result.StellarHealthRecoveryMultiplier *= Scale;
				break;
			case ESRRunModifierEffectKind::LogisticsTravelTimeMultiplier:
				Result.LogisticsTravelTimeMultiplier *= Scale;
				break;
			default:
				break;
			}
		}
	}

	Result.FacilityProcessTimeMultiplier = FMath::Clamp(Result.FacilityProcessTimeMultiplier, MinimumGeneralMultiplier, MaximumGeneralMultiplier);
	Result.TransformOrganicGrowthDelta = FMath::Clamp(FMath::RoundToInt(OrganicGrowthDelta), MinimumDelta, MaximumDelta);
	Result.EnvironmentIntensityDelta = FMath::Clamp(FMath::RoundToInt(EnvironmentIntensityDelta), MinimumDelta, MaximumDelta);
	Result.StellarBaseScoreMultiplier = FMath::Clamp(Result.StellarBaseScoreMultiplier, MinimumGeneralMultiplier, MaximumGeneralMultiplier);
	Result.StellarBonusScoreMultiplier = FMath::Clamp(Result.StellarBonusScoreMultiplier, MinimumGeneralMultiplier, MaximumGeneralMultiplier);
	Result.StellarRequiredScoreMultiplier = FMath::Clamp(Result.StellarRequiredScoreMultiplier, MinimumRequiredScoreMultiplier, MaximumRequiredScoreMultiplier);
	Result.StellarHealthDamageMultiplier = FMath::Clamp(Result.StellarHealthDamageMultiplier, MinimumGeneralMultiplier, MaximumGeneralMultiplier);
	Result.StellarHealthRecoveryMultiplier = FMath::Clamp(Result.StellarHealthRecoveryMultiplier, MinimumGeneralMultiplier, MaximumGeneralMultiplier);
	Result.LogisticsTravelTimeMultiplier = FMath::Clamp(Result.LogisticsTravelTimeMultiplier, MinimumLogisticsTravelTimeMultiplier, MaximumLogisticsTravelTimeMultiplier);
	return Result;
}
