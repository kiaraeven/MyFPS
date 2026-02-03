#pragma once

#define CUSTOM_DEPTH_PURPLE 250
#define CUSTOM_DEPTH_BLUE 251
#define CUSTOM_DEPTH_TAN 252


UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	EWT_AssaultRifle UMETA(DisplayName = "Assault Rifle"), // Projectile
	EWT_RocketLauncher UMETA(DisplayName = "Rocket Launcher"), // Rocket
	EWT_Pistol UMETA(DisplayName = "Pistol"), // Hitscan
	EWT_MAX UMETA(DisplayName = "DefaultMAX")
};