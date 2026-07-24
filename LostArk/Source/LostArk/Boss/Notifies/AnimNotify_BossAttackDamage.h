// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_BossAttackDamage.generated.h"

class UGameplayEffect;

/**
 * 보스 일반 공격/몽타주 타격 순간에 적용하는 데미지 애님 노티파이.
 * 보스의 정면/공격 범위 내 플레이어에게 (Boss.BaseAttackDamage * DamageCoefficient) 데미지를 적용합니다.
 */
UCLASS(meta = (DisplayName = "Boss Attack Damage"))
class LOSTARK_API UAnimNotify_BossAttackDamage : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_BossAttackDamage();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	/** 데미지 계수 (기본 1.0 = BaseAttackDamage 100%) */
	UPROPERTY(EditAnywhere, Category = "Damage", meta = (ClampMin = "0.1"))
	float DamageCoefficient = 1.f;

	/** 보스 중심으로부터 타격 판정 중심까지의 전방 오프셋 (cm) */
	UPROPERTY(EditAnywhere, Category = "Damage")
	float FrontOffset = 150.f;

	/** 타격 판정 구체 반경 (cm) */
	UPROPERTY(EditAnywhere, Category = "Damage", meta = (ClampMin = "10.0"))
	float AttackRadius = 250.f;

	/** 커스텀 데미지 이펙트 (지정 시 보스의 DefaultDamageEffectClass 대신 사용) */
	UPROPERTY(EditAnywhere, Category = "Damage")
	TSubclassOf<UGameplayEffect> CustomDamageEffectClass;
};
