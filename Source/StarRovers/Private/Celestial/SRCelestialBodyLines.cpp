#include "Celestial/SRCelestialBody.h"

#include "Gravity/SRGravityParent.h"

void ASRCelestialBody::RefreshRotationAxisLineVisual()
{
}

void ASRCelestialBody::ApplyGravityLineSettings()
{
	if (!IsValid(GravityParent))
	{
		return;
	}

	GravityParent->ConfigureGravity(
		Mass,
		GravityRatio,
		GravityRadiusRatio,
		ShowGravityLine,
		GravityLineColor,
		GravityLineOpacity,
		GravityLineSegments,
		GravityLineThickness);
}
