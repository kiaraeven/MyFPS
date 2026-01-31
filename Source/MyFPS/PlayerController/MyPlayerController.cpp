

#include "MyPlayerController.h"
#include "MyFPS/HUD/MyHUD.h"
#include "MyFPS/HUD/CharacterOverlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "MyFPS/Character/MyCharacter.h"


void AMyPlayerController::SetHUDHealth(float Health, float MaxHealth)
{
	CharacterHUD = CharacterHUD == nullptr ? Cast<AMyHUD>(GetHUD()) : CharacterHUD;

	bool bHUDValid = CharacterHUD &&
		CharacterHUD->CharacterOverlay &&
		CharacterHUD->CharacterOverlay->HealthBar &&
		CharacterHUD->CharacterOverlay->HealthText;
	if (bHUDValid)
	{
		const float HealthPercent = Health / MaxHealth;
		CharacterHUD->CharacterOverlay->HealthBar->SetPercent(HealthPercent);
		FString HealthText = FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(Health), FMath::CeilToInt(MaxHealth));
		CharacterHUD->CharacterOverlay->HealthText->SetText(FText::FromString(HealthText));
	}
}

void AMyPlayerController::SetHUDScore(float Score)
{
	CharacterHUD = CharacterHUD == nullptr ? Cast<AMyHUD>(GetHUD()) : CharacterHUD;
	bool bHUDValid = CharacterHUD && CharacterHUD->CharacterOverlay && CharacterHUD->CharacterOverlay->ScoreAmount;
	if (bHUDValid) {
		FString ScoreText = FString::Printf(TEXT("%d"), FMath::FloorToInt(Score));
		CharacterHUD->CharacterOverlay->ScoreAmount->SetText(FText::FromString(ScoreText));
	}
}

void AMyPlayerController::SetHUDDefeats(int32 Defeats)
{
	CharacterHUD = CharacterHUD == nullptr ? Cast<AMyHUD>(GetHUD()) : CharacterHUD;
	bool bHUDValid = CharacterHUD && CharacterHUD->CharacterOverlay && CharacterHUD->CharacterOverlay->DefeatsAmount;
	if (bHUDValid) {
		FString DefeatsText = FString::Printf(TEXT("%d"), Defeats);
		CharacterHUD->CharacterOverlay->DefeatsAmount->SetText(FText::FromString(DefeatsText));
	}
}

void AMyPlayerController::SetHUDWeaponAmmo(int32 Ammo)
{
	CharacterHUD = CharacterHUD == nullptr ? Cast<AMyHUD>(GetHUD()) : CharacterHUD;
	bool bHUDValid = CharacterHUD && CharacterHUD->CharacterOverlay && CharacterHUD->CharacterOverlay->WeaponAmmoAmount;
	if (bHUDValid) {
		FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		CharacterHUD->CharacterOverlay->WeaponAmmoAmount->SetText(FText::FromString(AmmoText));
	}
}

void AMyPlayerController::SetHUDCarriedAmmo(int32 Ammo)
{
	CharacterHUD = CharacterHUD == nullptr ? Cast<AMyHUD>(GetHUD()) : CharacterHUD;
	bool bHUDValid = CharacterHUD && CharacterHUD->CharacterOverlay && CharacterHUD->CharacterOverlay->CarriedAmmoAmount;
	if (bHUDValid) {
		FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		CharacterHUD->CharacterOverlay->CarriedAmmoAmount->SetText(FText::FromString(AmmoText));
	}
}

void AMyPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	AMyCharacter* MyCharacter = Cast<AMyCharacter>(InPawn);
	if (MyCharacter)
	{
		SetHUDHealth(MyCharacter->GetHealth(), MyCharacter->GetMaxHealth());
	}
}

void AMyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	CharacterHUD = Cast<AMyHUD>(GetHUD());
}
