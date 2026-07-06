// Fill out your copyright notice in the Description page of Project Settings.


#include "GA_NormalAttack.h"
#include "LostArkCharacter.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"

void UGA_NormalAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle, 
	const FGameplayAbilityActorInfo* ActorInfo, 
	const FGameplayAbilityActivationInfo ActivationInfo, 
	const FGameplayEventData* TriggerEventData)
{
	CachedHandle = Handle;
	CachedActorInfo = ActorInfo;
	CachedActivationInfo = ActivationInfo;

	if (ComboIndex == 0)
	{
		ComboIndex = 1;
	}

	// ATTACK STATE ON
	ActorInfo->AbilitySystemComponent->AddLooseGameplayTag(
		FGameplayTag::RequestGameplayTag("State.Attacking"));

	UE_LOG(LogTemp, Warning, TEXT("Add AttackTag"));

	PlayCurrentCombo();
}

void UGA_NormalAttack::PlayCurrentCombo()
{
	if (!CachedActorInfo)
	{
		EndAbility(CachedHandle, CachedActorInfo, CachedActivationInfo, true, false);
		return;
	}
	if (!AttackMontages.IsValidIndex(ComboIndex - 1))
	{
		EndAbility(CachedHandle, CachedActorInfo, CachedActivationInfo, true, false);
		return;
	}

	UAnimMontage* Montage = AttackMontages[ComboIndex - 1];
	UE_LOG(LogTemp, Warning, TEXT("Play Attack Anim!!Count : %d"), ComboIndex);

	UAbilityTask_PlayMontageAndWait* Task =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			Montage
		);


	Task->OnCompleted.AddDynamic(this, &UGA_NormalAttack::OnMontageFinished);
	Task->OnInterrupted.AddDynamic(this, &UGA_NormalAttack::OnMontageInterrupted);
	Task->OnCancelled.AddDynamic(this, &UGA_NormalAttack::OnMontageCancelled);

	Task->ReadyForActivation();
}

void UGA_NormalAttack::OnMontageFinished()
{
	AdvanceComboOrFinish();
}

void UGA_NormalAttack::OnMontageInterrupted()
{
	ResetCombo();
}

void UGA_NormalAttack::OnMontageCancelled()
{
	ResetCombo();
}


void UGA_NormalAttack::AdvanceComboOrFinish()
{
	ALostArkCharacter* LostArkPlayer =
		Cast<ALostArkCharacter>(GetAvatarActorFromActorInfo());

	UE_LOG(LogTemp, Warning,
		TEXT("AdvanceCombo Buffer=%d Combo=%d"),
		LostArkPlayer ? LostArkPlayer->bAttackBuffered : -1,
		ComboIndex);

	if (LostArkPlayer && LostArkPlayer->IsAttackHeld())
	{
		UE_LOG(LogTemp, Warning, TEXT("NEXT COMBO"));
		LostArkPlayer->bAttackBuffered = false;

		ComboIndex++;

		if (ComboIndex > MaxComboCount)
		{
			ResetCombo();
			return;
		}

		PlayCurrentCombo();
		return;
	}

	ResetCombo();
}

void UGA_NormalAttack::ResetCombo()
{
	ComboIndex = 1;

	if (CachedActorInfo)
	{
		UAbilitySystemComponent* ASC = CachedActorInfo->AbilitySystemComponent.Get();
		if (ASC)
		{
			ASC->RemoveLooseGameplayTag(
				FGameplayTag::RequestGameplayTag("State.Attacking")); 
			UE_LOG(LogTemp, Warning, TEXT("Remove AttackTag"));
		}
	}
	EndAbility(CachedHandle, CachedActorInfo, CachedActivationInfo, true, false);
}
