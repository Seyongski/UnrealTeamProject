// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "PatternDataAsset.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UBossPatternAbility;

/**
 * 패턴 1개의 "무엇을" 정의하는 원자적 데이터. (총 ~55개)
 * 흐름(다음 패턴)은 여기 두지 않는다 -> 여러 페이즈에서 재사용하기 위함.
 * 흐름은 UBossPhaseDataAsset 의 노드가 담당.
 */
UCLASS(BlueprintType)
class LOSTARK_API UPatternDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 식별/디버그용 이름 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern")
	FName PatternId;

	/** 실행 로직이 특수하면 전용 어빌리티로 오버라이드 (비우면 브레인의 기본 PatternAbility 사용) */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern")
	TSubclassOf<UBossPatternAbility> AbilityOverride;

	/** 재생할 몽타주 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Visual")
	TObjectPtr<UAnimMontage> Montage;

	/** 예고(선딜) 시간 초 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Timing")
	float TelegraphTime = 1.0f;

	/** 후딜 시간 초 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Timing")
	float RecoveryTime = 0.5f;

	/** 적중 시 적용할 데미지 GameplayEffect */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/** 시전 중 보스에게 부여할 태그 (예: State.Boss.Counterable 카운터 윈도우) */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Tags")
	FGameplayTagContainer GrantedTagsDuringCast;

	/** 패턴 종료 시 이 태그가 보스에 있으면 '실패' 판정 (예: State.Boss.Countered / Groggy) */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Tags")
	FGameplayTag FailTag;
};
