// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "BossPhaseDataAsset.generated.h"

class UPatternDataAsset;

/** 페이즈 안에서 가중치로 뽑을 패턴 하나 */
USTRUCT(BlueprintType)
struct FBossWeightedPattern
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Pattern")
	TObjectPtr<UPatternDataAsset> Pattern = nullptr;

	/** 선택 가중치 (클수록 자주 나옴) */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern", meta = (ClampMin = "0.0"))
	float Weight = 1.f;
};

/**
 * 한 페이즈 = 패턴 가중치 리스트 + 진입 조건 + 페이즈 태그.
 * 분기는 패턴 '안'에서 처리하므로, 페이즈 레벨은 가중치 랜덤 선택만 한다.
 * 맵은 그대로, 체력에 따라 이 애셋만 교체.
 */
UCLASS(BlueprintType)
class LOSTARK_API UBossPhaseDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 이 페이즈를 나타내는 태그 (Boss.Phase.1 ~ 4). 전환 시 보스 ASC에 루스태그로 부여됨 */
	UPROPERTY(EditDefaultsOnly, Category = "Phase")
	FGameplayTag PhaseTag;

	/** 현재 체력(%)이 이 값 이하가 되면 이 페이즈로 진입. Phases 배열은 이 값 내림차순(100,75,50,25) */
	UPROPERTY(EditDefaultsOnly, Category = "Phase", meta = (ClampMin = "0", ClampMax = "100"))
	float EnterHealthPercent = 100.f;

	/** 이 페이즈 진입 시 1회 실행할 전환 기믹 패턴 (비우면 스킵) */
	UPROPERTY(EditDefaultsOnly, Category = "Phase")
	TObjectPtr<UPatternDataAsset> TransitionGimmick = nullptr;

	/**
	 * 이 페이즈 동안 약점포착(State.Boss.WeakPointExposed) 유지 여부.
	 * 켜면 페이즈 진입 시점부터 '어디서 때려도' 백/헤드어택 보너스가 적용된다
	 * (판정 규칙은 UBossCombatStatics::EvaluatePositionalBonus 참고).
	 *
	 * 끈 페이즈로 넘어가면 이 컴포넌트가 부여한 태그는 회수된다. 단 지형 파괴
	 * (ABossRaidGameMode::DestroySlice)로 붙은 약점포착은 별개 경로라 유지된다.
	 * 카운터 판정은 약점포착과 무관하게 항상 실제 헤드 존만 인정한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Phase")
	bool bWeakPointExposed = false;

	/** 이 페이즈에서 사용할 패턴들 (가중치 랜덤 선택) */
	UPROPERTY(EditDefaultsOnly, Category = "Phase")
	TArray<FBossWeightedPattern> Patterns;

	/** 가중치에 따라 패턴 하나 선택 (없으면 nullptr) */
	UPatternDataAsset* PickRandomWeighted() const;
};
