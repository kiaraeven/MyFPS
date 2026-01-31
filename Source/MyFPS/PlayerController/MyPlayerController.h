#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MyPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class MYFPS_API AMyPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	void SetHUDHealth(float Health, float MaxHealth);
	void SetHUDScore(float Score);
	void SetHUDDefeats(int32 Defeats);
	void SetHUDWeaponAmmo(int32 Ammo); // the number of ammo in the weapon
	void SetHUDCarriedAmmo(int32 Ammo); // the number of ammo carried by the character

	virtual void OnPossess(APawn* InPawn) override;
protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	 class AMyHUD* CharacterHUD;
};
