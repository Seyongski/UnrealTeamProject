// Fill out your copyright notice in the Description page of Project Settings.

#include "LostArkGameplayAbility.h"

ULostArkGameplayAbility::ULostArkGameplayAbility()
{
	// 인스턴싱 정책 설정 (일반적으로 NonInstanced 또는 InstancedPerActor 사용)
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}
