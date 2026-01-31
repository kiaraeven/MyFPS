
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "MyPlayerState.generated.h"

/**
 * 
 */
UCLASS()
class MYFPS_API AMyPlayerState : public APlayerState
{
	GENERATED_BODY()
	
public:
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const override;
	virtual void OnRep_Score() override;
	UFUNCTION()
	virtual void OnRep_Defeats();
	void AddToScore(float ScoreAmount); // be called only on server
	void AddToDefeats(int32 DefeatsAmount);
private:
	UPROPERTY()
	class AMyCharacter* Character;
	UPROPERTY()
	class AMyPlayerController* Controller;
	UPROPERTY(ReplicatedUsing = OnRep_Defeats)
	int32 Defeats;
};
