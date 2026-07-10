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
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(ActorInfo->AvatarActor.Get(), 0))
	{
		FHitResult HitResult;
		if (PC->GetHitResultUnderCursor(ECC_Visibility, false, HitResult))
		{
			FVector TargetLoc = HitResult.ImpactPoint;
			FVector Dir = TargetLoc - ActorInfo->AvatarActor->GetActorLocation();
			Dir.Z = 0.f;
			CachedTargetDirection = Dir.GetSafeNormal();
			
			ActorInfo->AvatarActor->SetActorRotation(CachedTargetDirection.Rotation());
		}
		else
		{
			CachedTargetDirection = ActorInfo->AvatarActor->GetActorForwardVector();
		}
	}
	else
	{
		CachedTargetDirection = ActorInfo->AvatarActor->GetActorForwardVector();
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
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
