#pragma once

#include "CoreMinimal.h"
#include "LostArkSkillGameplayAbility.h"
#include "LostArkSkill_Projectile.generated.h"

class ALostArkProjectile;

UCLASS()
class LOSTARK_API ULostArkSkill_Projectile : public ULostArkSkillGameplayAbility
{
	GENERATED_BODY()

public:
	ULostArkSkill_Projectile();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile")
	TSubclassOf<ALostArkProjectile> ProjectileClass;

	virtual void OnHitCheckReceived(FGameplayEventData Payload) override;

private:
	FVector CachedTargetDirection;
};
