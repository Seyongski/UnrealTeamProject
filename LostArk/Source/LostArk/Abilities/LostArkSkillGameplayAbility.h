#pragma once

#include "CoreMinimal.h"
#include "Abilities/LostArkGameplayAbility.h"
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
	virtual UGameplayEffect* GetCostGameplayEffect() const override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	float SkillPlayRate = 1.0f;

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

	// ?먯떇 ?대퉴由ы떚?ㅼ뿉???쒖쟾 ?쒖옉 ???뺤? 諛?留덉슦???뚯쟾???쇨??섍쾶 泥섎━?????덈뒗 怨듯넻 ?ы띁
	void HandleActivationBasics(const FGameplayAbilityActorInfo* ActorInfo);

};




