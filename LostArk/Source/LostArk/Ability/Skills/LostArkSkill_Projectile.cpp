#include "LostArk/Ability/Skills/LostArkSkill_Projectile.h"
#include "LostArk/Actor/LostArkProjectile.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

ULostArkSkill_Projectile::ULostArkSkill_Projectile()
{
}

void ULostArkSkill_Projectile::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 부모 클래스에서 마우스 방향으로 캐릭터 자동 회전 및 네비게이션 정지가 공통 처리됩니다.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 시전 방향은 이미 마우스를 향해 회전된 캐릭터의 정면 방향을 사용합니다.
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

	FVector SpawnLocation = AvatarActor->GetActorLocation() + CachedTargetDirection * 100.f;
	FRotator SpawnRotation = CachedTargetDirection.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Instigator = Cast<APawn>(AvatarActor);
	SpawnParams.Owner = AvatarActor;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ALostArkProjectile* Projectile = GetWorld()->SpawnActor<ALostArkProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
	if (Projectile && DamageEffectClass)
	{
		FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
		ContextHandle.AddInstigator(AvatarActor, AvatarActor);

		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(DamageEffectClass, 1.f, ContextHandle);
		
		UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
			SpecHandle,
			FGameplayTag::RequestGameplayTag(FName("Data.Damage"), false),
			DamageShapeParams.DamageCoefficient
		);

		Projectile->DamageEffectSpecHandle = SpecHandle;
		
		if (DamageShapeParams.ShapeType == EDamageShape::Sphere)
		{
			Projectile->ExplodeRadius = DamageShapeParams.Radius;
		}
	}
}



