#include "RDGPostProcessSettingsProvider.h"

#include "RDGPostProcessSettingsAsset.h"

ARDGPostProcessSettingsProvider::ARDGPostProcessSettingsProvider()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorHiddenInGame(true);
}

bool ARDGPostProcessSettingsProvider::ResolveSettings(bool& OutEnabled, float& OutRadius) const
{
	if (SettingsAsset != nullptr)
	{
		OutEnabled = SettingsAsset->bEnabled;
		OutRadius = SettingsAsset->Radius;
		return true;
	}

	OutEnabled = bEnabled;
	OutRadius = Radius;
	return true;
}
