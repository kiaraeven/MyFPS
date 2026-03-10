#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "RDGPostProcessSettingsAsset.generated.h"

UCLASS(BlueprintType)
class RDGPOSTPROCESS_API URDGPostProcessSettingsAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RDG Post Process")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RDG Post Process", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "32.0"))
	float Radius = 4.0f;
};
