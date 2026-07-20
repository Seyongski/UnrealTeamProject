#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "LostArkProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UParticleSystemComponent;

UCLASS()
class LOSTARK_API ALostArkProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	ALostArkProjectile();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnProjectileStop(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

public:	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	USphereComponent* CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	UProjectileMovementComponent* ProjectileMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	UParticleSystemComponent* ParticleComponent;

	FGameplayEffectSpecHandle DamageEffectSpecHandle;
	float ExplodeRadius = 0.f;
};

