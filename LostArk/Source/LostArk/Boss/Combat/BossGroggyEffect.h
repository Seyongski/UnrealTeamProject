// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "BossGroggyEffect.generated.h"

/**
 * 카운터 성공 시 보스에 적용되는 그로기 GE.
 * - 지속시간 = SetByCaller(Data.Duration) : BossCounterComponent 의 GroggyDuration 이 실려 온다
 * - 지속 중 State.Boss.Groggy 태그 부여 -> 만료로 태그가 빠지는 순간
 *   그로기 루프 스텝의 'NOT State.Boss.Groggy' 분기가 종료 몽타주로 넘어간다 (타이머 코드 불필요)
 */
UCLASS()
class LOSTARK_API UBossGroggyEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBossGroggyEffect();
};
