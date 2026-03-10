#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "RDGPostProcessSettingsProvider.generated.h"

class APostProcessVolume;
class URDGPostProcessSettingsAsset;

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

	bool ResolveSettings(bool& OutEnabled, float& OutRadius) const;
};
