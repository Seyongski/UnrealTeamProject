#pragma once

#include "CoreMinimal.h"
#include "LostArk/Ability/LostArkSkillGameplayAbility.h"
#include "LostArkComboSkillAbility.generated.h"

UCLASS()
class LOSTARK_API ULostArkComboSkillAbility : public ULostArkSkillGameplayAbility
{
	GENERATED_BODY()

public:
	ULostArkComboSkillAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Combo")
	TArray<TSoftObjectPtr<UAnimMontage>> ComboMontages;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Combo", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ComboWindowOpenRatio;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Combo", meta = (ClampMin = "0.1"))
	float ComboInputTimeout;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	TSoftClassPtr<class ALostArkShadowClone> ShadowCloneClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	TSoftObjectPtr<class UAnimMontage> LeftCloneMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	TSoftObjectPtr<class UAnimMontage> RightCloneMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	float CloneSpawnOffsetForward;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	float CloneSpawnOffsetRight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	FDamageShapeParams CloneDamageShapeParams;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Dash")
	float FinalDashDistance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Dash")
	float FinalDashDuration;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Dash")
	FDamageShapeParams FinalDashDamageShapeParams;

protected:
	virtual void OnHitCheckReceived(FGameplayEventData Payload) override;

private:
	void PlayComboSegment(int32 Index);

	UFUNCTION()
	void OnComboMontageCompleted();

	UFUNCTION()
	void OnComboMontageInterrupted();

	UFUNCTION()
	void OpenComboWindow();

	UFUNCTION()
	void CloseComboWindow();

	int32 CurrentComboIndex;
	bool bIsComboWindowActive;
	bool bHasPendingComboInput;

	FTimerHandle ComboWindowOpenTimerHandle;
	FTimerHandle ComboWindowCloseTimerHandle;
};



