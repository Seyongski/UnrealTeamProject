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
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
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

	// 스폰 즉시 이미 겹쳐 있는 Pawn(보스/몬스터)이 있을 경우 피격 처리 수행
	if (CollisionComponent)
	{
		TArray<AActor*> OverlappingActors;
		CollisionComponent->GetOverlappingActors(OverlappingActors, APawn::StaticClass());
		for (AActor* OverlappedActor : OverlappingActors)
		{
			if (OverlappedActor && OverlappedActor != GetInstigator() && OverlappedActor != this)
			{
				HandleImpact(OverlappedActor, GetActorLocation());
				break;
			}
		}
	}
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

	TArray<AActor*> HitTargets;

	// 1) 직접 충돌한 타겟(OtherActor)이 존재하면 1순위로 포함 (Z축 오차 및 캡슐 쿼리 누락 방지)
	if (OtherActor && CanHitTarget(OtherActor))
	{
		HitTargets.AddUnique(OtherActor);
	}

	// 2) 폭발 반경이 존재하는 경우 주변 쿼리로 타겟 수집
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
				HitTargets.AddUnique(HitActor);
			}
		}
	}

	// 3) 수집된 타겟들에 대해 데미지, 카운터, 무력화 일괄 처리
	for (AActor* TargetActor : HitTargets)
	{
		if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor))
		{
			TargetASC->ApplyGameplayEffectSpecToSelf(*DamageEffectSpecHandle.Data.Get());

			if (bIsCounterSkill)
			{
				FGameplayEventData CounterEventData;
				CounterEventData.Instigator = InstigatorActor;
				CounterEventData.Target = TargetActor;
				UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
					TargetActor,
					FGameplayTag::RequestGameplayTag(FName("Event.Boss.CounterHit"), false),
					CounterEventData
				);
			}

			if (StaggerAmount > 0.f)
			{
				FGameplayEventData StaggerEventData;
				StaggerEventData.Instigator = InstigatorActor;
				StaggerEventData.Target = TargetActor;
				StaggerEventData.EventMagnitude = StaggerAmount;
				UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
					TargetActor,
					FGameplayTag::RequestGameplayTag(FName("Event.Boss.StaggerHit"), false),
					StaggerEventData
				);
			}
		}
	}

	if (bDestroyOnImpact && !bIsPenetrating)
	{
		Destroy();
	}
}




