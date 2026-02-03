// Fill out your copyright notice in the Description page of Project Settings.


#include "MyGameMode.h"
#include "MyFPS/Character/MyCharacter.h"
#include "MyFPS/PlayerController/MyPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "MyFPS/PlayerState/MyPlayerState.h"
#include "MyFPS/GameState/MyGameState.h"

// Customized MatchState
namespace MatchState
{
	const FName Cooldown = FName("Cooldown");
}

AMyGameMode::AMyGameMode()
{
	bDelayedStart = true;
}

void AMyGameMode::BeginPlay()
{
	Super::BeginPlay();

	LevelStartingTime = GetWorld()->GetTimeSeconds();
}

void AMyGameMode::OnMatchStateSet()
{
	Super::OnMatchStateSet();

	// Iterate all player controllers to call their OnMatchStateSet
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AMyPlayerController* MyPlayer = Cast<AMyPlayerController>(*It);
		if (MyPlayer)
		{
			MyPlayer->OnMatchStateSet(MatchState);
		}
	}
}

void AMyGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (MatchState == MatchState::WaitingToStart)
	{
		CountdownTime = WarmupTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
		if (CountdownTime <= 0.f)
		{
			StartMatch();
		}
	}
	else if (MatchState == MatchState::InProgress)
	{
		CountdownTime = WarmupTime + MatchTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
		if (CountdownTime <= 0.f)
		{
			SetMatchState(MatchState::Cooldown);
		}
	}
	else if (MatchState == MatchState::Cooldown)
	{
		CountdownTime = CooldownTime + WarmupTime + MatchTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
		if (CountdownTime <= 0.f)
		{
			RestartGame();
		}
	}
}

void AMyGameMode::PlayerEliminated(AMyCharacter* ElimmedCharacter, AMyPlayerController* VictimController, AMyPlayerController* AttackerController)
{
	AMyPlayerState* AttackerPlayerState = AttackerController ? Cast<AMyPlayerState>(AttackerController->PlayerState) : nullptr;
	AMyPlayerState* VictimPlayerState = VictimController ? Cast<AMyPlayerState>(VictimController->PlayerState) : nullptr;
	AMyGameState* MyGameState = GetGameState<AMyGameState>();

	if (AttackerPlayerState && AttackerPlayerState != VictimPlayerState && MyGameState) {
		AttackerPlayerState->AddToScore(1.f);
		MyGameState->UpdateTopScore(AttackerPlayerState);
	}
	if (VictimPlayerState) {
		VictimPlayerState->AddToDefeats(1);
	}

	if (ElimmedCharacter)
	{
		ElimmedCharacter->Elim();
	}
}

void AMyGameMode::RequestRespawn(ACharacter* ElimmedCharacter, AController* ElimmedController)
{
	if (ElimmedCharacter)
	{
		ElimmedCharacter->Reset(); // Reset before destroying to avoid issues with lingering references
		ElimmedCharacter->Destroy();
	}
	if (ElimmedController)
	{
		TArray<AActor*> PlayerStarts; // Get all PlayerStart actors in the level
		UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);
		int32 Selection = FMath::RandRange(0, PlayerStarts.Num() - 1);
		RestartPlayerAtPlayerStart(ElimmedController, PlayerStarts[Selection]); // Respawn the player at a random PlayerStart
	}
}


