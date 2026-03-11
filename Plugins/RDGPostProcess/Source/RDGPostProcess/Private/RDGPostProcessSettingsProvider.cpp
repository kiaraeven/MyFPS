#include "RDGPostProcessSettingsProvider.h"

ARDGPostProcessSettingsProvider::ARDGPostProcessSettingsProvider()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorHiddenInGame(true);
}

namespace
{
FRDGResolvedPostProcessSettings MakeResolvedSettings(
	const bool bInEnabled,
	const float InRadius,
	const bool bInOutlineEnabled,
	const bool bInGlitchEnabled,
	const FOutlineSettings& InOutlineSettings)
{
	FRDGResolvedPostProcessSettings Settings;
	Settings.bEnabled = bInEnabled;
	Settings.Radius = FMath::Max(InRadius, 0.0f);
	Settings.bOutlineEnabled = bInOutlineEnabled;
	Settings.bGlitchEnabled = bInGlitchEnabled;
	Settings.OutlineSettings = InOutlineSettings;
	Settings.OutlineSettings.EdgeThreshold = FMath::Max(Settings.OutlineSettings.EdgeThreshold, 0.0f);
	Settings.OutlineSettings.Softness = FMath::Clamp(Settings.OutlineSettings.Softness, 0.0f, 1.0f);
	Settings.OutlineSettings.GlowIntensity = FMath::Max(Settings.OutlineSettings.GlowIntensity, 0.0f);
	Settings.OutlineSettings.PulseSpeed = FMath::Max(Settings.OutlineSettings.PulseSpeed, 0.0f);
	Settings.OutlineSettings.GlitchIntensity = FMath::Clamp(Settings.OutlineSettings.GlitchIntensity, 0.0f, 1.0f);
	Settings.OutlineSettings.RGBShiftAmount = FMath::Max(Settings.OutlineSettings.RGBShiftAmount, 0.0f);
	Settings.OutlineSettings.BlockSliceAmount = FMath::Clamp(Settings.OutlineSettings.BlockSliceAmount, 0.0f, 1.0f);
	Settings.OutlineSettings.StaticNoiseLevel = FMath::Clamp(Settings.OutlineSettings.StaticNoiseLevel, 0.0f, 1.0f);
	return Settings;
}
}

bool ARDGPostProcessSettingsProvider::ResolveSettings(FRDGResolvedPostProcessSettings& OutSettings) const
{
	if (SettingsAsset != nullptr)
	{
		OutSettings = MakeResolvedSettings(
			SettingsAsset->bEnabled,
			SettingsAsset->Radius,
			SettingsAsset->bEnableOutline,
			SettingsAsset->bEnableGlitch,
			SettingsAsset->OutlineSettings);
		return true;
	}

	OutSettings = MakeResolvedSettings(bEnabled, Radius, bEnableOutline, bEnableGlitch, OutlineSettings);
	return true;
}
