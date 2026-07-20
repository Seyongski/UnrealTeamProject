// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Boss/Gimmick/BossTerrainGimmickComponent.h"	// EStaggerResolve
#include "AnimNotify_BossBeginStaggerPhase.generated.h"

/**
 * 무력화 페이즈를 시작하는 범용 노티파이 (서버).
 * 게이지를 RequiredAmount 로 채우고 UI 태그를 세운다. 팀원이 E/스킬로 0까지 깎으면 Resolve 처리.
 *
 * - 기믹 시작은 'Boss Gimmick: Spawn Tower'(bBeginStaggerPhase)가 이미 처리하므로,
 *   이 노티파이는 주로 '잡기 구출 게이지'(Resolve=GrabRelease)를 잡기 몽타주에 배치할 때 쓴다.
 * - 같은 보스 StaggerGauge/WBP_StaggerGauge 를 공유하고 요구량만 다르다.
 */
UCLASS(meta = (DisplayName = "Boss Begin Stagger Phase"))
class LOSTARK_API UAnimNotify_BossBeginStaggerPhase : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	/** 무력화 요구량(=게이지 최대치). 패턴마다 다르게 설정 */
	UPROPERTY(EditAnywhere, Category = "Stagger", meta = (ClampMin = "1.0"))
	float RequiredAmount = 100.f;

	/** 게이지 0 도달 시 처리: 그로기(기믹) / 잡기 해제(구출) */
	UPROPERTY(EditAnywhere, Category = "Stagger")
	EStaggerResolve Resolve = EStaggerResolve::GrabRelease;
};
