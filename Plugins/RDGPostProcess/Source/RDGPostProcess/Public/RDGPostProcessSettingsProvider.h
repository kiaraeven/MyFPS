#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RDGPostProcessSettingsAsset.h"

#include "RDGPostProcessSettingsProvider.generated.h"

class APostProcessVolume;
class URDGPostProcessSettingsAsset;

struct FRDGResolvedPostProcessSettings
{
	bool bEnabled = false;
	float Radius = 0.0f;
	bool bOutlineEnabled = false;
	bool bGlitchEnabled = false;
	FOutlineSettings OutlineSettings;
};

UCLASS(BlueprintType, Blueprintable)
class RDGPOSTPROCESS_API ARDGPostProcessSettingsProvider : public AActor
{
	GENERATED_BODY()

public:
	ARDGPostProcessSettingsProvider();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "RDG Post Process")
	TObjectPtr<APostProcessVolume> TargetVolume = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RDG Post Process")
	TObjectPtr<URDGPostProcessSettingsAsset> SettingsAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RDG Post Process")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RDG Post Process", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "32.0"))
	float Radius = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RDG Post Process|Outline")
	bool bEnableOutline = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RDG Post Process|Glitch")
	bool bEnableGlitch = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RDG Post Process|Outline", meta = (ShowOnlyInnerProperties))
	FOutlineSettings OutlineSettings;

	bool ResolveSettings(FRDGResolvedPostProcessSettings& OutSettings) const;
};
