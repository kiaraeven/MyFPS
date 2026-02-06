// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LagCompensationComponent.generated.h"


USTRUCT(BlueprintType)
struct FBoxInformation
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location;

	UPROPERTY()
	FRotator Rotation;

	UPROPERTY()
	FVector BoxExtent;
};

USTRUCT(BlueprintType)
struct FFramePackage
{
	GENERATED_BODY()

	UPROPERTY()
	float Time;

	UPROPERTY()
	TMap<FName, FBoxInformation> HitBoxInfo;
};

USTRUCT(BlueprintType)
struct FServerSideRewindResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bHitConfirmed;

	UPROPERTY()
	bool bHeadShot;
};


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MYFPS_API ULagCompensationComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	ULagCompensationComponent();
	friend class AMyCharacter;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void ShowFramePackage(const FFramePackage& Package, const FColor& Color);
	FServerSideRewindResult ServerSideRewind(
		class AMyCharacter* HitCharacter,
		const FVector_NetQuantize& TraceStart,
		const FVector_NetQuantize& HitLocation,
		float HitTime);
	UFUNCTION(Server, Reliable)
	void ServerScoreRequest(
		AMyCharacter* HitCharacter,
		const FVector_NetQuantize& TraceStart,
		const FVector_NetQuantize& HitLocation,
		float HitTime,
		class AWeapon* DamageCauser
	);
protected:
	virtual void BeginPlay() override;
	void SaveFramePackage(FFramePackage& Package);
	FFramePackage InterpBetweenFrames(const FFramePackage& OlderFrame, const FFramePackage& YoungerFrame, float HitTime);
	FServerSideRewindResult ConfirmHit(
		const FFramePackage& Package,
		AMyCharacter* HitCharacter,
		const FVector_NetQuantize& TraceStart,
		const FVector_NetQuantize& HitLocation);
	void CacheBoxPositions(AMyCharacter* HitCharacter, FFramePackage& OutFramePackage); // cache all boxes of the HitCharacter in the current frame
	void MoveBoxes(AMyCharacter* HitCharacter, const FFramePackage& Package); // Replace HitCharacter's current boxes with the frame to check (package)
	void ResetHitBoxes(AMyCharacter* HitCharacter, const FFramePackage& Package); // Recover HitCharacter's boxes at the current frame using cached package
	void EnableCharacterMeshCollision(AMyCharacter* HitCharacter, ECollisionEnabled::Type CollisionEnabled);
	void SaveFramePackage();
private:
	UPROPERTY()
	AMyCharacter* Character;
	UPROPERTY()
	class AMyPlayerController* Controller;

	TDoubleLinkedList<FFramePackage> FrameHistory; 
	UPROPERTY(EditAnywhere)
	float MaxRecordTime = 4.f; // Save frames in the past 4 seconds. Discard the oldest frame when reaching this maxrecordtime.
public:	

		
};
