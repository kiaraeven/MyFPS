// Fill out your copyright notice in the Description page of Project Settings.


#include "MyGameMode.h"
#include "MyFPS/Character/MyCharacter.h"
#include "MyFPS/PlayerController/MyPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "MyFPS/PlayerState/MyPlayerState.h"

void AMyGameMode::PlayerEliminated(AMyCharacter* ElimmedCharacter, AMyPlayerController* VictimController, AMyPlayerController* AttackerController)
{
	AMyPlayerState* AttackerPlayerState = AttackerController ? Cast<AMyPlayerState>(AttackerController->PlayerState) : nullptr;
	AMyPlayerState* VictimPlayerState = VictimController ? Cast<AMyPlayerState>(VictimController->PlayerState) : nullptr;

	if (AttackerPlayerState && AttackerPlayerState != VictimPlayerState) {
		AttackerPlayerState->AddToScore(1.f);
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
