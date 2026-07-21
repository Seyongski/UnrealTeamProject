// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossJustGuardComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
struct FGameplayEventData;

/** 저스트가드 판정 결과 (실패는 원인별로 구분 — 디버그/연출용) */
UENUM(BlueprintType)
enum class EJustGuardResult : uint8
{
	Success			UMETA(DisplayName = "성공"),
	FailTiming		UMETA(DisplayName = "실패(타이밍)"),
	FailDirection	UMETA(DisplayName = "실패(방향)"),
	FailNoInput		UMETA(DisplayName = "실패(미입력)")
};

/** 한 명의 가드 입력 기록 (창 열림 동안 G 를 누른 순간의 스냅샷) */
USTRUCT()
struct FJustGuardInput
{
	GENERATED_BODY()

	/** 눌린 월드 시각(초). <0 이면 입력 없음 */
	float PressTime = -1.f;

	/** 누른 순간 플레이어 정면(수평, 정규화). 방향 판정 기준 */
	FVector Facing = FVector::ForwardVector;

	/** 누른 순간 플레이어 위치. 장판 중심으로의 방향 계산 기준 */
	FVector Location = FVector::ZeroVector;
};

/** 저스트가드 판정 파라미터 (장판/이펙트가 자기 히트 타이밍·중심과 함께 넘김) */
USTRUCT(BlueprintType)
struct FJustGuardResolveParams
{
	GENERATED_BODY()

	/** T = 예고(CastTime)가 끝나 장판이 다 차오른 순간(월드 초) */
	float HitTime = 0.f;

	/** 판정 시각 J 를 T 보다 뒤로 미는 엇박 오프셋(초). J = T + JudgmentDelay. 0이면 J = T */
	float JudgmentDelay = 0.f;

	/** 성공 윈도우 길이(초). 윈도우는 항상 판정 시각 J 에서 끝난다: [J - Duration, J] */
	float GuardWindowDuration = 0.3f;

	/** 방향 판정 허용 각도(도). 플레이어 정면과 (플레이어->장판중심) 사이 각이 이 이내면 통과 */
	float GuardAngleTolerance = 60.f;

	/** 장판 중심(월드). 방향 판정 기준점 */
	FVector GuardCenter = FVector::ZeroVector;

	/** [디버그] 방향 판정을 건너뜀 (키보드 디버그로 타이밍만 검증할 때) */
	bool bBypassDirection = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJustGuardWindowChanged, bool, bWindowOpen);

/**
 * 보스 공통 저스트가드 판정 컴포넌트 (서버 전용 로직). 설계: docs/09_JUSTGUARD_PATTERN.md
 *
 * 역할 분리 (카운터 컴포넌트와 동일한 구조):
 *  - AnimNotifyState_BossJustGuard : 몽타주 구간 동안 창을 열고 닫기만 한다
 *  - 이 컴포넌트                  : 가드 가능 상태(GuardReady) 부여/회수 + 가드 입력 기록 + 판정
 *  - 장판(BossAoeJustGuardEffect) : 자기 히트 타이밍/중심으로 ResolveGuard 를 호출해
 *                                   성공이면 데미지 스킵, 실패면 데미지 적용
 *
 * 판정 이중 규칙 (판정 시각 J = T(다 차오른 순간) + JudgmentDelay):
 *  - 타이밍 : 누른 시각이 [J - Duration, J] 안 (성공 윈도우는 항상 판정 시각 '직전'에 붙는다)
 *  - 방향   : 누른 순간 플레이어 정면이 (플레이어->장판중심) 기준 GuardAngleTolerance 이내
 *
 * "패턴당 1회" : 창 열릴 때 GuardReady 부여 -> G 입력 시 즉시 소모(제거).
 *               너무 일찍 눌러 타이밍 실패해도 GuardReady 는 소모되므로 다시 못 누른다.
 *
 * 플레이어 쪽 사용법 (결합도 최소):
 *  - 가드(G) 발동 시 SendGameplayEventToActor(보스, Event.Boss.JustGuardInput, Instigator=플레이어)
 *  - 또는 NotifyGuardInput(플레이어) 직접 호출. 방향은 보스가 Instigator 전방으로 읽는다
 */
UCLASS(ClassGroup = (Boss), meta = (BlueprintSpawnableComponent))
class LOSTARK_API UBossJustGuardComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossJustGuardComponent();

	/** 저스트가드 창 열기 (NotifyBegin). 전 플레이어에 GuardReady 부여 + 보스 글로우 태그 */
	void OpenWindow();

	/** 저스트가드 창 닫기 (NotifyEnd, 중복 호출 안전). 남은 GuardReady 회수 + 글로우 태그 제거 */
	void CloseWindow();

	/** 가드(G) 입력 보고 (Event.Boss.JustGuardInput 수신 시 자동 호출되지만 직접 호출도 가능).
	 *  창이 열려 있고 GuardReady 를 가진 플레이어만 유효 -> 기록하고 GuardReady 소모 */
	UFUNCTION(BlueprintCallable, Category = "Boss|JustGuard")
	void NotifyGuardInput(AActor* Player);

	/** 기록된 가드 입력으로 한 명 판정 (장판 이펙트가 히트 타이밍·중심과 함께 호출) */
	EJustGuardResult ResolveGuard(AActor* Player, const FJustGuardResolveParams& Params) const;

	/**
	 * 저스트가드 성공 처리 (장판이 판정 후 호출).
	 *  - PatternResult.JustGuarded 부여 (Branch 조건용). 이 태그는 OpenWindow 에서 창마다 리셋되므로
	 *    '이번 창의 성공'만 의미한다 -> 연속 저스트가드(2-3 성공 시 2-4, 2-4 성공 시 그로기)가 각각 분기된다.
	 *  - bApplyGroggy=true 면 그로기 GE 를 먼저 적용(State.Boss.Groggy)한 뒤 태그를 붙인다.
	 *    (최종 저스트가드 성공 -> 그로기 몽타주 분기. 카운터 성공 경로와 동일한 순서: GE 먼저, 트리거 태그 마지막)
	 */
	void MarkJustGuardedResult(bool bApplyGroggy = false);

	/**
	 * 기믹 실패 확정: 보스에 PatternResult.JustGuardFailed 부여 (1회, 패턴 종료 시 자동 정리).
	 * 전부-아니면-전무 저스트가드에서 지정자가 실패했을 때 장판이 호출 -> 43(부수기) 분기 +
	 * 이후 저스트가드 창 무시 게이트(OpenWindow) 로 작동. (카운터의 CounterFailed 대응)
	 */
	void MarkJustGuardFailedResult();

	UFUNCTION(BlueprintPure, Category = "Boss|JustGuard")
	bool IsWindowOpen() const { return bWindowOpen; }

	/** 해당 플레이어가 아직 가드 가능한지 (GuardReady 보유) — 디버그 메시지 분기용 */
	UFUNCTION(BlueprintPure, Category = "Boss|JustGuard")
	bool HasGuardReady(AActor* Player) const;

	/**
	 * 가드 가능 대상을 이 플레이어 1명으로 제한 (null 이면 전원 허용으로 복귀).
	 * 지형파괴 기믹의 '기믹 대상만 저스트가드 가능' 규칙용 — OpenWindow 전에 세팅할 것.
	 * (창이 열릴 때 GuardReady 자체를 이 플레이어에게만 부여하므로 나머지는 입력이 무시된다)
	 */
	UFUNCTION(BlueprintCallable, Category = "Boss|JustGuard")
	void SetExclusiveGuardPlayer(AActor* Player) { ExclusiveGuardPlayer = Player; }

	/** 전용 가드 대상 해제 (창 닫은 뒤 호출 — 다음 일반 저스트가드 패턴에 새지 않게) */
	UFUNCTION(BlueprintCallable, Category = "Boss|JustGuard")
	void ClearExclusiveGuardPlayer() { ExclusiveGuardPlayer = nullptr; }

	/** 창 열림/닫힘 방송 (보스 '노란 글로우' 연출을 BP에서 여기에 바인딩) */
	UPROPERTY(BlueprintAssignable, Category = "Boss|JustGuard")
	FOnJustGuardWindowChanged OnJustGuardWindowChanged;

	/** 저스트가드 성공(bGroggyOnSuccess 장판) 시 적용할 그로기 GE. 카운터와 동일 (State.Boss.Groggy 부여) */
	UPROPERTY(EditAnywhere, Category = "Boss|JustGuard")
	TSubclassOf<UGameplayEffect> GroggyEffectClass;

	/**
	 * 그로기 총 지속시간(초) = 그로기 시작+루프 몽타주가 도는 시간.
	 * 만료로 Groggy 태그가 빠지는 순간 루프 스텝의 'NOT State.Boss.Groggy' 분기가 종료 몽타주로 넘어간다.
	 */
	UPROPERTY(EditAnywhere, Category = "Boss|JustGuard", meta = (ClampMin = "0.1"))
	float GroggyDuration = 6.f;

protected:
	virtual void BeginPlay() override;

private:
	UAbilitySystemComponent* GetASC() const;

	/** Event.Boss.JustGuardInput 수신 -> Instigator 로 위임 */
	void HandleGuardEvent(const FGameplayEventData* Payload);

	/** 대상 플레이어 ASC 조회 */
	static UAbilitySystemComponent* GetPlayerASC(AActor* Player);

	bool bWindowOpen = false;

	/** 이번 창에서 유효 입력한 플레이어별 기록 (창 열 때 리셋) */
	TMap<TWeakObjectPtr<AActor>, FJustGuardInput> GuardInputs;

	/** GuardReady 를 부여한 플레이어들 (창 닫을 때 남은 태그 회수) */
	TSet<TWeakObjectPtr<AActor>> ReadyPlayers;

	/** 지정 시 이 플레이어에게만 GuardReady 부여 (기믹 대상 전용 저스트가드) */
	TWeakObjectPtr<AActor> ExclusiveGuardPlayer;
};
