#include "Abilities/LostArkSkill_Projectile.h"
#include "Combat/LostArkProjectile.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

ULostArkSkill_Projectile::ULostArkSkill_Projectile()
{
}

void ULostArkSkill_Projectile::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 遺紐??대옒?ㅼ뿉??留덉슦??諛⑺뼢?쇰줈 罹먮┃???먮룞 ?뚯쟾 諛??ㅻ퉬寃뚯씠???뺤?媛 怨듯넻 泥섎━?⑸땲??
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// ?쒖쟾 諛⑺뼢? ?대? 留덉슦?ㅻ? ?ν빐 ?뚯쟾??罹먮┃?곗쓽 ?뺣㈃ 諛⑺뼢???ъ슜?⑸땲??
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		CachedTargetDirection = ActorInfo->AvatarActor->GetActorForwardVector();
	}
	else
	{
		CachedTargetDirection = FVector::ForwardVector;
	}
}

void ULostArkSkill_Projectile::OnHitCheckReceived(FGameplayEventData Payload)
{
	if (!ProjectileClass) return;

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor) return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	// 시전 위치: 캐릭터 캡슐 정면 근처(45.f)로 스폰 오프셋 단축하여 근접 시전 시 뚫고 지나가는 문제 방지
	FVector SpawnLocation = AvatarActor->GetActorLocation() + CachedTargetDirection * 45.f;
	FRotator SpawnRotation = CachedTargetDirection.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Instigator = Cast<APawn>(AvatarActor);
	SpawnParams.Owner = AvatarActor;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ALostArkProjectile* Projectile = GetWorld()->SpawnActor<ALostArkProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
	if (Projectile && DamageEffectClass)
	{
		Projectile->bIsCounterSkill = bIsCounterSkill;
		Projectile->StaggerAmount = StaggerAmount;

		FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
		ContextHandle.AddInstigator(AvatarActor, AvatarActor);

		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(DamageEffectClass, 1.f, ContextHandle);
		
		UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
			SpecHandle,
			FGameplayTag::RequestGameplayTag(FName("Data.Damage"), false),
			DamageShapeParams.DamageCoefficient
		);

		Projectile->DamageEffectSpecHandle = SpecHandle;
		
		float HitRadius = DamageShapeParams.Radius;
		if (DamageShapeParams.ShapeType == EDamageShape::Box)
		{
			HitRadius = FMath::Max(DamageShapeParams.BoxExtent.X, DamageShapeParams.BoxExtent.Y);
		}
		if (HitRadius <= 0.f)
		{
			HitRadius = 100.f;
		}
		Projectile->ExplodeRadius = HitRadius;

		// 근접 밀착 시전 보정: 캐릭터 발밑~스폰위치 구간 근접 Overlap 탐색
		TArray<FOverlapResult> Overlaps;
		FCollisionObjectQueryParams ObjParams(ECC_Pawn);
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(AvatarActor);
		QueryParams.AddIgnoredActor(Projectile);

		if (GetWorld()->OverlapMultiByObjectType(Overlaps, AvatarActor->GetActorLocation() + CachedTargetDirection * 30.f, FQuat::Identity, ObjParams, FCollisionShape::MakeSphere(60.f), QueryParams))
		{
			for (const FOverlapResult& Overlap : Overlaps)
			{
				if (AActor* HitActor = Overlap.GetActor())
				{
					Projectile->HandleImpact(HitActor, HitActor->GetActorLocation());
					break;
				}
			}
		}
	}
}




