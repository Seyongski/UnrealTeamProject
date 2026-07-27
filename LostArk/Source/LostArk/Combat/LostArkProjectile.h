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

	void HandleImpact(AActor* OtherActor, const FVector& ImpactLocation);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnProjectileStop(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnProjectileOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

public:	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	USphereComponent* CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	UProjectileMovementComponent* ProjectileMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	UParticleSystemComponent* ParticleComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	bool bDestroyOnImpact = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	bool bIsPenetrating = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Combat")
	bool bIsCounterSkill = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Combat")
	float StaggerAmount = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float DamageInterval = 0.5f;

	FGameplayEffectSpecHandle DamageEffectSpecHandle;
	float ExplodeRadius = 0.f;

private:
	TMap<TWeakObjectPtr<AActor>, float> LastHitTimeMap;
};

