// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Boss/Targeting/BossTargetingComponent.h"	// ETargetSelectPolicy
#include "PatternDataAsset.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * 스텝 분기. 보스 ASC의 태그를 TagQuery로 평가해서 참이면 NextStepId로 이동.
 * 몽타주 재생 '중' 조건이 충족되면 즉시 끊고 넘어간다(카운터 성공 -> 그로기 등).
 */
USTRUCT(BlueprintType)
struct FPatternBranch
{
	GENERATED_BODY()

	/** 보스 ASC 태그로 평가할 조건 (예: State.Boss.Countered 보유) */
	UPROPERTY(EditDefaultsOnly, Category = "Branch")
	FGameplayTagQuery Condition;

	/** 조건 충족 시 이동할 스텝 */
	UPROPERTY(EditDefaultsOnly, Category = "Branch")
	FName NextStepId;
};

/**
 * 패턴을 구성하는 몽타주 스텝 하나.
 * 몽타주 1개를 재생하고, 재생 중/종료 시 분기 조건을 평가해 다음 스텝을 정한다.
 */
USTRUCT(BlueprintType)
struct FPatternStep
{
	GENERATED_BODY()

	/** 스텝 식별자 (분기 연결용) */
	UPROPERTY(EditDefaultsOnly, Category = "Step")
	FName StepId;

	/** 이 스텝에서 재생할 몽타주 (소프트 레퍼런스: 현재 페이즈 것만 메모리에 로드됨) */
	UPROPERTY(EditDefaultsOnly, Category = "Step")
	TSoftObjectPtr<UAnimMontage> Montage;

	/** 이 몽타주 적중 시 적용할 데미지 GE (실제 적용은 애님노티파이 타이밍 예정) */
	UPROPERTY(EditDefaultsOnly, Category = "Step")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/** 이 몽타주 재생 중 보스에 부여할 윈도우 태그 (예: State.Boss.Counterable) */
	UPROPERTY(EditDefaultsOnly, Category = "Step")
	FGameplayTagContainer GrantedTagsDuringStep;

	/** 조건 분기 (위에서부터 첫 매치 채택). 재생 중 충족되면 즉시 인터럽트 */
	UPROPERTY(EditDefaultsOnly, Category = "Step")
	TArray<FPatternBranch> Branches;

	/** 분기 안 걸리고 몽타주가 정상 종료되면 이동할 스텝. 비우면 패턴 종료 */
	UPROPERTY(EditDefaultsOnly, Category = "Step")
	FName NextStepDefault;
};

/**
 * 패턴 1개 = 몽타주 스텝들의 분기 그래프. (총 ~55개, 페이즈에서 가중치로 선택되어 재사용)
 * 예) 기본 1->2->3, 2에서 카운터 성공 시 2 도중 끊고 1->2->4->5.
 */
UCLASS(BlueprintType)
class LOSTARK_API UPatternDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 식별/디버그용 이름 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern")
	FName PatternId;

	/** 시작 스텝 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern")
	FName EntryStepId;

	/** 패턴 시작 시 타겟을 새로 선정할지 (false면 직전 타겟 유지) */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Targeting")
	bool bReselectTargetOnStart = true;

	/** 타겟 선정 정책 (근접/원거리/랜덤) */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Targeting", meta = (EditCondition = "bReselectTargetOnStart"))
	ETargetSelectPolicy TargetPolicy = ETargetSelectPolicy::Nearest;

	/** 스텝 그래프 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern")
	TArray<FPatternStep> Steps;

	/** StepId 로 스텝 찾기 (없으면 nullptr) */
	const FPatternStep* FindStep(FName StepId) const;
};
