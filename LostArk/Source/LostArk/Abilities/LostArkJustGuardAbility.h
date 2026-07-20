#pragma once

#include "CoreMinimal.h"
#include "Abilities/LostArkGameplayAbility.h"
#include "LostArkJustGuardAbility.generated.h"

/**
 * ??ㅽ듃媛??(Just Guard) ?대퉴由ы떚
 * 
 * 蹂댁뒪 ?⑦꽩 以?'State.Player.GuardReady' ?쒓렇媛 ?덉쓣 ?뚮쭔 諛쒕룞 媛??
 * 留덉슦??而ㅼ꽌 諛⑺뼢?쇰줈 ?뚯쟾 ??媛??紐쏀?二쇰? ?ъ깮?섍퀬, 蹂댁뒪?먭쾶 ?대깽?몃? ?꾩넚??
 */
UCLASS()
class LOSTARK_API ULostArkJustGuardAbility : public ULostArkGameplayAbility
{
	GENERATED_BODY()

public:
	ULostArkJustGuardAbility();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UPROPERTY(EditDefaultsOnly, Category = "JustGuard|Anim")
	class UAnimMontage* GuardStartMontage;

	UPROPERTY(EditDefaultsOnly, Category = "JustGuard|Anim")
	class UAnimMontage* GuardLoopMontage;

	UPROPERTY(EditDefaultsOnly, Category = "JustGuard|Anim")
	class UAnimMontage* GuardSuccessMontage;

	UPROPERTY(EditDefaultsOnly, Category = "JustGuard|Anim")
	class UAnimMontage* GuardFailMontage;

	UPROPERTY(EditDefaultsOnly, Category = "JustGuard|Config")
	float MaxGuardDuration = 1.5f;

private:
	void RotateToCursor(const FGameplayAbilityActorInfo* ActorInfo);
	void SendJustGuardEventToBoss(const FGameplayAbilityActorInfo* ActorInfo);
	AActor* FindBossActor(UWorld* World) const;

	void PlayLoopMontage();
	void EndGuardDueToTimeout();

	UFUNCTION()
	void OnStartMontageCompleted();

	UFUNCTION()
	void OnMontageCancelledOrInterrupted();

	UFUNCTION()
	void OnSuccessEventReceived(FGameplayEventData Payload);

	UFUNCTION()
	void OnFailEventReceived(FGameplayEventData Payload);

	UFUNCTION()
	void OnResultMontageCompleted();

	FTimerHandle GuardTimeoutTimerHandle;
};

