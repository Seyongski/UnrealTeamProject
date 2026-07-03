// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "LostArkGameplayAbility.generated.h"

/**
 * 프로젝트 내 모든 Gameplay Ability의 기본이 되는 클래스
 */
UCLASS()
class LOSTARK_API ULostArkGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	ULostArkGameplayAbility();
};
