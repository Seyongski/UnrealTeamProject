#pragma once

#include "CoreMinimal.h"
#include "LostArk/Ability/LostArkGameplayAbility.h"
#include "LostArkSkillGameplayAbility.generated.h"

UCLASS()
class LOSTARK_API ULostArkSkillGameplayAbility : public ULostArkGameplayAbility
{
	GENERATED_BODY()

public:
	ULostArkSkillGameplayAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual UGameplayEffect* GetCooldownGameplayEffect() const override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	TSoftObjectPtr<UAnimMontage> SkillMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Targeting")
	bool bRotateToMouseOnActivate;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Targeting")
	bool bAbortNavigationMove;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Dash")
	bool bApplyDashForce;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Dash", meta = (EditCondition = "bApplyDashForce"))
	float DashDistance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Dash", meta = (EditCondition = "bApplyDashForce"))
	float DashDuration;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Dash", meta = (EditCondition = "bApplyDashForce"))
	bool bIgnoreCollisionDuringDash;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Dash", meta = (EditCondition = "bApplyDashForce"))
	bool bInvincibleDuringDash;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	TSubclassOf<class UGameplayEffect> CooldownEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	TSubclassOf<class UGameplayEffect> CostEffectClass;

	UPROPERTY()
	class UAbilityTask_PlayMontageAndWait* CurrentPlayTask;

public:
	UFUNCTION()
	virtual void OnMontageCompleted();

	UFUNCTION()
	virtual void OnMontageInterrupted();

	// 자식 어빌리티들에서 시전 시작 시 정지 및 마우스 회전을 일관되게 처리할 수 있는 공통 헬퍼
	void HandleActivationBasics(const FGameplayAbilityActorInfo* ActorInfo);

};



