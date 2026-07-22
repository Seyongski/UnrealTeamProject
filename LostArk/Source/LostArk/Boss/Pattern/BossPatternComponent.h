// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "Engine/StreamableManager.h"
#include "BossPatternComponent.generated.h"

class UBossPhaseDataAsset;
class UBossPatternAbility;
class UPatternDataAsset;
class UAbilitySystemComponent;
class UBossTargetingComponent;

/**
 * 보스 패턴 흐름 브레인.
 * - 페이즈별 패턴 리스트(UBossPhaseDataAsset)를 들고 체력에 따라 교체
 * - 현재 페이즈에서 가중치 랜덤으로 패턴을 뽑아 GAS(UBossPatternAbility)로 실행
 * - 페이즈 전환은 '지연': 현재 패턴 완주 후 태그 교체 + 전환 기믹 -> 다음 페이즈
 * - 현재 페이즈는 보스 ASC의 루스 태그(Boss.Phase.N)로도 노출된다
 */
UCLASS(ClassGroup = (Boss), meta = (BlueprintSpawnableComponent))
class LOSTARK_API UBossPatternComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossPatternComponent();

	/** 페이즈 목록. EnterHealthPercent 내림차순(100,75,50,25) -> index0 = 1페이즈 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern")
	TArray<TObjectPtr<UBossPhaseDataAsset>> Phases;

	/** 데이터 주도 패턴에 쓸 기본 어빌리티 클래스 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern")
	TSubclassOf<UBossPatternAbility> DefaultPatternAbility;

	/** 패턴 어빌리티를 ASC에 부여. ASC 초기화(InitAbilityActorInfo) 이후에 호출할 것 */
	void InitializePatterns();

	/** 전투 시작 -> 1페이즈부터 흐름 시작 */
	UFUNCTION(BlueprintCallable, Category = "Boss|Pattern")
	void StartCombat();

	/**
	 * 전투 정지 (보스 사망 등). 이후 어빌리티 종료 콜백이 와도 다음 패턴을 돌리지 않고,
	 * 발동 재시도 타이머도 멈춘다. 진행 중 어빌리티 취소는 호출자가 ASC.CancelAllAbilities 로.
	 */
	UFUNCTION(BlueprintCallable, Category = "Boss|Pattern")
	void StopCombat();

	/** 보스가 GAS 체력 변화 델리게이트에서 호출. Percent = 0~100. 전환은 예약만(지연 실행) */
	UFUNCTION(BlueprintCallable, Category = "Boss|Pattern")
	void NotifyHealthPercent(float HealthPercent);

	/** 현재 실행할 패턴 데이터 (어빌리티가 읽어감) */
	UPatternDataAsset* GetActivePatternData() const { return ActivePatternData; }

	/** 패턴 어빌리티가 종료되면 호출 (다음 패턴/페이즈 결정) */
	void OnPatternAbilityFinished();

private:
	UAbilitySystemComponent* GetASC() const;

	void EnterPhase(int32 PhaseIndex);
	void SwapPhaseTag(const FGameplayTag& NewPhaseTag);
	void RunNextPattern();
	void RunPatternData(UPatternDataAsset* Data);
	void ActivateAbilityNow();

	/** 이 페이즈에서 쓸 몽타주들을 비동기 프리로드하고, 이전 페이즈 핸들은 해제(메모리 반환) */
	void PreloadPhaseAssets(const UBossPhaseDataAsset* Phase);

	UPROPERTY(Transient)
	TObjectPtr<UPatternDataAsset> ActivePatternData;

	int32 CurrentPhaseIndex = INDEX_NONE;
	int32 PendingPhaseIndex = INDEX_NONE;	// 예약된 전환 대상 (현재 패턴 종료 후 반영)
	bool bRunningTransition = false;		// 지금 실행 중인 게 페이즈 전환 기믹인지
	bool bCombatStopped = false;			// StopCombat 이후 (사망 등) — 패턴 재개 금지
	FGameplayTag CurrentPhaseTag;			// 현재 ASC에 부여 중인 페이즈 태그

	FGameplayAbilitySpecHandle PatternAbilityHandle;
	FTimerHandle ActivateRetryTimer;			// 발동 실패(그로기 등) 시 재시도 타이머
	TSharedPtr<FStreamableHandle> PhasePreloadHandle;	// 현재 페이즈 몽타주 로드 유지 핸들

	UPROPERTY(Transient)
	TObjectPtr<UBossTargetingComponent> CachedTargeting;	// 타겟 선정용 (InitializePatterns에서 캐시)
};
