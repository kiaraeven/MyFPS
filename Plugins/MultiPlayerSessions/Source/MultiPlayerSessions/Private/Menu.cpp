// Fill out your copyright notice in the Description page of Project Settings.


#include "Menu.h"
#include "Components/Button.h"
#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"


void UMenu::MenuSetup(int32 NumberPublicConnections, FString TypeofMatch, FString LobbyPath)
{
	PathToLobby = FString::Printf(TEXT("%s?listen"), *LobbyPath);
	NumPublicConnections = NumberPublicConnections;
	MatchType = TypeofMatch;

	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
	
	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			FInputModeUIOnly InputModeData; // Interations only works on UI
			InputModeData.SetWidgetToFocus(TakeWidget()); // Focus on this widget
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); // Do not lock mouse to viewport
			PlayerController->SetInputMode(InputModeData); 
			PlayerController->bShowMouseCursor = true; // Show mouse cursor
		}
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiPlayerSessionsSubsystem>();
	}

	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->MultiplayerOnCreateSessionCompleteDelegate.AddDynamic(this, &UMenu::OnCreateSession);
		MultiplayerSessionsSubsystem->MultiplayerOnFindSessionsCompleteDelegate.AddUObject(this, &UMenu::OnFindSessions);
		MultiplayerSessionsSubsystem->MultiplayerOnJoinSessionCompleteDelegate.AddUObject(this, &UMenu::OnJoinSession);
		MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionCompleteDelegate.AddDynamic(this, &UMenu::OnDestroySession);
		MultiplayerSessionsSubsystem->MultiplayerOnStartSessionCompleteDelegate.AddDynamic(this, &UMenu::OnStartSession);
	}
	
}

bool UMenu::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &UMenu::HostButtonClicked);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &UMenu::JoinButtonClicked);
	}

	return true;
}

void UMenu::NativeDestruct()
{
	MenuTearDown();
	Super::NativeDestruct();
}

void UMenu::OnCreateSession(bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Red,
				TEXT("Session Creation Failed!")
			);
		}
		HostButton->SetIsEnabled(true); // Re-enable host button
		return;
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Green,
			TEXT("Session Created Successfully!")
		);
		UE_LOG(LogTemp, Warning, TEXT("Session Created Successfully!"));
	}

	UWorld* WorldPtr = GetWorld();
	if (WorldPtr)
	{
		WorldPtr->ServerTravel(PathToLobby);
	}
}

void UMenu::OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful)
{
	if (MultiplayerSessionsSubsystem == nullptr)
	{
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("Menu: OnFindSessions triggered. Success: %d, Count: %d"), bWasSuccessful, SessionResults.Num());

	for (auto Result : SessionResults)
	{
		FString SettingsValue;
		Result.Session.SessionSettings.Get(FName("MatchType"), SettingsValue);
		// check if the match type is the same as what we're looking for
		UE_LOG(LogTemp, Warning, TEXT("Found session of type: %s"), *SettingsValue);
		UE_LOG(LogTemp, Warning, TEXT("Looking for session of type: %s"), *MatchType);
		if (SettingsValue == MatchType)
		{
			UE_LOG(LogTemp, Warning, TEXT("Menu: MatchType matches! Requesting Join..."));
			MultiplayerSessionsSubsystem->JoinSession(Result);
			return;
		}
	}
	if (SessionResults.Num() > 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Menu: Sessions found but none matched the criteria."));
	}
	if (!bWasSuccessful || SessionResults.Num() == 0)
	{
		JoinButton->SetIsEnabled(true); // Re-enable join button
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Red,
				TEXT("No Sessions Found!")
			);
		}
	}
}

void UMenu::OnJoinSession(bool Result)
{
	UE_LOG(LogTemp, Warning, TEXT("Menu: OnJoinSession Callback. bWasSuccessful: %d"), Result);
	if (MultiplayerSessionsSubsystem == nullptr)
	{
		return;
	}
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem)
	{
		IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			FString Address;
			if (SessionInterface->GetResolvedConnectString(NAME_GameSession, Address))
			{
				APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
				if (PlayerController)
				{
					PlayerController->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
				}
			}
			else {
				UE_LOG(LogTemp, Error, TEXT("Menu: GetResolvedConnectString FAILED!"));
			}
		}
	}
	if (!Result)
	{
		JoinButton->SetIsEnabled(true); // Re-enable join button
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Red,
				TEXT("Failed to Join Session!")
			);
		}
	}
}

void UMenu::OnDestroySession(bool bWasSuccessful)
{
}

void UMenu::OnStartSession(bool bWasSuccessful)
{
}

//void UMenu::OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld)
//{
//	MenuTearDown();
//	Super::OnLevelRemovedFromWorld(InLevel, InWorld);
//}

void UMenu::HostButtonClicked()
{
	HostButton->SetIsEnabled(false); // Disable host button to prevent multiple clicks

	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->CreateSession(NumPublicConnections, MatchType);
	}

}

void UMenu::JoinButtonClicked()
{
	JoinButton->SetIsEnabled(false); // Disable join button to prevent multiple clicks
	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->FindSessions(10000); // Arbitrary large number
	}
}

void UMenu::MenuTearDown()
{
	RemoveFromParent();
	
	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			FInputModeGameOnly InputModeData; // Interations only works on UI
			PlayerController->SetInputMode(InputModeData); 
			PlayerController->bShowMouseCursor = false; // Show mouse cursor
		}
	}
}
