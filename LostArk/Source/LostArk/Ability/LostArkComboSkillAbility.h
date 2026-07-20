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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Combo")
	FGameplayTag ComboEventTag;

protected:
	virtual void PlayComboSegment(int32 Index);
	int32 CurrentComboIndex;

private:

	UFUNCTION()
	void OnComboMontageCompleted();

	UFUNCTION()
	void OnComboMontageInterrupted();

	UFUNCTION()
	void OpenComboWindow();

	UFUNCTION()
	void CloseComboWindow();

	bool bIsComboWindowActive;
	bool bHasPendingComboInput;

	FTimerHandle ComboWindowOpenTimerHandle;
	FTimerHandle ComboWindowCloseTimerHandle;
};

