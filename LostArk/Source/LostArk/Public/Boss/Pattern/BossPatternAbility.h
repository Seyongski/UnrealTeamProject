// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "BossPatternTypes.h"
#include "BossPatternAbility.generated.h"

class UPatternDataAsset;
class UBossPatternComponent;

/**
 * 패턴 1개를 실행하는 GameplayAbility.
 * 데이터 주도: 브레인(UBossPatternComponent)이 넣어준 PatternData 로 동작하는 하나의 클래스.
 * 특수 패턴만 이 클래스를 상속해 EvaluateResult/RunPattern 을 오버라이드하면 된다.
 */
UCLASS()
class LOSTARK_API UBossPatternAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UBossPatternAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	/** 성공/실패 판정. 기본: FailTag 가 보스에 있으면 Fail. 특수 패턴은 오버라이드 */
	virtual EPatternResult EvaluateResult() const;

	/** 실제 연출 실행. 현재는 (Telegraph+Recovery) 타이머 스텁 -> 이후 몽타주/노티파이로 교체 */
	virtual void RunPattern();

	/** 연출 종료 시점: 결과 판정 후 브레인에 보고하고 어빌리티 종료 */
	void FinishPattern();

	/** 이번 활성화에서 실행 중인 패턴 (브레인에서 주입) */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Pattern")
	TObjectPtr<UPatternDataAsset> PatternData;

	/** 소유 보스의 브레인 컴포넌트 캐시 */
	UPROPERTY(Transient)
	TObjectPtr<UBossPatternComponent> Brain;

	FTimerHandle PatternTimer;
};
