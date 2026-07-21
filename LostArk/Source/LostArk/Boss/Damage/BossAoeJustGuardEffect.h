// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Boss/Damage/BossAoeEffect.h"
#include "BossAoeJustGuardEffect.generated.h"

class ABossPatternActorBase;

/**
 * 저스트가드 '적중 시 행동'. 설계: docs/09_JUSTGUARD_PATTERN.md
 *
 * 일반 장판은 히트 순간 즉시 데미지지만, 저스트가드 장판은:
 *  1) T(다 차오른 순간, OnHit): 도형 안 플레이어를 '데미지 없이' 잡아둔다(PendingTargets).
 *  2) 파괴 지연(OnFinish=true): 판정 시각 J = T + JudgmentDelay 까지 장판을 살려둔다
 *     (잡기 이펙트가 파괴를 지연시키는 것과 동일한 메커니즘).
 *  3) 판정 시각 J(ResolveGuards): 보스의 UBossJustGuardComponent 에 각 대상을 판정 위임 ->
 *     성공이면 데미지 스킵(+첫 성공 시 PatternResult.JustGuarded), 실패면 데미지 적용. 그 후 파괴.
 *
 * 'T 에 바로 데미지를 주지 않고 J 에 판정'하는 이유:
 *  성공 윈도우 [J - Duration, J] 는 판정 시각 '직전'에 붙는 구간이라, J 를 뒤로 미는 엇박
 *  패턴에선 T 이후의 입력까지 봐야 한다. 그래서 데미지 확정을 J 까지 미룬다.
 *  (그 전에 누른 입력은 컴포넌트가 기록해 두었다가 J 에 함께 평가)
 *
 * 판정 대상 스냅샷: T(다 차오른 순간) 에 도형 안에 있던 플레이어만. 장판 밖 플레이어는
 *  판정 로직에 아예 들어가지 않는다(성공도 실패도 없음). T 이후 빠져나가도 대상 유지.
 *
 * 예고 변형 (장판 BP 설정, docs §2):
 *  - A. 장판 보임  : bInstant=false + bTelegraphFill=true -> 중앙에서 CastTime 동안 차오름
 *  - B. 장판 안보임: bInstant=true (예고 메시만 숨김, 판정 타이밍은 CastTime 그대로)
 *  판정 타이밍(글로우~판정 시각 J)을 덮는 저스트가드 창(AnimNotifyState_BossJustGuard)이
 *  같은 몽타주에 배치돼 있어야 GuardReady 가 부여된다.
 */
UCLASS(DisplayName = "저스트가드 (Just Guard)")
class LOSTARK_API UBossAoeJustGuardEffect : public UBossAoeEffect
{
	GENERATED_BODY()

public:
	/** 판정 시각(초록선) J 를 T(다 차오른 순간)보다 뒤로 미는 엇박 오프셋(초). J = T + 이 값.
	 *  0(기본)이면 다 차는 순간이 곧 판정. 키우면 '장판이 다 차고 사라진 뒤 보스 모션을 보고
	 *  엇박에 눌러야 하는' 고난이도 패턴이 된다 (데미지 확정도 J 로 함께 밀림) */
	UPROPERTY(EditDefaultsOnly, Category = "JustGuard", meta = (ClampMin = "0.0"))
	float JudgmentDelay = 0.f;

	/** 성공 윈도우(파란 박스) 길이(초). 윈도우는 항상 판정 시각 J 직전에 붙는다: [J - 이 값, J].
	 *  Delay=0 기준으론 '다 차기 직전 ~ 다 차는 순간'에 누르면 성공 (미리 누르는 유예 역할 포함) */
	UPROPERTY(EditDefaultsOnly, Category = "JustGuard", meta = (ClampMin = "0.0"))
	float GuardWindowDuration = 0.3f;

	/** 방향 판정 허용 각도(도). 플레이어 정면과 (플레이어->장판중심) 사이 각이 이 이내면 통과 */
	UPROPERTY(EditDefaultsOnly, Category = "JustGuard", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float GuardAngleTolerance = 60.f;

	/**
	 * 지형파괴 기믹 전용 '전부-아니면-전무' 모드.
	 * 끄면(기본): 대상별 독립 판정 — 성공한 사람만 무피해, 실패한 사람만 데미지 (일반 저스트가드).
	 * 켜면: 지정 1명(GuardReady 보유자)의 성공/실패로 장판 전체를 결정한다.
	 *   - 성공 -> 장판 안 전원 무피해 (+ PatternResult.JustGuarded)
	 *   - 실패 -> 장판 안 전원 데미지+넉백 (넘어짐) + PatternResult.JustGuardFailed 발행
	 *     -> 패턴 Branch 가 43(부수기)으로 분기, 이후 저스트가드 창은 게이트로 무시됨.
	 * (지정 1명만 GuardReady 를 받으므로 '성공'은 지정자만 낼 수 있다 = bOnlyCurrentTarget 창과 함께 쓸 것)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "JustGuard")
	bool bAllOrNothingByGuard = false;

	/**
	 * 성공 시 보스 그로기 진입 여부. 연속 저스트가드에서 '마지막' 창에만 켠다.
	 *  - 끄면(기본): 성공 시 PatternResult.JustGuarded 만 발행 -> 다음 저스트가드 스텝으로 분기 (예: 2-3 성공 -> 2-4)
	 *  - 켜면: 성공 시 그로기 GE(State.Boss.Groggy)까지 적용 -> 그로기 몽타주로 분기 (예: 2-4 성공 -> 그로기)
	 * 그로기 지속시간은 UBossJustGuardComponent::GroggyDuration 에서 조정.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "JustGuard")
	bool bGroggyOnSuccess = false;

	/** [디버그] 방향 판정을 건너뛰고 타이밍만으로 판정 (키보드 G 디버그 검증용) */
	UPROPERTY(EditDefaultsOnly, Category = "JustGuard|Debug")
	bool bDebugBypassDirection = false;

	/** [디버그] 대상별 성공/실패를 화면에 표시 */
	UPROPERTY(EditDefaultsOnly, Category = "JustGuard|Debug")
	bool bShowDebugResult = true;

	// ─── UBossAoeEffect ───
	virtual void OnHit(ABossPatternActorBase* Aoe, AActor* Target) override;
	virtual bool OnFinish(ABossPatternActorBase* Aoe) override;
	virtual void OnEndPlay(ABossPatternActorBase* Aoe) override;

private:
	/** 판정 시각 J: 잡아둔 대상 전원 판정 -> 실패자 데미지 -> 장판 파괴 */
	void ResolveGuards();

	/** T 순간 도형 안에 있던 대상 (데미지 보류. T 이후 빠져나가도 유지) */
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<AActor>> PendingTargets;

	/** 판정 타이머 콜백이 참조할 소유 장판 */
	UPROPERTY(Transient)
	TWeakObjectPtr<ABossPatternActorBase> OwningAoe;

	FTimerHandle ResolveTimerHandle;

	/** T(월드 초) = 장판이 다 차오른 순간. 판정 타이밍 기준 */
	float HitTime = -1.f;

	bool bResolved = false;
};
