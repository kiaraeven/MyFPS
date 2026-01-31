

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Casing.generated.h"

UCLASS()
class MYFPS_API ACasing : public AActor
{
	GENERATED_BODY()
	
public:	
	ACasing();
	virtual void Tick(float DeltaTime) override;
protected:
	virtual void BeginPlay() override;
	UFUNCTION()
	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
private:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* CasingMesh;
	UPROPERTY(EditAnywhere)
	float ShellEjectionImpulse;
	UPROPERTY(EditAnywhere)
	class USoundCue* ShellSound;
public:	
	

};
