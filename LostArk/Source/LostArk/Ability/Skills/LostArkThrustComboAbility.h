#pragma once

#include "CoreMinimal.h"
#include "LostArk/Ability/LostArkSkillGameplayAbility.h"
#include "LostArkThrustComboAbility.generated.h"

class ALostArkShadowClone;

UENUM(BlueprintType)
enum class EThrustComboState : uint8
{
	Idle,
	Thrusting,
	ComboWindowOpen,
	SpinActive,
	Finished
};

UCLASS()
class LOSTARK_API ULostArkThrustComboAbility : public ULostArkSkillGameplayAbility
{
	GENERATED_BODY()

public:
	ULostArkThrustComboAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Combo")
	TSoftObjectPtr<UAnimMontage> ThrustMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Combo")
	TSoftObjectPtr<UAnimMontage> ThrustComboFinishMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	TSoftClassPtr<ALostArkShadowClone> ShadowCloneClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	FDamageShapeParams CloneDamageShapeParams;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	TSoftObjectPtr<UAnimMontage> CloneSpinMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Combo")
	float ComboWindowStartTime;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Combo")
	float ComboWindowEndTime;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	float CloneSpawnOffset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	float CloneRadius;

private:
	EThrustComboState CurrentComboState;

	FTimerHandle ComboWindowStartTimerHandle;
	FTimerHandle ComboWindowEndTimerHandle;
	FTimerHandle SkillTimeoutTimerHandle;

	UFUNCTION()
	void OnComboWindowOpened();

	UFUNCTION()
	void OnComboWindowClosed();

	UFUNCTION()
	void OnThrustAnimCompleted();

	void PlayThrustComboFinish();
};



