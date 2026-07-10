// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossCounterComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
struct FGameplayEventData;

/** 카운터 창 종류 (AnimNotifyState_BossCounter 인스턴스에서 편집) */
UENUM(BlueprintType)
enum class EBossCounterType : uint8
{
	Counter			UMETA(DisplayName = "카운터 (1명)"),
	MultiCounter	UMETA(DisplayName = "협동 카운터 (N명)"),
	FakeCounter		UMETA(DisplayName = "가짜 카운터 (치면 실패)"),
};

/**
 * 보스 공통 카운터 판정 컴포넌트 (서버 전용 로직).
 *
 * 역할 분리:
 *  - AnimNotifyState_BossCounter : 창을 열고 닫기만 한다 (노티파이 객체는 몽타주에 1개라 상태 저장 금지)
 *  - 이 컴포넌트              : 창 상태/히트 카운트 보유 + 판정 + 결과를 '태그로만' 발행
 *  - 라우팅                  : 전부 PatternDataAsset 의 Branch(TagQuery) 가 담당 (인터럽트 경로 단일화)
 *
 * 판정 규칙:
 *  - 모든 카운터는 (약점포착과 무관하게) 공격자가 '실제 헤드 존'에 있어야 성립
 *  - Counter      : 1명 적중 -> 성공 (그로기 GE + PatternResult.Countered)
 *  - MultiCounter : 고유 플레이어 N명 적중 -> 성공 (인원은 창 열 때 주입)
 *  - FakeCounter  : 1명이라도 치면 기믹 실패 (PatternResult.CounterFailed + FakeCountered)
 *
 * 결과 태그 수명 (설계 핵심):
 *  - Countered     : 패턴 종료까지 (BossPatternAbility::EndAbility 의 PatternResult 스윕이 정리)
 *  - FakeCountered : '이 창에서 침' 즉시 분기용. 창이 닫힐 때 이 컴포넌트가 제거
 *                    -> 앞 몽타주의 가짜 히트가 뒤 스텝의 분기를 오발시키지 않는다
 *  - CounterFailed : 패턴 종료까지. 이 태그가 있으면 OpenWindow 자체를 무시
 *                    -> 실패 확정 후 재생되는 몽타주들에 박힌 카운터 창이 자동으로 죽는다
 *
 * 플레이어 쪽 사용법 (결합도 최소):
 *  - 카운터 스킬 적중 시 SendGameplayEventToActor(보스, Event.Boss.CounterHit, Instigator=플레이어)
 *  - 또는 이 컴포넌트의 NotifyCounterAttackHit(플레이어) 직접 호출. 판정은 전부 보스 쪽에서 한다
 */
UCLASS(ClassGroup = (Boss), meta = (BlueprintSpawnableComponent))
class LOSTARK_API UBossCounterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossCounterComponent();

	/** 카운터 창 열기 (AnimNotifyState_BossCounter NotifyBegin). CounterFailed 상태면 무시됨 */
	void OpenWindow(EBossCounterType Type, int32 InRequiredPlayers = 1);

	/** 카운터 창 닫기 (NotifyEnd + 패턴 스텝 정리에서 호출, 중복 호출 안전) */
	void CloseWindow();

	/** 카운터 스킬 적중 보고 (Event.Boss.CounterHit 수신 시 자동 호출되지만 직접 호출도 가능) */
	UFUNCTION(BlueprintCallable, Category = "Boss|Counter")
	void NotifyCounterAttackHit(AActor* Attacker);

	UFUNCTION(BlueprintPure, Category = "Boss|Counter")
	bool IsWindowOpen() const { return bWindowOpen; }

	/** 카운터 성공 시 적용할 그로기 GE (State.Boss.Groggy 를 지속시간 동안 부여) */
	UPROPERTY(EditAnywhere, Category = "Boss|Counter")
	TSubclassOf<UGameplayEffect> GroggyEffectClass;

	/**
	 * 그로기 총 지속시간(초) = 그로기 시작+루프 몽타주가 도는 시간.
	 * 만료로 Groggy 태그가 빠지는 순간 루프 스텝의 'NOT State.Boss.Groggy' 분기가 종료 몽타주로 넘어간다
	 */
	UPROPERTY(EditAnywhere, Category = "Boss|Counter", meta = (ClampMin = "0.1"))
	float GroggyDuration = 8.f;

protected:
	virtual void BeginPlay() override;

private:
	UAbilitySystemComponent* GetASC() const;

	/** Event.Boss.CounterHit 수신 -> Instigator 로 판정 위임 */
	void HandleCounterEvent(const FGameplayEventData* Payload);

	/** 진짜/협동 카운터 성공 처리: 그로기 GE 먼저, 분기 트리거 태그(Countered)는 마지막 */
	void ApplyCounterSuccess();

	bool bWindowOpen = false;
	EBossCounterType WindowType = EBossCounterType::Counter;
	int32 RequiredPlayers = 1;

	/** 협동 카운터: 이 창에서 유효 적중한 고유 플레이어 (창 닫힐 때 리셋) */
	TSet<TWeakObjectPtr<AActor>> UniqueAttackers;
};
