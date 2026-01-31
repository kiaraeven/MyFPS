// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "MyFPS/Character/MyCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Casing.h"
#include "Engine/SkeletalMeshSocket.h"
#include "MyFPS/PlayerController/MyPlayerController.h"


AWeapon::AWeapon()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(RootComponent);
	SetRootComponent(WeaponMesh);

	WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block); // Set collision response to block all channels 
	WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore); // Ignore collision with pawns
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Disable collision for the weapon mesh

	AreaSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AreaSphere"));
	AreaSphere->SetupAttachment(RootComponent);
	AreaSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision); 

	PickupWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PickupWidget"));
	PickupWidget->SetupAttachment(WeaponMesh);
}



void AWeapon::ShowPickupWidget(bool bShowWidget)
{
	if (PickupWidget)
	{
		PickupWidget->SetVisibility(bShowWidget);
	}
}

void AWeapon::Fire(const FVector& HitTarget)
{
	if (FireAnimation)
	{
		WeaponMesh->PlayAnimation(FireAnimation, false);
	} 

	if (CasingClass)
	{
		const USkeletalMeshSocket* AmmoEjectSocket = WeaponMesh->GetSocketByName(FName("AmmoEject"));
		if (AmmoEjectSocket)
		{
			FTransform SocketTransform = AmmoEjectSocket->GetSocketTransform(WeaponMesh);

			UWorld* World = GetWorld();
			if (World)
			{
				World->SpawnActor<ACasing>(
					CasingClass,
					SocketTransform.GetLocation(),
					SocketTransform.GetRotation().Rotator()
				);
			}
		}
	}
	SpendRound();
}



void AWeapon::BeginPlay()
{
	Super::BeginPlay();

	
	if (HasAuthority())
	{
		// Server-side logic goes here
		// Enable collision for the area sphere on the server. QueryAndPhysics to allow overlap events and physics interactions.
		AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); 
		AreaSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap); // Enable overlap events with pawns
		AreaSphere->OnComponentBeginOverlap.AddDynamic(this, &AWeapon::OnSphereOverlap);
		AreaSphere->OnComponentEndOverlap.AddDynamic(this, &AWeapon::OnSphereEndOverlap);
	}

	if (PickupWidget)
	{
		PickupWidget->SetVisibility(false);
	}
}

void AWeapon::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor)
	{
		AMyCharacter* MyCharacter = Cast<AMyCharacter>(OtherActor);
		if (MyCharacter)
		{
			//PickupWidget->SetVisibility(true);
			MyCharacter->SetOverlappingWeapon(this);
		}
	}
}

void AWeapon::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor)
	{
		AMyCharacter* MyCharacter = Cast<AMyCharacter>(OtherActor);
		if (MyCharacter)
		{
			//PickupWidget->SetVisibility(true);
			MyCharacter->SetOverlappingWeapon(nullptr);
		}
	}
}

void AWeapon::OnRep_WeaponState()
{
	switch (WeaponState)
	{
	case EWeaponState::EWS_Equipped:
		ShowPickupWidget(false);
		WeaponMesh->SetSimulatePhysics(false);
		WeaponMesh->SetEnableGravity(false);
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		break;
	case EWeaponState::EWS_Dropped:
		WeaponMesh->SetSimulatePhysics(true);
		WeaponMesh->SetEnableGravity(true);
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		break;
	default:
		break;
	}
}

void AWeapon::OnRep_Ammo()
{
	MyOwnerCharacter = MyOwnerCharacter == nullptr ? Cast<AMyCharacter>(GetOwner()) : MyOwnerCharacter;
	SetHUDAmmo();
}

void AWeapon::SpendRound()
{
	Ammo = FMath::Clamp(Ammo - 1, 0, MagCapacity);
	SetHUDAmmo();
}

void AWeapon::SetWeaponState(EWeaponState State)
{
	WeaponState = State;
	switch (WeaponState)
	{
	case EWeaponState::EWS_Equipped:
		ShowPickupWidget(false);
		AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WeaponMesh->SetSimulatePhysics(false);
		WeaponMesh->SetEnableGravity(false);
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		break;
	case EWeaponState::EWS_Dropped:
		if (HasAuthority())
		{
			AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}
		WeaponMesh->SetSimulatePhysics(true);
		WeaponMesh->SetEnableGravity(true);
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		break;
	}
}

bool AWeapon::AmmoIsEmpty()
{
	return Ammo <= 0;
}




void AWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AWeapon::Dropped()
{
	SetWeaponState(EWeaponState::EWS_Dropped); // weapon state is replicated
	FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
	WeaponMesh->DetachFromComponent(DetachRules);
	SetOwner(nullptr);
	MyOwnerCharacter = nullptr;
	MyOwnerController = nullptr;
}

void AWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWeapon, WeaponState); // register WeaponState for replication
	DOREPLIFETIME(AWeapon, Ammo);
}

void AWeapon::OnRep_Owner()
{
	Super::OnRep_Owner();
	if (Owner == nullptr)
	{
		MyOwnerCharacter = nullptr;
		MyOwnerController = nullptr;
	}
	else
	{
		SetHUDAmmo();
	}
}

void AWeapon::SetHUDAmmo()
{
	MyOwnerCharacter = MyOwnerCharacter == nullptr ? Cast<AMyCharacter>(GetOwner()) : MyOwnerCharacter;
	if (MyOwnerCharacter)
	{
		MyOwnerController = MyOwnerController == nullptr ? Cast<AMyPlayerController>(MyOwnerCharacter->Controller) : MyOwnerController;
		if (MyOwnerController)
		{
			MyOwnerController->SetHUDWeaponAmmo(Ammo);
		}
	}
}

void AWeapon::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}


