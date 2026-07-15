// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_BossJustGuard.generated.h"

/**
 * 몽타주 구간 동안 보스의 저스트가드 창을 여는 노티파이 스테이트. 설계: docs/09_JUSTGUARD_PATTERN.md
 *
 * - 창 열림/닫힘만 담당. 가드 부여/판정/결과는 전부 UBossJustGuardComponent (서버 전용)
 * - 배치 계약: 이 구간은 '글로우 시작 ~ 판정 시각 J(= T + JudgmentDelay)' 까지 덮어야 한다.
 *   창이 닫히면 GuardReady 가 회수되므로, J 를 뒤로 미는 엇박 패턴에선 구간을
 *   장판이 다 차는 지점(T) 너머까지 넉넉히 잡을 것.
 * - 보스 '노란 글로우' 연출은 이 노티파이가 아니라 컴포넌트의 창 열림 태그
 *   (State.Boss.JustGuardable) / OnJustGuardWindowChanged 델리게이트에 BP에서 바인딩한다.
 */
UCLASS(meta = (DisplayName = "Boss Just Guard Window"))
class LOSTARK_API UAnimNotifyState_BossJustGuard : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;
};
