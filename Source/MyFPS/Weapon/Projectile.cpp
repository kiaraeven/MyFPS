#include "Projectile.h"
#include "Components/BoxComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h" 
#include "NiagaraComponent.h"
#include "Sound/SoundCue.h"
#include "MyFPS/Character/MyCharacter.h"
#include "MyFPS/MyFPS.h"

AProjectile::AProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true; // set to true so that the projectile is replicated in multiplayer
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);
	CollisionBox->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	CollisionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block); // So traces can hit the projectile
	CollisionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Block); // So projectile bounces off walls
	CollisionBox->SetCollisionResponseToChannel(ECC_SkeletalMesh, ECollisionResponse::ECR_Block); // So projectile hits characters

	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
	ProjectileMovementComponent->bRotationFollowsVelocity = true; // Projectile rotates to match its velocity direction
}

void AProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	if (Tracer)
	{
		TracerComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			Tracer,                 
			CollisionBox,           // attaching target
			FName(),                 
			GetActorLocation(),      
			GetActorRotation(),     
			EAttachLocation::KeepWorldPosition, // attaching rule
			true                    // auto destroy
		);
	}

	if (HasAuthority())
	{
		CollisionBox->OnComponentHit.AddDynamic(this, &AProjectile::OnHit);
	}
}

void AProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{

	Destroy();
}

void AProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AProjectile::Destroyed()
{
	if (ImpactParticles)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			ImpactParticles,
			GetActorLocation(),
			GetActorRotation()
		);
	}
	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			ImpactSound,
			GetActorLocation()
		);
	}
}

