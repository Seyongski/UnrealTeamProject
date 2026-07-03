#pragma once

#include "CoreMinimal.h"
#include "LostArkGameplayAbility.h"
#include "LostArkMonsterAttackAbility.generated.h"

UCLASS()
class LOSTARK_API ULostArkMonsterAttackAbility : public ULostArkGameplayAbility
{
	GENERATED_BODY()

public:
	ULostArkMonsterAttackAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	TSoftObjectPtr<class UAnimMontage> AttackMontage;
};

