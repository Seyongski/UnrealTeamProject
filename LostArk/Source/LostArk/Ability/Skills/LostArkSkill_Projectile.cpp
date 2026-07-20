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




