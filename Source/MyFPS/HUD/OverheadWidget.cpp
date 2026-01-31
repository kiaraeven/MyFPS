// Fill out your copyright notice in the Description page of Project Settings.


#include "OverheadWidget.h"
#include "Components/TextBlock.h"

void UOverheadWidget::SetDisplayText(FString TextToDisplay)
{
	if (DisplayText) {
		DisplayText->SetText(FText::FromString(TextToDisplay));
	}
}

void UOverheadWidget::ShowPlayerNetRole(APawn* InPawn)
{
	ENetRole LocalRole = InPawn->GetLocalRole();
	FString Role;
	switch (LocalRole) {
	case ENetRole::ROLE_Authority:
		Role = "Authority";
		break;
	case ENetRole::ROLE_AutonomousProxy:
		Role = "Autonomous Proxy";
		break;
	case ENetRole::ROLE_SimulatedProxy:
		Role = "Simulated Proxy";
		break;
	default:
		Role = "None";
		break;
	}
	FString RoleString = FString::Printf(TEXT("Local Role: %s"), *Role);
	SetDisplayText(RoleString);
}


//void UOverheadWidget::OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld)
//{
//	RemoveFromParent();
//	Super::OnLevelRemovedFromWorld(InLevel, InWorld);
//}

void UOverheadWidget::NativeDestruct()
{
	RemoveFromParent();
	Super::NativeDestruct();
}