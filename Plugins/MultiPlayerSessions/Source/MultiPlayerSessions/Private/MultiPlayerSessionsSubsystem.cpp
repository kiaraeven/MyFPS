// Fill out your copyright notice in the Description page of Project Settings.


#include "MultiPlayerSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"

UMultiPlayerSessionsSubsystem::UMultiPlayerSessionsSubsystem() :
	OnCreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UMultiPlayerSessionsSubsystem::OnCreateSessionComplete)),
	OnFindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UMultiPlayerSessionsSubsystem::OnFindSessionsComplete)),
	OnJoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UMultiPlayerSessionsSubsystem::OnJoinSessionComplete)),
	OnDestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &UMultiPlayerSessionsSubsystem::OnDestroySessionComplete)),
	OnStartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &UMultiPlayerSessionsSubsystem::OnStartSessionComplete))
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();

	if (Subsystem)
	{
		SessionInterface = Subsystem->GetSessionInterface();
	}
}

void UMultiPlayerSessionsSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful){
	// if session created successfully, clear the delegate from the delegate list
	if (SessionInterface)
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);
	}
	MultiplayerOnCreateSessionCompleteDelegate.Broadcast(bWasSuccessful);
}

void UMultiPlayerSessionsSubsystem::OnFindSessionsComplete(bool bWasSuccessful){
	if (SessionInterface)
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompleteDelegateHandle);
	}
	if (LastSessionSearch->SearchResults.Num() <= 0)
	{
		// if no sessions found then broadcast empty array
		MultiplayerOnFindSessionsCompleteDelegate.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
	}
	else
	{
		// broadcast the array of found sessions
		MultiplayerOnFindSessionsCompleteDelegate.Broadcast(LastSessionSearch->SearchResults, bWasSuccessful);
	} 
}

void UMultiPlayerSessionsSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result){
	if (SessionInterface)
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegateHandle);
	}
	UE_LOG(LogTemp, Warning, TEXT("Subsystem: OnJoinSessionComplete. Result Code: %d (0 is Success)"), (int32)Result);
	//MultiplayerOnJoinSessionCompleteDelegate.Broadcast(Result);
	bool bWasSuccessful = (Result == EOnJoinSessionCompleteResult::Success);
	MultiplayerOnJoinSessionCompleteDelegate.Broadcast(bWasSuccessful);
}

void UMultiPlayerSessionsSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful){
	if (SessionInterface)
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegateHandle);
	}
	if (bWasSuccessful && bCreateSessionOnDestroy)
	{
		bCreateSessionOnDestroy = false;
		CreateSession(LastNumPublicConnections, LastMatchType);
	}
	MultiplayerOnDestroySessionCompleteDelegate.Broadcast(bWasSuccessful);
}

void UMultiPlayerSessionsSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful){}

void UMultiPlayerSessionsSubsystem::CreateSession(int32 NumPublicConnections, FString MatchType) {
	if (!SessionInterface.IsValid())
	{
		return;
	}
	auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession != nullptr)
	{
		// if a session already exists, destroy it first then create the new session
		// store the desired number of connections and match type so we can reuse them once the existing session is destroyed
		bCreateSessionOnDestroy = true;
		LastNumPublicConnections = NumPublicConnections;
		LastMatchType = MatchType;
		DestroySession();
	}

	SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegate); // Store the delegate handle so that we can later remove it from the delegate list
	LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
	// check subsystem name (log debug info)
	UE_LOG(LogTemp, Warning, TEXT("Plugin(createsession) found subsystem: %s"), *IOnlineSubsystem::Get()->GetSubsystemName().ToString());
	LastSessionSettings->bIsLANMatch = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false; // "null" means LAN match
	//LastSessionSettings->bIsLANMatch = true;
	LastSessionSettings->NumPublicConnections = NumPublicConnections; // max number of players
	LastSessionSettings->bShouldAdvertise = true; // advertise the session
	LastSessionSettings->bUsesPresence = true; // use presence system
	LastSessionSettings->bAllowJoinInProgress = true; // allow players to join while game is in progress
	LastSessionSettings->bAllowJoinViaPresence = true; // allow players to join via presence
	LastSessionSettings->bUseLobbiesIfAvailable = true;
	LastSessionSettings->Set(FName("MatchType"), MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing); // set custom session setting 
	LastSessionSettings->BuildUniqueId = 1; // to differentiate sessions with same name
	
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings))
	{
		// if failed to create session, clear the delegate from the delegate list
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);
		// broadcast our own delegate to Menu class
		MultiplayerOnCreateSessionCompleteDelegate.Broadcast(false); 
	}
}

void UMultiPlayerSessionsSubsystem::FindSessions(int32 MaxSearchResults) {
	if (!SessionInterface.IsValid())
	{
		return;
	}
	OnFindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompleteDelegate);
	LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastSessionSearch->MaxSearchResults = MaxSearchResults;
	UE_LOG(LogTemp, Warning, TEXT("Plugin(findsessions) found subsystem: %s"), *IOnlineSubsystem::Get()->GetSubsystemName().ToString());
	LastSessionSearch->bIsLanQuery = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false; // "null" means LAN match
	//LastSessionSearch->bIsLanQuery = true;
	//LastSessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals); // only search for sessions that are using presence
	LastSessionSearch->QuerySettings.Set(FName("PRESENCE"), true, EOnlineComparisonOp::Equals);
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef()))
	{
		// if failed to find sessions, clear the delegate from the delegate list
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompleteDelegateHandle);
		MultiplayerOnFindSessionsCompleteDelegate.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
	}
}

void UMultiPlayerSessionsSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult) {
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnJoinSessionCompleteDelegate.Broadcast(false);
		return;
	}

	OnJoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegate);
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionResult))
	{
		// if failed to join session, clear the delegate from the delegate list
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegateHandle);
		MultiplayerOnJoinSessionCompleteDelegate.Broadcast(false);
	}
}

void UMultiPlayerSessionsSubsystem::DestroySession() {
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnDestroySessionCompleteDelegate.Broadcast(false);
		return;
	}

	OnDestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegate);
	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		// if failed to destroy session, clear the delegate from the delegate list
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegateHandle);
		MultiplayerOnDestroySessionCompleteDelegate.Broadcast(false);
	}
}

void UMultiPlayerSessionsSubsystem::StartSession() {

}