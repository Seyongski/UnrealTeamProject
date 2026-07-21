// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_BossJustGuardJudge.generated.h"

class UGameplayEffect;

/**
 * 보스가 '딱 공격하는 순간'(그림의 빨간 선)에 저스트가드를 판정하는 노티파이.
 * 장판(AOE) 없이 이 프레임에서 UBossJustGuardComponent 가 직접 판정한다:
 *  - 대상(전용 가드 플레이어/현재 타겟)이 이 순간 기준 [now - 가드상태시간, now] 안에 G 를 눌렀고
 *    그때 정면이 보스 쪽을 향했으면 성공(무피해). 가드상태시간은 창(Just Guard Window)이 정한다.
 *  - 아니면 실패 -> 대상에게 DamageEffect 를 '직접' 적용(장판 없음).
 *
 * 배치: 같은 몽타주에 JustGuard Window(가드 가능 구간, GuardReady 부여)를 앞에 깔고,
 *       이 노티파이를 '보스 공격 타점' 프레임에 1개 찍는다.
 */
UCLASS(meta = (DisplayName = "Boss: Just Guard Judge (공격 순간 판정)"))
class LOSTARK_API UAnimNotify_BossJustGuardJudge : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	/** 방향 허용 각도(도). 누른 순간 대상 정면과 (대상->보스) 사이 각이 이 이내면 방향 통과 */
	UPROPERTY(EditAnywhere, Category = "JustGuard", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float GuardAngleTolerance = 60.f;

	/** [디버그] 방향 판정을 건너뛰고 타이밍만으로 판정 (키보드 검증용) */
	UPROPERTY(EditAnywhere, Category = "JustGuard|Debug")
	bool bDebugBypassDirection = false;

	/** 실패 시 대상에게 '직접' 적용할 데미지 GE (장판 없이). 미지정이면 데미지 없이 실패 처리만 */
	UPROPERTY(EditAnywhere, Category = "JustGuard")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/** 데미지 공격력계수 (GE 의 SetByCaller Data.Damage 로 전달) */
	UPROPERTY(EditAnywhere, Category = "JustGuard")
	float DamageCoefficient = 1.f;

	/** 성공 시 보스 그로기 진입 (연속 저스트가드의 '마지막' 판정에만 켠다 → 그로기 몽타주로 분기) */
	UPROPERTY(EditAnywhere, Category = "JustGuard")
	bool bGroggyOnSuccess = false;
};
