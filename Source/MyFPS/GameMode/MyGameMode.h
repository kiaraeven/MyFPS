// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "MyGameMode.generated.h"


namespace MatchState
{
	extern MYFPS_API const FName Cooldown; // Match duration has been reached. Display winner and begin cooldown timer.
}


/**
 * 
 */
UCLASS()
class MYFPS_API AMyGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	AMyGameMode();
	virtual void Tick(float DeltaTime) override;
	virtual void PlayerEliminated(class AMyCharacter* ElimmedCharacter, class AMyPlayerController* VictimController, AMyPlayerController* AttackerController);
	virtual void RequestRespawn(ACharacter* ElimmedCharacter, AController* ElimmedController);
	UPROPERTY(EditDefaultsOnly)
	float WarmupTime = 10.f;
	UPROPERTY(EditDefaultsOnly)
	float MatchTime = 120.f;
	UPROPERTY(EditDefaultsOnly)
	float CooldownTime = 10.f;
	float LevelStartingTime = 0.f; // how much time it takes from the game lauched to entering the current level

protected:
	virtual void BeginPlay() override;
	virtual void OnMatchStateSet() override;
private:
	float CountdownTime = 0.f; // time remaining to end WarmupTime

public:
	FORCEINLINE float GetCountdownTime() const { return CountdownTime; }
};
