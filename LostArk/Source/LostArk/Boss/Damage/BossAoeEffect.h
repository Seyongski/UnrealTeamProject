// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BossAoeEffect.generated.h"

class ABossPatternActorBase;

/**
 * 장판 '적중 시 행동'을 도형에서 분리한 교체형 오브젝트.
 *
 * 도형(원/사각/부채꼴)은 ABossPatternActorBase 파생 클래스로, '행동'(데미지/잡기/전하스왑)은
 * 이 UObject 로 분리한다. 도형 BP 의 OnHitEffect 슬롯에 인스턴스로 지정하면
 * '잡기 × 원', '전하스왑 × 부채꼴' 같은 조합이 상속 없이 자유롭게 된다.
 * 미지정(null)이면 베이스가 기본 데미지+상태이상 GE 를 적용한다.
 *
 * 수명주기 훅 (전부 서버 권위에서 호출):
 *  - OnHit    : 판정 통과한 대상 1명마다
 *  - OnFinish : 장판 수명 종료 시. true 반환하면 즉시 파괴를 막고, 파괴 타이밍을 이 오브젝트가
 *               직접 제어한다(잡기 유지 후 Aoe->DestroyAoeNow()). false면 베이스가 바로 파괴.
 *  - OnEndPlay: 액터 파괴 시 정리(부착 해제 등 안전 복구)
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class LOSTARK_API UBossAoeEffect : public UObject
{
	GENERATED_BODY()

public:
	/** 판정 통과한 대상 1명 적중 (서버). 기본 데미지가 필요하면 Aoe->ApplyDamageAndStatus(Target) 호출 */
	virtual void OnHit(ABossPatternActorBase* Aoe, AActor* Target) {}

	/** 장판 수명 종료. true=파괴 지연(이 오브젝트가 이후 Aoe->DestroyAoeNow()로 파괴), false=즉시 파괴 */
	virtual bool OnFinish(ABossPatternActorBase* Aoe) { return false; }

	/** 액터 파괴 시 정리(잡은 대상 복구 등) */
	virtual void OnEndPlay(ABossPatternActorBase* Aoe) {}

	/**
	 * 이 행동이 붙은 장판은 높이(Z) 판정을 무시해야 하는가.
	 * true면 베이스 PerformHitCheck 가 XY 도형만으로 판정한다(보스 크기 변동에 강건).
	 * 잡기처럼 '범위 안이면 무조건 잡아야' 하는 행동이 override 해서 true 를 돌려준다.
	 */
	virtual bool IgnoresAoeHeight() const { return false; }
};
