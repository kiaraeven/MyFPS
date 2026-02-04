// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BuffComponent.generated.h"


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MYFPS_API UBuffComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBuffComponent();
	friend class AMyCharacter;
	void Heal(float HealAmount, float HealingTime);
protected:
	virtual void BeginPlay() override;
	void HealRampUp(float DeltaTime); // be called in tick to heal
private:
	UPROPERTY()
	class AMyCharacter* Character;
	
	bool bHealing = false;
	float HealingRate = 0;
	float AmountToHeal = 0.f; // Total amount to heal. It should minus the heal amount per frame every frame.
public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;


};