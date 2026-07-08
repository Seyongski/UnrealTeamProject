#pragma once

#include "CoreMinimal.h"
#include "LostArkGameplayAbility.h"
#include "LostArkSkillGameplayAbility.generated.h"

UCLASS()
class LOSTARK_API ULostArkSkillGameplayAbility : public ULostArkGameplayAbility
{
	GENERATED_BODY()

public:
	ULostArkSkillGameplayAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	TSoftObjectPtr<UAnimMontage> SkillMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Dash")
	bool bApplyDashForce;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Dash", meta = (EditCondition = "bApplyDashForce"))
	float DashDistance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Dash", meta = (EditCondition = "bApplyDashForce"))
	float DashDuration;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Dash", meta = (EditCondition = "bApplyDashForce"))
	bool bIgnoreCollisionDuringDash;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	TSubclassOf<class UGameplayEffect> CooldownEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	TSubclassOf<class UGameplayEffect> CostEffectClass;

	UPROPERTY()
	class UAbilityTask_PlayMontageAndWait* CurrentPlayTask;

	UFUNCTION()
	virtual void OnMontageCompleted();

	UFUNCTION()
	virtual void OnMontageInterrupted();

};
