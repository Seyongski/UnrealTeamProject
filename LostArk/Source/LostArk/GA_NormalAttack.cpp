// Fill out your copyright notice in the Description page of Project Settings.


#include "GA_NormalAttack.h"

void UGA_NormalAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle, 
	const FGameplayAbilityActorInfo* ActorInfo, 
	const FGameplayAbilityActivationInfo ActivationInfo, 
	const FGameplayEventData* TriggerEventData)
{
	UE_LOG(LogTemp, Warning, TEXT("GA_NormalAttack Activated"));

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
