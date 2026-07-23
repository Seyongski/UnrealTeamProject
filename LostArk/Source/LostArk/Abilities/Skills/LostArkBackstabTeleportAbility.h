#pragma once

#include "CoreMinimal.h"
#include "Abilities/LostArkSkillGameplayAbility.h"
#include "LostArkBackstabTeleportAbility.generated.h"

class ALostArkMonster;

UCLASS()
class LOSTARK_API ULostArkBackstabTeleportAbility : public ULostArkSkillGameplayAbility
{
	GENERATED_BODY()

public:
	ULostArkBackstabTeleportAbility();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Backstab")
	float TeleportOffset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Backstab")
	float MaxSearchDistance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Backstab")
	float TargetSearchRadius;

private:
	mutable TWeakObjectPtr<ALostArkMonster> CachedTargetMonster;
};




