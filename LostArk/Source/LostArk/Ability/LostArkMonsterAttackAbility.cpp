#include "LostArk/Ability/LostArkMonsterAttackAbility.h"
#include "LostArk/Core/LostArkCombatInterface.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"

ULostArkMonsterAttackAbility::ULostArkMonsterAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void ULostArkMonsterAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ILostArkCombatInterface* CombatInterface = Cast<ILostArkCombatInterface>(ActorInfo->AvatarActor.Get());
	if (!CombatInterface)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!AttackMontage.IsNull())
	{
		UAnimMontage* MontageToPlay = AttackMontage.LoadSynchronous();
		if (MontageToPlay)
		{
			UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
				this,
				TEXT("MonsterAttackPlayTask"),
				MontageToPlay,
				1.0f,
				NAME_None,
				false,
				1.0f
			);

			if (Task)
			{
				Task->OnCompleted.AddDynamic(this, &ULostArkMonsterAttackAbility::K2_EndAbility);
				Task->OnBlendOut.AddDynamic(this, &ULostArkMonsterAttackAbility::K2_EndAbility);
				Task->OnInterrupted.AddDynamic(this, &ULostArkMonsterAttackAbility::K2_EndAbility);
				Task->OnCancelled.AddDynamic(this, &ULostArkMonsterAttackAbility::K2_EndAbility);
				Task->ReadyForActivation();

				SetupHitCheckListener();
			}
		}
		else
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		}
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void ULostArkMonsterAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	ILostArkCombatInterface* CombatInterface = Cast<ILostArkCombatInterface>(ActorInfo->AvatarActor.Get());
	if (CombatInterface && !CombatInterface->IsDead())
	{
		CombatInterface->SetCombatState(FGameplayTag::RequestGameplayTag(FName("State.Idle"), false));
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}







