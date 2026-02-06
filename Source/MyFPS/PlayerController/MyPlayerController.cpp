

#include "MyPlayerController.h"
#include "MyFPS/HUD/MyHUD.h"
#include "MyFPS/HUD/CharacterOverlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "MyFPS/Character/MyCharacter.h"
#include "Net/UnrealNetwork.h"
#include "MyFPS/GameMode/MyGameMode.h"
#include "MyFPS/PlayerState/MyPlayerState.h"
#include "MyFPS/HUD/Announcement.h"
#include "Kismet/GameplayStatics.h"
#include "MyFPS/Components/CombatComponent.h"
#include "MyFPS/GameState/MyGameState.h"
#include "Components/Image.h"

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
	else
	{
		bInitializeCharacterOverlay = true;
		HUDHealth = Health;
		HUDMaxHealth = MaxHealth;
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
	else
	{
		bInitializeScore = true;
		HUDScore = Score;
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
	else
	{
		bInitializeDefeats = true;
		HUDDefeats = Defeats;
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
	else
	{
		bInitializeWeaponAmmo = true;
		HUDWeaponAmmo = Ammo;
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
	else
	{
		bInitializeWeaponAmmo = true;
		HUDWeaponAmmo = Ammo;
	}
}

void AMyPlayerController::SetHUDMatchCountdown(float CountdownTime)
{
	CharacterHUD = CharacterHUD == nullptr ? Cast<AMyHUD>(GetHUD()) : CharacterHUD;
	bool bHUDValid = CharacterHUD && CharacterHUD->CharacterOverlay && CharacterHUD->CharacterOverlay->MatchCountdownText;
	if (bHUDValid) {
		if (CountdownTime < 0.f) {
			CharacterHUD->Announcement->WarmupTime->SetText(FText());
			return;
		}
		int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
		int32 Seconds = CountdownTime - Minutes * 60;
		FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		CharacterHUD->CharacterOverlay->MatchCountdownText->SetText(FText::FromString(CountdownText));
	}
}

void AMyPlayerController::SetHUDAnnouncementCountdown(float CountdownTime)
{
	CharacterHUD = CharacterHUD == nullptr ? Cast<AMyHUD>(GetHUD()) : CharacterHUD;
	bool bHUDValid = CharacterHUD && CharacterHUD->Announcement && CharacterHUD->Announcement->WarmupTime;
	if (bHUDValid) {
		if (CountdownTime < 0.f) {
			CharacterHUD->Announcement->WarmupTime->SetText(FText());
			return;
		}
		int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
		int32 Seconds = CountdownTime - Minutes * 60;
		FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		CharacterHUD->Announcement->WarmupTime->SetText(FText::FromString(CountdownText));
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

void AMyPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	SetHUDTime();

	CheckTimeSync(DeltaTime);

	PollInit();

	CheckPing(DeltaTime);
}

void AMyPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMyPlayerController, MatchState);
}

float AMyPlayerController::GetServerTime()
{
	if (HasAuthority()) return GetWorld()->GetTimeSeconds();
	else return GetWorld()->GetTimeSeconds() + ClientServerDelta;
}

void AMyPlayerController::ReceivedPlayer()
{
	// ReceivedPlayer is the earliest we can get the time from server.
	Super::ReceivedPlayer();
	if (IsLocalController())
	{
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
	}
}

void AMyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	CharacterHUD = Cast<AMyHUD>(GetHUD());

	ServerCheckMatchState();

}

void AMyPlayerController::SetHUDTime()
{
	// Update countdown hud every second instead of every frame
	// so only update when CountdownInt != SecondsLeft
	float TimeLeft = 0.f;
	if (MatchState == MatchState::WaitingToStart) TimeLeft = WarmupTime - GetServerTime() + LevelStartingTime;
	else if (MatchState == MatchState::InProgress) TimeLeft = WarmupTime + MatchTime - GetServerTime() + LevelStartingTime;
	else if (MatchState == MatchState::Cooldown) TimeLeft = CooldownTime + WarmupTime + MatchTime - GetServerTime() + LevelStartingTime;

	uint32 SecondsLeft = FMath::CeilToInt(TimeLeft);

	if (HasAuthority()) {
		CharacterGameMode = CharacterGameMode == nullptr ? Cast<AMyGameMode>(UGameplayStatics::GetGameMode(this)) : CharacterGameMode;
		if (CharacterGameMode) {
			SecondsLeft = FMath::CeilToInt(CharacterGameMode->GetCountdownTime() + LevelStartingTime);
		}
	}

	if (CountdownInt != SecondsLeft)
	{
		if (MatchState == MatchState::WaitingToStart || MatchState == MatchState::Cooldown)
		{
			SetHUDAnnouncementCountdown(TimeLeft);
		}
		if (MatchState == MatchState::InProgress)
		{
			SetHUDMatchCountdown(TimeLeft);
		}
	}

	CountdownInt = SecondsLeft;
}

void AMyPlayerController::CheckTimeSync(float DeltaTime)
{
	TimeSyncRunningTime += DeltaTime;
	if (IsLocalController() && TimeSyncRunningTime > TimeSyncFrequency)
	{
		ServerRequestServerTime(GetWorld()->GetTimeSeconds()); // recalculate ClientServerDelta
		TimeSyncRunningTime = 0.f;
	}
}

void AMyPlayerController::HighPingWarning()
{
	CharacterHUD = CharacterHUD == nullptr ? Cast<AMyHUD>(GetHUD()) : CharacterHUD;
	bool bHUDValid = CharacterHUD &&
		CharacterHUD->CharacterOverlay &&
		CharacterHUD->CharacterOverlay->HighPingImage &&
		CharacterHUD->CharacterOverlay->HighPingAnimation;
	if (bHUDValid)
	{
		CharacterHUD->CharacterOverlay->HighPingImage->SetOpacity(1.f);
		CharacterHUD->CharacterOverlay->PlayAnimation(
			CharacterHUD->CharacterOverlay->HighPingAnimation,
			0.f,
			5);
	}
}

void AMyPlayerController::StopHighPingWarning()
{
	CharacterHUD = CharacterHUD == nullptr ? Cast<AMyHUD>(GetHUD()) : CharacterHUD;
	bool bHUDValid = CharacterHUD &&
		CharacterHUD->CharacterOverlay &&
		CharacterHUD->CharacterOverlay->HighPingImage &&
		CharacterHUD->CharacterOverlay->HighPingAnimation;
	if (bHUDValid)
	{
		CharacterHUD->CharacterOverlay->HighPingImage->SetOpacity(0.f);
		if (CharacterHUD->CharacterOverlay->IsAnimationPlaying(CharacterHUD->CharacterOverlay->HighPingAnimation))
		{
			CharacterHUD->CharacterOverlay->StopAnimation(CharacterHUD->CharacterOverlay->HighPingAnimation);
		}
	}
}

void AMyPlayerController::CheckPing(float DeltaTime)
{
	if (HasAuthority()) return;
	HighPingRunningTime += DeltaTime;
	if (HighPingRunningTime > CheckPingFrequency)
	{
		//PlayerState = PlayerState == nullptr ? GetPlayerState<APlayerState>() : PlayerState;
		if (PlayerState == nullptr)
		{
			PlayerState = GetPlayerState<APlayerState>();
		}
		if (PlayerState)
		{
			if (PlayerState->GetPingInMilliseconds() > HighPingThreshold) 
			{
				HighPingWarning();
				PingAnimationRunningTime = 0.f;
				ServerReportPingStatus(true);
			}
			else
			{
				ServerReportPingStatus(false);
			}
		}
		HighPingRunningTime = 0.f;
	}
	bool bHighPingAnimationPlaying =
		CharacterHUD && CharacterHUD->CharacterOverlay &&
		CharacterHUD->CharacterOverlay->HighPingAnimation &&
		CharacterHUD->CharacterOverlay->IsAnimationPlaying(CharacterHUD->CharacterOverlay->HighPingAnimation);
	if (bHighPingAnimationPlaying)
	{
		PingAnimationRunningTime += DeltaTime;
		if (PingAnimationRunningTime > HighPingDuration)
		{
			StopHighPingWarning();
		}
	}
}

void AMyPlayerController::ServerReportPingStatus_Implementation(bool bHighPing)
{
	HighPingDelegate.Broadcast(bHighPing);
}

void AMyPlayerController::ClientJoinMidgame_Implementation(FName StateOfMatch, float Warmup, float Match, float Cooldown, float StartingTime)
{
	// pass params at the same time when client join mid game instead of replicate all properties
	WarmupTime = Warmup;
	MatchTime = Match;
	CooldownTime = Cooldown;
	LevelStartingTime = StartingTime;
	MatchState = StateOfMatch;
	OnMatchStateSet(MatchState);
	if (CharacterHUD && MatchState == MatchState::WaitingToStart)
	{
		CharacterHUD->AddAnnouncement();
	}
}

void AMyPlayerController::ServerCheckMatchState_Implementation()
{
	AMyGameMode* GameMode = Cast<AMyGameMode>(UGameplayStatics::GetGameMode(this));
	if (GameMode)
	{
		WarmupTime = GameMode->WarmupTime;
		MatchTime = GameMode->MatchTime;
		CooldownTime = GameMode->CooldownTime;
		LevelStartingTime = GameMode->LevelStartingTime;
		MatchState = GameMode->GetMatchState();
		ClientJoinMidgame(MatchState, WarmupTime, MatchTime, CooldownTime, LevelStartingTime);
	}
}

void AMyPlayerController::ClientReportServerTime_Implementation(float TimeOfClientRequest, float TimeServerReceivedClientRequest)
{
	// This func is called on server and executed on client.
	// RoundTripTime - time taken for data to travel across the network (client -> server -> client)
	// assume that it take the same time from client to server and from server to client
	float RoundTripTime = GetWorld()->GetTimeSeconds() - TimeOfClientRequest; 
	SingleTripTime = 0.5f * RoundTripTime;
	float CurrentServerTime = TimeServerReceivedClientRequest + SingleTripTime;
	ClientServerDelta = CurrentServerTime - GetWorld()->GetTimeSeconds(); // difference between client and server time
}

void AMyPlayerController::ServerRequestServerTime_Implementation(float TimeOfClientRequest)
{
	// This func is called on client and executed on server.
	// TimeOfClientRequest - the client's time when the request was sent
	// ServerTimeOfReceipt - current server time
	float ServerTimeOfReceipt = GetWorld()->GetTimeSeconds();
	ClientReportServerTime(TimeOfClientRequest, ServerTimeOfReceipt);
}

void AMyPlayerController::PollInit()
{
	if (CharacterOverlay == nullptr)
	{
		if (CharacterHUD && CharacterHUD->CharacterOverlay)
		{
			CharacterOverlay = CharacterHUD->CharacterOverlay;
			if (CharacterOverlay)
			{
				if (bInitializeScore) SetHUDScore(HUDScore);
				if (bInitializeDefeats) SetHUDDefeats(HUDDefeats);
				if (bInitializeCarriedAmmo) SetHUDCarriedAmmo(HUDCarriedAmmo);
				if (bInitializeWeaponAmmo) SetHUDWeaponAmmo(HUDWeaponAmmo);
			}
		}
	}
}

void AMyPlayerController::OnMatchStateSet(FName State)
{
	MatchState = State;

	if (MatchState == MatchState::InProgress)
	{
		HandleMatchHasStarted();
	}
	else if (MatchState == MatchState::Cooldown)
	{
		HandleCooldown();
	}
}

void AMyPlayerController::OnRep_MatchState()
{
	if (MatchState == MatchState::InProgress)
	{
		HandleMatchHasStarted();
	}
	else if (MatchState == MatchState::Cooldown)
	{
		HandleCooldown();
	}
}

void AMyPlayerController::HandleMatchHasStarted()
{
	CharacterHUD = CharacterHUD == nullptr ? Cast<AMyHUD>(GetHUD()) : CharacterHUD;
	if (CharacterHUD)
	{
		CharacterHUD->AddCharacterOverlay();
		if (CharacterHUD->Announcement)
		{
			CharacterHUD->Announcement->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void AMyPlayerController::HandleCooldown()
{
	CharacterHUD = CharacterHUD == nullptr ? Cast<AMyHUD>(GetHUD()) : CharacterHUD;
	if (CharacterHUD)
	{
		CharacterHUD->CharacterOverlay->RemoveFromParent();
		bool bHUDValid = CharacterHUD->Announcement &&
			CharacterHUD->Announcement->AnnouncementText &&
			CharacterHUD->Announcement->InfoText;

		if (bHUDValid)
		{
			CharacterHUD->Announcement->SetVisibility(ESlateVisibility::Visible);
			FString AnnouncementText("New game starts in:");
			CharacterHUD->Announcement->AnnouncementText->SetText(FText::FromString(AnnouncementText));

			AMyGameState* MyGameState = Cast<AMyGameState>(UGameplayStatics::GetGameState(this));
			AMyPlayerState* MyPlayerState = GetPlayerState<AMyPlayerState>();
			if (MyGameState && MyPlayerState)
			{
				TArray<AMyPlayerState*> TopPlayers = MyGameState->TopScoringPlayers;
				FString InfoTextString;
				if (TopPlayers.Num() == 0)
				{
					InfoTextString = FString("There is no winner.");
				}
				else if (TopPlayers.Num() == 1 && TopPlayers[0] == MyPlayerState)
				{
					InfoTextString = FString("You are the winner!");
				}
				else if (TopPlayers.Num() == 1)
				{
					InfoTextString = FString::Printf(TEXT("Winner: \n%s"), *TopPlayers[0]->GetPlayerName());
				}
				else if (TopPlayers.Num() > 1)
				{
					InfoTextString = FString("Players tied for the win:\n");
					for (auto TiedPlayer : TopPlayers)
					{
						InfoTextString.Append(FString::Printf(TEXT("%s\n"), *TiedPlayer->GetPlayerName()));
					}
				}

				CharacterHUD->Announcement->InfoText->SetText(FText::FromString(InfoTextString));
			}
		}
	}
	AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetPawn());
	if (MyCharacter && MyCharacter->GetCombat())
	{
		MyCharacter->bDisableGameplay = true;
		MyCharacter->GetCombat()->FireButtonPressed(false);
	}
}
