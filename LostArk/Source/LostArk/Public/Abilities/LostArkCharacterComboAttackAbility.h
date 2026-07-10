#pragma once

#include "CoreMinimal.h"
#include "Abilities/LostArkGameplayAbility.h"
#include "LostArkCharacterComboAttackAbility.generated.h"

UCLASS()
class LOSTARK_API ULostArkCharacterComboAttackAbility : public ULostArkGameplayAbility
{
	GENERATED_BODY()

public:
	ULostArkCharacterComboAttackAbility();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UFUNCTION(BlueprintCallable, Category = "Combo Attack")
	void RequestComboInput();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo Attack")
	TArray<TSoftObjectPtr<class UAnimMontage>> ComboMontages;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo Attack", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ComboWindowOpenRatio;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo Attack", meta = (ClampMin = "0.1"))
	float ComboInputTimeout;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo Attack", meta = (ClampMin = "0.0"))
	float PostComboCooldown;

	UPROPERTY()
	class UAbilityTask_PlayMontageAndWait* CurrentPlayTask;

	FGameplayTag CooldownTag;

private:
	void PlayComboMontage(int32 Index);

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

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

