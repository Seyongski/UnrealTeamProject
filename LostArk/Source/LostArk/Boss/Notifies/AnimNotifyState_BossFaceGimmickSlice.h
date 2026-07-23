// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_BossFaceGimmickSlice.generated.h"

/**
 * 구간 동안 보스가 '이번 라운드에 파괴할 지형(기믹 위치)'을 바라보게 회전 (서버).
 * 저스트가드 준비 ~ 지형 파괴 모션까지의 몽타주들에 배치.
 *
 * - 회전 기준은 UBossTerrainGimmickComponent 의 기믹 위치 (파괴 요청 후에도 유지됨)
 * - 액터 회전 복제로 클라에 전파 (타겟 추적 회전과 동일 원리)
 * - 주의: 같은 구간에 TrackTarget 노티파이를 겹치지 말 것 (타겟팅 회전과 서로 싸운다)
 * - NotifyBegin 에서 (기본값) 무력화 페이즈를 종료한다: 레이저 루프가 끝나고
 *   저스트가드 단계로 넘어왔으므로 게이지 UI 를 내린다
 */
UCLASS(meta = (DisplayName = "Boss Gimmick: Face Slice"))
class LOSTARK_API UAnimNotifyState_BossFaceGimmickSlice : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	/** 회전 보간 속도 (클수록 빠르게 돎. 타겟팅 컴포넌트 기본값과 동일 스케일) */
	UPROPERTY(EditAnywhere, Category = "Gimmick", meta = (ClampMin = "0.0"))
	float RotationInterpSpeed = 6.f;

	/** 구간 시작 시 무력화 페이즈 종료 (게이지 UI 내림). 저스트가드 단계 진입 몽타주에서만 끄면 됨 */
	UPROPERTY(EditAnywhere, Category = "Gimmick")
	bool bEndStaggerPhaseOnBegin = true;
};
