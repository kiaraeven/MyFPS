// Fill out your copyright notice in the Description page of Project Settings.


#include "MyCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "MyFPS/Weapon/Weapon.h"
#include "MyFPS/Components/CombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "MyFPS/MyFPS.h"
#include "MyFPS/PlayerController/MyPlayerController.h"
#include "MyFPS/GameMode/MyGameMode.h"
#include "TimerManager.h"
#include "MyFPS/PlayerState/MyPlayerState.h"

// Sets default values
AMyCharacter::AMyCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent); // To aviud camera moving with capsule size changing
	CameraBoom->TargetArmLength = 600.0f; // The camera follows at
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...

	OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	OverheadWidget->SetupAttachment(RootComponent);

	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	CombatComponent->SetIsReplicated(true);

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); // So camera does not collide with capsule
	GetMesh()->SetCollisionObjectType(ECC_SkeletalMesh); // Custom channel so we can ignore in trace
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); // So camera does not collide with mesh
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // So traces hit the mesh
	TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	NetUpdateFrequency = 66.f;
	MinNetUpdateFrequency = 33.f; 

	DissolveTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DissolveTimelineComponent"));
}

// Called when the game starts or when spawned
void AMyCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	MyPlayerController = Cast<AMyPlayerController>(Controller);
	if (MyPlayerController)
	{
		MyPlayerController->SetHUDHealth(Health, MaxHealth);
	}

	UpdateHUDHealth();
	if (HasAuthority())
	{
		OnTakeAnyDamage.AddDynamic(this, &AMyCharacter::ReceiveDamage);
	}
}

// Called every frame
void AMyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AimOffset(DeltaTime); 
	HideCameraIfCharacterClose();
	PollInit();
}

// Called to bind functionality to input
void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AMyCharacter::Jump);
	PlayerInputComponent->BindAxis("MoveForward", this, &AMyCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AMyCharacter::MoveRight);
	PlayerInputComponent->BindAxis("LookUp", this, &AMyCharacter::LookUp);
	PlayerInputComponent->BindAxis("Turn", this, &AMyCharacter::Turn);

	PlayerInputComponent->BindAction("Equip", IE_Pressed, this, &AMyCharacter::EquipButtonPressed);
	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &AMyCharacter::CrouchButtonPressed);

	PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &AMyCharacter::AimButtonPressed);
	PlayerInputComponent->BindAction("Aim", IE_Released, this, &AMyCharacter::AimButtonReleased);

	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AMyCharacter::FireButtonPressed);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &AMyCharacter::FireButtonReleased);
}

void AMyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AMyCharacter, OverlappingWeapon, COND_OwnerOnly);
	DOREPLIFETIME(AMyCharacter, Health);
}

void AMyCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (CombatComponent)
	{
		CombatComponent->Character = this;
	}
}

void AMyCharacter::PlayFireMontage(bool bAiming)
{
	if (CombatComponent == nullptr || CombatComponent->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && FireWeaponMontage)
	{
		AnimInstance->Montage_Play(FireWeaponMontage);
		FName SectionName;
		SectionName = bAiming ? FName("RifleAim") : FName("RifleHip");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void AMyCharacter::PlayElimMontage()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ElimMontage)
	{
		AnimInstance->Montage_Play(ElimMontage);
	}
}

void AMyCharacter::Elim()
{
	if (CombatComponent && CombatComponent->EquippedWeapon)
	{
		CombatComponent->EquippedWeapon->Dropped();
	}
	MulticastElim();
	GetWorldTimerManager().SetTimer(
		ElimTimer,
		this,
		&AMyCharacter::ElimTimerFinished,
		ElimDelay
	);
}

void AMyCharacter::MulticastElim_Implementation()
{
	if (MyPlayerController)
	{
		MyPlayerController->SetHUDWeaponAmmo(0);
	}

	bElimmed = true;
	PlayElimMontage();

	// Start dissolve effect
	if (DissolveMaterialInstanceHead && DissolveMaterialInstanceLimb && DissolveMaterialInstanceTorso)
	{
		DynamicDissolveMaterialInstanceHead = UMaterialInstanceDynamic::Create(DissolveMaterialInstanceHead, this);
		DynamicDissolveMaterialInstanceLimb = UMaterialInstanceDynamic::Create(DissolveMaterialInstanceLimb, this);
		DynamicDissolveMaterialInstanceTorso = UMaterialInstanceDynamic::Create(DissolveMaterialInstanceTorso, this);

		GetMesh()->SetMaterial(0, DynamicDissolveMaterialInstanceHead);
		GetMesh()->SetMaterial(2, DynamicDissolveMaterialInstanceLimb);
		GetMesh()->SetMaterial(1, DynamicDissolveMaterialInstanceTorso);

		DynamicDissolveMaterialInstanceHead->SetScalarParameterValue(TEXT("Dissolve"), -0.55f);
		DynamicDissolveMaterialInstanceHead->SetScalarParameterValue(TEXT("Glow"), 200.f);
		DynamicDissolveMaterialInstanceLimb->SetScalarParameterValue(TEXT("Dissolve"), -0.55f);
		DynamicDissolveMaterialInstanceLimb->SetScalarParameterValue(TEXT("Glow"), 200.f);
		DynamicDissolveMaterialInstanceTorso->SetScalarParameterValue(TEXT("Dissolve"), -0.55f);
		DynamicDissolveMaterialInstanceTorso->SetScalarParameterValue(TEXT("Glow"), 200.f);
	}
	StartDissolve();

	// Disable character movement
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->StopMovementImmediately();
	if (MyPlayerController)
	{
		DisableInput(MyPlayerController);
	}
	// Disable collision
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AMyCharacter::PlayHitReactMontage()
{
	if (CombatComponent == nullptr || CombatComponent->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HitReactMontage)
	{
		AnimInstance->Montage_Play(HitReactMontage);
		FName SectionName("FromFront");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void AMyCharacter::ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser)
{
	Health = FMath::Clamp(Health - Damage, 0.f, MaxHealth);
	UpdateHUDHealth();
	PlayHitReactMontage();

	if (Health == 0.f)
	{
		AMyGameMode* MyGameMode = GetWorld()->GetAuthGameMode<AMyGameMode>();
		if (MyGameMode)
		{
			MyPlayerController = MyPlayerController == nullptr ? Cast<AMyPlayerController>(Controller) : MyPlayerController;
			// InstigatorController is the controller of the player who caused the damage
			AMyPlayerController* AttackerController = Cast<AMyPlayerController>(InstigatorController);
			MyGameMode->PlayerEliminated(this, MyPlayerController, AttackerController);
		}
	}
}

void AMyCharacter::UpdateHUDHealth()
{
	MyPlayerController = MyPlayerController == nullptr ? Cast<AMyPlayerController>(Controller) : MyPlayerController;
	if (MyPlayerController)
	{
		MyPlayerController->SetHUDHealth(Health, MaxHealth);
	}
}

void AMyCharacter::PollInit()
{
	// tick to try to get playerstate every frame. At the beginning it can be nullptr.
	if (MyPlayerState == nullptr) {
		MyPlayerState = GetPlayerState<AMyPlayerState>();
		if (MyPlayerState) {
			MyPlayerState->AddToScore(0.f); // so that after respawning the character can keep the same score
			MyPlayerState->AddToDefeats(0);
		}
	}
}


void AMyCharacter::MoveForward(float Value) {
	if (Controller != nullptr && Value != 0.0f) {
		// Find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		// Get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AMyCharacter::MoveRight(float Value) {
	if (Controller != nullptr && Value != 0.0f) {
		// Find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		// Get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(Direction, Value);
	}
}

void AMyCharacter::LookUp(float Value) {
	AddControllerPitchInput(Value);
}

void AMyCharacter::Turn(float Value) {
	AddControllerYawInput(Value);
}

void AMyCharacter::EquipButtonPressed()
{
	if (CombatComponent)
	{
		if (HasAuthority()) {
			// call from server
			CombatComponent->EquipWeapon(OverlappingWeapon);
		}
		else {
			// call from client
			ServerEquipButtonPressed();
		}
	}
}

void AMyCharacter::CrouchButtonPressed()
{
	if (bIsCrouched)
		UnCrouch();
	else
		Crouch();
}

void AMyCharacter::AimButtonPressed()
{
	if (CombatComponent)
	{
		CombatComponent->SetAiming(true);
	}
}

void AMyCharacter::AimButtonReleased()
{
	if (CombatComponent)
	{
		CombatComponent->SetAiming(false);
	}
}

void AMyCharacter::AimOffset(float DeltaTime)
{
	if (CombatComponent && CombatComponent->EquippedWeapon == nullptr) return;
	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;
	float Speed = Velocity.Size();
	bool bIsInAir = GetCharacterMovement()->IsFalling();

	if (Speed == 0.f && !bIsInAir) // standing still, not jumping
	{
		FRotator CurrentAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation);
		AO_Yaw = DeltaAimRotation.Yaw;
		bUseControllerRotationYaw = false;
		TurnInPlace(DeltaTime);
	}
	if (Speed > 0.f || bIsInAir) // running, or jumping
	{
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		AO_Yaw = 0.f;
		bUseControllerRotationYaw = true;
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}

	AO_Pitch = GetBaseAimRotation().Pitch;

	if (AO_Pitch > 90.f && !IsLocallyControlled())
	{
		// map pitch from [270, 360) to [-90, 0)
		FVector2D InRange(270.f, 360.f);
		FVector2D OutRange(-90.f, 0.f);
		AO_Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AO_Pitch);
	}
}

void AMyCharacter::Jump()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Super::Jump();
	}
}

void AMyCharacter::FireButtonPressed()
{
	if (CombatComponent)
	{
		CombatComponent->FireButtonPressed(true);
	}
}

void AMyCharacter::FireButtonReleased()
{
	if (CombatComponent)
	{
		CombatComponent->FireButtonPressed(false);
	}
}

void AMyCharacter::OnRep_OverlappingWeapon(AWeapon* LastWeapon)
{
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(true);
	}
	if (LastWeapon)
	{
		LastWeapon->ShowPickupWidget(false);
	}
}

void AMyCharacter::ServerEquipButtonPressed_Implementation()
{
	if (CombatComponent)
	{
		CombatComponent->EquipWeapon(OverlappingWeapon);
	}
}

void AMyCharacter::TurnInPlace(float DeltaTime)
{
	if (AO_Yaw > 90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Right;
	}
	else if (AO_Yaw < -90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Left;
	}
}

void AMyCharacter::HideCameraIfCharacterClose()
{
	if (!IsLocallyControlled()) return;
	if ((FollowCamera->GetComponentLocation() - GetActorLocation()).Size() < CameraThreshold)
	{
		GetMesh()->SetVisibility(false);
		if (CombatComponent && CombatComponent->EquippedWeapon && CombatComponent->EquippedWeapon->GetWeaponMesh())
		{
			CombatComponent->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = true;
		}
	}
	else
	{
		GetMesh()->SetVisibility(true);
		if (CombatComponent && CombatComponent->EquippedWeapon && CombatComponent->EquippedWeapon->GetWeaponMesh())
		{
			CombatComponent->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = false;
		}
	}
}

void AMyCharacter::OnRep_Health()
{
	UpdateHUDHealth();
	PlayHitReactMontage();
}

void AMyCharacter::ElimTimerFinished()
{
	AMyGameMode* MyGameMode = GetWorld()->GetAuthGameMode<AMyGameMode>();
	if (MyGameMode)
	{
		MyGameMode->RequestRespawn(this, Controller);
	}
}

void AMyCharacter::UpdateDissolveMaterial(float DissolveValue)
{
	DynamicDissolveMaterialInstanceHead->SetScalarParameterValue(TEXT("Dissolve"), DissolveValue);
	DynamicDissolveMaterialInstanceLimb->SetScalarParameterValue(TEXT("Dissolve"), DissolveValue);
	DynamicDissolveMaterialInstanceTorso->SetScalarParameterValue(TEXT("Dissolve"), DissolveValue);
}

void AMyCharacter::StartDissolve()
{
	DissolveTrack.BindDynamic(this, &AMyCharacter::UpdateDissolveMaterial); // bind callback
	if (DissolveCurve && DissolveTimeline)
	{
		DissolveTimeline->AddInterpFloat(DissolveCurve, DissolveTrack);
		DissolveTimeline->Play();
	}
}

void AMyCharacter::SetOverlappingWeapon(AWeapon* Weapon)
{
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(false);
	}
	OverlappingWeapon = Weapon;
	if (IsLocallyControlled())
	{
		if (OverlappingWeapon)
		{
			OverlappingWeapon->ShowPickupWidget(true);
		}
	}
}

bool AMyCharacter::IsWeaponEquipped() const
{
	return CombatComponent && CombatComponent->EquippedWeapon;
}

bool AMyCharacter::IsAiming() const {
	return CombatComponent && CombatComponent->bAiming;
}

AWeapon* AMyCharacter::GetEquippedWeapon()
{
	if (CombatComponent == nullptr) return nullptr;
	return CombatComponent->EquippedWeapon;
}

FVector AMyCharacter::GetHitTarget() const
{
	if (CombatComponent == nullptr) return FVector();
	return CombatComponent->HitTarget;
}
