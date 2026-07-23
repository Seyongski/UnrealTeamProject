#include "Combat/LostArkProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Character/LostArkCharacter.h"
#include "Monster/LostArkMonster.h"

ALostArkProjectile::ALostArkProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
	CollisionComponent->InitSphereRadius(15.0f);
	CollisionComponent->BodyInstance.SetCollisionProfileName("Projectile");
	CollisionComponent->OnComponentHit.AddDynamic(this, &ALostArkProjectile::OnProjectileStop);
	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &ALostArkProjectile::OnProjectileOverlap);
	
	RootComponent = CollisionComponent;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = 3000.f;
	ProjectileMovement->MaxSpeed = 3000.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;

	ParticleComponent = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("ParticleComponent"));
	ParticleComponent->SetupAttachment(RootComponent);
}

void ALostArkProjectile::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(5.0f);
}

void ALostArkProjectile::OnProjectileStop(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	HandleImpact(OtherActor, Hit.Location);
}

void ALostArkProjectile::OnProjectileOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != GetInstigator() && OtherActor != this)
	{
		FVector ImpactLoc = bFromSweep ? FVector(SweepResult.Location) : GetActorLocation();
		HandleImpact(OtherActor, ImpactLoc);
	}
}

void ALostArkProjectile::HandleImpact(AActor* OtherActor, const FVector& ImpactLocation)
{
	if (!DamageEffectSpecHandle.IsValid())
	{
		if (bDestroyOnImpact && !bIsPenetrating)
		{
			Destroy();
		}
		return;
	}

	AActor* InstigatorActor = GetInstigator();
	if (OtherActor == InstigatorActor)
	{
		return;
	}

	float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	auto CanHitTarget = [&](AActor* Target) -> bool
	{
		if (!Target || Target == InstigatorActor || Target == this)
		{
			return false;
		}

		// PvP 아군 피격 방지
		bool bPvP = InstigatorActor && Target->IsA<ALostArkCharacter>() && InstigatorActor->IsA<ALostArkCharacter>();
		if (bPvP)
		{
			return false;
		}

		if (bIsPenetrating || !bDestroyOnImpact)
		{
			TWeakObjectPtr<AActor> WeakTarget(Target);
			if (float* LastTime = LastHitTimeMap.Find(WeakTarget))
			{
				if (CurrentTime - *LastTime < DamageInterval)
				{
					return false;
				}
			}
			LastHitTimeMap.Add(WeakTarget, CurrentTime);
		}

		return true;
	};

	if (ExplodeRadius > 0.f)
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);
		if (InstigatorActor)
		{
			QueryParams.AddIgnoredActor(InstigatorActor);
		}

		GetWorld()->OverlapMultiByObjectType(Overlaps, ImpactLocation, FQuat::Identity, ObjectQueryParams, FCollisionShape::MakeSphere(ExplodeRadius), QueryParams);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* HitActor = Overlap.GetActor();
			if (CanHitTarget(HitActor))
			{
				if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor))
				{
					TargetASC->ApplyGameplayEffectSpecToSelf(*DamageEffectSpecHandle.Data.Get());
				}
			}
		}
	}
	else if (OtherActor)
	{
		if (CanHitTarget(OtherActor))
		{
			if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor))
			{
				TargetASC->ApplyGameplayEffectSpecToSelf(*DamageEffectSpecHandle.Data.Get());
			}
		}
	}

	if (bDestroyOnImpact && !bIsPenetrating)
	{
		Destroy();
	}
}




