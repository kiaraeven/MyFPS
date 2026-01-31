#pragma once

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	EWT_ProjectileWeapon UMETA(DisplayName = "Projectile Weapon"),

	EWT_MAX UMETA(DisplayName = "DefaultMAX")
};