// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "MyGameMode.generated.h"

/**
 * 
 */
UCLASS()
class MYFPS_API AMyGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	virtual void PlayerEliminated(class AMyCharacter* ElimmedCharacter, class AMyPlayerController* VictimController, AMyPlayerController* AttackerController);
	virtual void RequestRespawn(ACharacter* ElimmedCharacter, AController* ElimmedController);

};
