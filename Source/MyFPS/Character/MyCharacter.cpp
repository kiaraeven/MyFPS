// Fill out your copyright notice in the Description page of Project Settings.


#include "MyCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "MyFPS/Weapon/Weapon.h"
#include "MyFPS/Components/CombatComponent.h"
#include "MyFPS/Components/BuffComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "MyFPS/MyFPS.h"
#include "MyFPS/PlayerController/MyPlayerController.h"
#include "MyFPS/GameMode/MyGameMode.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "MyFPS/PlayerState/MyPlayerState.h"
#include "MyFPS/Weapon/WeaponTypes.h"
#include "Components/BoxComponent.h"
#include "MyFPS/Components/LagCompensationComponent.h"

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

	Buff = CreateDefaultSubobject<UBuffComponent>(TEXT("BuffComponent"));
	Buff->SetIsReplicated(true);

	LagCompensation = CreateDefaultSubobject<ULagCompensationComponent>(TEXT("LagCompensation"));

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); // So camera does not collide with capsule
	GetMesh()->SetCollisionObjectType(ECC_SkeletalMesh); // Custom channel so we can ignore in trace
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); // So camera does not collide with mesh
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // So traces hit the mesh
	TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	NetUpdateFrequency = 66.f;
	MinNetUpdateFrequency = 33.f; 

	DissolveTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DissolveTimelineComponent"));

	/**
	* Hit boxes for server-side rewind
	*/

	head = CreateDefaultSubobject<UBoxComponent>(TEXT("head"));
	head->SetupAttachment(GetMesh(), FName("head"));
	head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitCollisionBoxes.Add(FName("head"), head);

	pelvis = CreateDefaultSubobject<UBoxComponent>(TEXT("pelvis"));
	pelvis->SetupAttachment(GetMesh(), FName("pelvis"));
	pelvis->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitCollisionBoxes.Add(FName("pelvis"), pelvis);

	spine_02 = CreateDefaultSubobject<UBoxComponent>(TEXT("spine_02"));
	spine_02->SetupAttachment(GetMesh(), FName("spine_02"));
	spine_02->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitCollisionBoxes.Add(FName("spine_02"), spine_02);

	spine_03 = CreateDefaultSubobject<UBoxComponent>(TEXT("spine_03"));
	spine_03->SetupAttachment(GetMesh(), FName("spine_03"));
	spine_03->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitCollisionBoxes.Add(FName("spine_03"), spine_03);

	upperarm_l = CreateDefaultSubobject<UBoxComponent>(TEXT("upperarm_l"));
	upperarm_l->SetupAttachment(GetMesh(), FName("upperarm_l"));
	upperarm_l->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitCollisionBoxes.Add(FName("upperarm_l"), upperarm_l);

	upperarm_r = CreateDefaultSubobject<UBoxComponent>(TEXT("upperarm_r"));
	upperarm_r->SetupAttachment(GetMesh(), FName("upperarm_r"));
	upperarm_r->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitCollisionBoxes.Add(FName("upperarm_r"), upperarm_r);

	lowerarm_l = CreateDefaultSubobject<UBoxComponent>(TEXT("lowerarm_l"));
	lowerarm_l->SetupAttachment(GetMesh(), FName("lowerarm_l"));
	lowerarm_l->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitCollisionBoxes.Add(FName("lowerarm_l"), lowerarm_l);

	lowerarm_r = CreateDefaultSubobject<UBoxComponent>(TEXT("lowerarm_r"));
	lowerarm_r->SetupAttachment(GetMesh(), FName("lowerarm_r"));
	lowerarm_r->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitCollisionBoxes.Add(FName("lowerarm_r"), lowerarm_r);

	hand_l = CreateDefaultSubobject<UBoxComponent>(TEXT("hand_l"));
	hand_l->SetupAttachment(GetMesh(), FName("hand_l"));
	hand_l->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitCollisionBoxes.Add(FName("hand_l"), hand_l);

	hand_r = CreateDefaultSubobject<UBoxComponent>(TEXT("hand_r"));
	hand_r->SetupAttachment(GetMesh(), FName("hand_r"));
	hand_r->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitCollisionBoxes.Add(FName("hand_r"), hand_r);

	thigh_l = CreateDefaultSubobject<UBoxComponent>(TEXT("thigh_l"));
	thigh_l->SetupAttachment(GetMesh(), FName("thigh_l"));
	thigh_l->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitCollisionBoxes.Add(FName("thigh_l"), thigh_l);

	thigh_r = CreateDefaultSubobject<UBoxComponent>(TEXT("thigh_r"));
	thigh_r->SetupAttachment(GetMesh(), FName("thigh_r"));
	thigh_r->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitCollisionBoxes.Add(FName("thigh_r"), thigh_r);

	calf_l = CreateDefaultSubobject<UBoxComponent>(TEXT("calf_l"));
	calf_l->SetupAttachment(GetMesh(), FName("calf_l"));
	calf_l->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitCollisionBoxes.Add(FName("calf_l"), calf_l);

	calf_r = CreateDefaultSubobject<UBoxComponent>(TEXT("calf_r"));
	calf_r->SetupAttachment(GetMesh(), FName("calf_r"));
	calf_r->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitCollisionBoxes.Add(FName("calf_r"), calf_r);

	foot_l = CreateDefaultSubobject<UBoxComponent>(TEXT("foot_l"));
	foot_l->SetupAttachment(GetMesh(), FName("foot_l"));
	foot_l->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitCollisionBoxes.Add(FName("foot_l"), foot_l);

	foot_r = CreateDefaultSubobject<UBoxComponent>(TEXT("foot_r"));
	foot_r->SetupAttachment(GetMesh(), FName("foot_r"));
	foot_r->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitCollisionBoxes.Add(FName("foot_r"), foot_r);
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
	SpawnDefaultWeapon();
	UpdateHUDAmmo();
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

	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &AMyCharacter::ReloadButtonPressed);

}

void AMyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AMyCharacter, OverlappingWeapon, COND_OwnerOnly);
	DOREPLIFETIME(AMyCharacter, Health);
	DOREPLIFETIME(AMyCharacter, bDisableGameplay);
}

void AMyCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (CombatComponent)
	{
		CombatComponent->Character = this;
	}
	if (Buff)
	{
		Buff->Character = this;
	}
	if (LagCompensation)
	{
		LagCompensation->Character = this;
		if (Controller)
		{
			LagCompensation->Controller = Cast<AMyPlayerController>(Controller);
		}
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

void AMyCharacter::PlayReloadMontage()
{
	if (CombatComponent == nullptr || CombatComponent->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ReloadWeaponMontage)
	{
		AnimInstance->Montage_Play(ReloadWeaponMontage);
		FName SectionName;
		switch (CombatComponent->EquippedWeapon->GetWeaponType())
		{
		case EWeaponType::EWT_AssaultRifle:
			SectionName = FName("Rifle");
			break;
		case EWeaponType::EWT_RocketLauncher:
			SectionName = FName("Rifle");
			break;
		case EWeaponType::EWT_Pistol:
			SectionName = FName("Rifle");
			break;
		}
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
		if (CombatComponent->EquippedWeapon->bDestroyWeapon)
		{
			CombatComponent->EquippedWeapon->Destroy();
		}
		else
		{
			CombatComponent->EquippedWeapon->Dropped();
		}
	}
	MulticastElim();
	GetWorldTimerManager().SetTimer(
		ElimTimer,
		this,
		&AMyCharacter::ElimTimerFinished,
		ElimDelay
	);
}

void AMyCharacter::Destroyed()
{
	Super::Destroyed();
	AMyGameMode* MyGameMode = Cast<AMyGameMode>(UGameplayStatics::GetGameMode(this));
	bool bMatchNotInProgress = MyGameMode && MyGameMode->GetMatchState() != MatchState::InProgress;
	if (CombatComponent && CombatComponent->EquippedWeapon && bMatchNotInProgress)
	{
		CombatComponent->EquippedWeapon->Destroy();
	}
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
	bDisableGameplay = true;
	if (CombatComponent)
	{
		CombatComponent->FireButtonPressed(false);
	}
	if (MyPlayerController)
	{
		DisableInput(MyPlayerController);
	}
	// Disable collision
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Hide sniper scope
	bool bHideSniperScope = IsLocallyControlled() &&
		CombatComponent &&
		CombatComponent->bAiming &&
		CombatComponent->EquippedWeapon &&
		CombatComponent->EquippedWeapon->GetWeaponType() == EWeaponType::EWT_AssaultRifle;
	if (bHideSniperScope)
	{
		ShowSniperScopeWidget(false);
	}
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

void AMyCharacter::UpdateHUDAmmo()
{
	MyPlayerController = MyPlayerController == nullptr ? Cast<AMyPlayerController>(Controller) : MyPlayerController;
	if (MyPlayerController && CombatComponent && CombatComponent->EquippedWeapon)
	{
		MyPlayerController->SetHUDCarriedAmmo(CombatComponent->CarriedAmmo);
		MyPlayerController->SetHUDWeaponAmmo(CombatComponent->EquippedWeapon->GetAmmo());
	}
}

void AMyCharacter::SpawnDefaultWeapon()
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AMyGameMode* MyGameMode = Cast<AMyGameMode>(UGameplayStatics::GetGameMode(this));
	UWorld* World = GetWorld();
	if (MyGameMode && World && !bElimmed && DefaultWeaponClass)
	{
		//AWeapon* StartingWeapon = World->SpawnActor<AWeapon>(DefaultWeaponClass);
		AWeapon* StartingWeapon = World->SpawnActor<AWeapon>(
			DefaultWeaponClass,
			GetActorTransform(),
			SpawnParams
		);
		if (StartingWeapon)
		{
			StartingWeapon->bDestroyWeapon = true;
			if (CombatComponent)
			{
				CombatComponent->EquipWeapon(StartingWeapon);
			}
		}
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
	if (bDisableGameplay) return;
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
	if (bDisableGameplay) return;

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
	if (bDisableGameplay) return;

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
	if (bDisableGameplay) return;

	if (bIsCrouched)
		UnCrouch();
	else
		Crouch();
}

void AMyCharacter::ReloadButtonPressed()
{
	if (bDisableGameplay) return;

	if (CombatComponent) {
		CombatComponent->Reload();
	}
}

void AMyCharacter::AimButtonPressed()
{
	if (bDisableGameplay) return;

	if (CombatComponent)
	{
		CombatComponent->SetAiming(true);
	}
}

void AMyCharacter::AimButtonReleased()
{
	if (bDisableGameplay) return;

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
	if (bDisableGameplay) return;
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
	if (bDisableGameplay) return;

	if (CombatComponent)
	{
		CombatComponent->FireButtonPressed(true);
	}
}

void AMyCharacter::FireButtonReleased()
{
	if (bDisableGameplay) return;

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

void AMyCharacter::OnRep_Health(float LastHealth)
{
	UpdateHUDHealth();
	if (Health < LastHealth)
	{
		PlayHitReactMontage();
	}
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

ECombatState AMyCharacter::GetCombatState() const
{
	if (CombatComponent == nullptr) return ECombatState::ECS_MAX;
	return CombatComponent->CombatState;
}

bool AMyCharacter::IsLocallyReloading()
{
	if (CombatComponent == nullptr) return false;
	return CombatComponent->bLocallyReloading;
}
