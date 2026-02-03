// Fill out your copyright notice in the Description page of Project Settings.


#include "MyAnimInstance.h"
#include "MyCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "MyFPS/Weapon/Weapon.h"
#include "MyFPS/MyTypes/CombatState.h"

void UMyAnimInstance::NativeInitializeAnimation() {
	Super::NativeBeginPlay();

	MyCharacter = Cast<AMyCharacter>(TryGetPawnOwner());
}

void UMyAnimInstance::NativeUpdateAnimation(float DeltaSeconds) {
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (MyCharacter == nullptr) {
		MyCharacter = Cast<AMyCharacter>(TryGetPawnOwner());
	}
	if (MyCharacter != nullptr) {
		FVector Velocity = MyCharacter->GetVelocity();
		Velocity.Z = 0;
		Speed = Velocity.Size();
		bIsInAir = MyCharacter->GetMovementComponent()->IsFalling();
		bIsAccelerating = MyCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f;
		bWeaponEquipped = MyCharacter->IsWeaponEquipped();
		EquippedWeapon = MyCharacter->GetEquippedWeapon();
		bIsCrouched = MyCharacter->bIsCrouched;
		bAiming = MyCharacter->IsAiming();
		TurningInPlace = MyCharacter->GetTurningInPlace(); 
		bElimmed = MyCharacter->IsElimmed();

		// Offset Yaw for strafing
		FRotator AimRotation = MyCharacter->GetBaseAimRotation(); // AimRotation is the controller rotation.
		FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(MyCharacter->GetVelocity()); // MovementRotation is the direction the character is moving towards.
		//UE_LOG(LogTemp, Warning, TEXT("AimRotation Yaw: %s"), AimRotation.Yaw);
		FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation); 
		DeltaRotation = FMath::RInterpTo(DeltaRotation, DeltaRot, DeltaSeconds, 6.f); // Smooth the delta rotation. 
		YawOffset = DeltaRotation.Yaw;

		CharacterRotationLastFrame = CharacterRotation;
		CharacterRotation = MyCharacter->GetActorRotation();
		const FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(CharacterRotation, CharacterRotationLastFrame); 
		const float Target = Delta.Yaw / DeltaSeconds;
		const float Interp = FMath::FInterpTo(Lean, Target, DeltaSeconds, 6.f);
		Lean = FMath::Clamp(Interp, -90.f, 90.f);

		AO_Yaw = MyCharacter->GetAO_Yaw();
		AO_Pitch = MyCharacter->GetAO_Pitch(); 

		if (bWeaponEquipped && EquippedWeapon && EquippedWeapon->GetWeaponMesh() && MyCharacter->GetMesh())
		{
			LeftHandTransform = EquippedWeapon->GetWeaponMesh()->GetSocketTransform(FName("LeftHandSocket"), ERelativeTransformSpace::RTS_World);
			FVector OutPosition;
			FRotator OutRotation;
			MyCharacter->GetMesh()->TransformToBoneSpace(FName("hand_r"), LeftHandTransform.GetLocation(), FRotator::ZeroRotator, OutPosition, OutRotation);
			LeftHandTransform.SetLocation(OutPosition);
			LeftHandTransform.SetRotation(FQuat(OutRotation));

			if (MyCharacter->IsLocallyControlled()) {
				bLocallyControlled = true;
				FTransform RightHandTransform = MyCharacter->GetMesh()->GetSocketTransform(FName("hand_r"), RTS_World);
				FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(RightHandTransform.GetLocation(), RightHandTransform.GetLocation() + (RightHandTransform.GetLocation() - MyCharacter->GetHitTarget()));
				RightHandRotation = FMath::RInterpTo(RightHandRotation, LookAtRotation, DeltaSeconds, 30.f);
			}
		}
		bUseFABRIK = MyCharacter->GetCombatState() != ECombatState::ECS_Reloading;
		bUseAimOffsets = MyCharacter->GetCombatState() != ECombatState::ECS_Reloading && !MyCharacter->GetDisableGameplay();
		bTransformRightHand = MyCharacter->GetCombatState() != ECombatState::ECS_Reloading && !MyCharacter->GetDisableGameplay();
	}
}
