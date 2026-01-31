// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Menu.generated.h"


/**
 * 
 */
UCLASS()
class MULTIPLAYERSESSIONS_API UMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)	
	void MenuSetup(int32 NumberPublicConnections = 4, FString TypeofMatch = FString(TEXT("FreeForAll")), FString LobbyPath = FString(TEXT("/Game/FirstPerson/Maps/Lobby")));

protected:
	virtual bool Initialize() override;
	//virtual void OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld) override;
	virtual void NativeDestruct() override;
	UFUNCTION()
	void OnCreateSession(bool bWasSuccessful); // Callback for the delegate we'll bind to the multiplayer session subsystem
	void OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);
	void OnJoinSession(bool Result);
	UFUNCTION()
	void OnDestroySession(bool bWasSuccessful);
	UFUNCTION()
	void OnStartSession(bool bWasSuccessful);

private:
	UPROPERTY(meta = (BindWidget))
	class UButton* HostButton;
	UPROPERTY(meta = (BindWidget))
	UButton* JoinButton;

	UFUNCTION()
	void HostButtonClicked();
	UFUNCTION()
	void JoinButtonClicked();

	void MenuTearDown(); // Remove menu from viewport and return to game

	class UMultiPlayerSessionsSubsystem* MultiplayerSessionsSubsystem; // The subsystem designed to handle all online session functionality.

	int32 NumPublicConnections{ 4 };
	FString MatchType{ FString("FreeForAll") };
	FString PathToLobby{ TEXT("") };
};
