#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "RDGPostProcessSettingsAsset.generated.h"

USTRUCT(BlueprintType)
struct RDGPOSTPROCESS_API FOutlineSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RDG Post Process|Outline")
	FLinearColor OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RDG Post Process|Outline", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2.0"))
	float EdgeThreshold = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RDG Post Process|Outline", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Softness = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RDG Post Process|Outline", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "8.0"))
	float GlowIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RDG Post Process|Outline", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "16.0"))
	float PulseSpeed = 0.0f;
};

UCLASS(BlueprintType)
class RDGPOSTPROCESS_API URDGPostProcessSettingsAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RDG Post Process")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RDG Post Process", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "32.0"))
	float Radius = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RDG Post Process|Outline")
	bool bEnableOutline = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RDG Post Process|Outline", meta = (ShowOnlyInnerProperties))
	FOutlineSettings OutlineSettings;
};
