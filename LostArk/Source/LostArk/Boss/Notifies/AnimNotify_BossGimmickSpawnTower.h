// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_BossGimmickSpawnTower.generated.h"

/**
 * 지형파괴 기믹 라운드 시작 노티파이.
 * 기믹 패턴(TransitionGimmick)의 시작 몽타주(포효 등) 프레임에 배치.
 *
 * 서버에서 UBossTerrainGimmickComponent::SpawnGimmickTower 호출:
 *  - 4개 지정 위치 중 (지형 미파괴 + 타워 없음) 랜덤 선정 -> 이번 라운드 파괴 대상 슬라이스 확정
 *  - 그 위치에 감전 타워 스폰
 * 이어서 (기본값) 무력화 페이즈 시작: 게이지 리필 + 클라 UI 게이지 표시 태그.
 */
UCLASS(meta = (DisplayName = "Boss Gimmick: Spawn Tower"))
class LOSTARK_API UAnimNotify_BossGimmickSpawnTower : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	/** 타워 스폰과 동시에 무력화 페이즈도 시작할지 (게이지 리필 + UI 태그) */
	UPROPERTY(EditAnywhere, Category = "Gimmick")
	bool bBeginStaggerPhase = true;

	/** 이번 기믹의 무력화 요구량(=게이지 최대치). 패턴마다 다르게 설정. 게이지 0 -> 그로기 */
	UPROPERTY(EditAnywhere, Category = "Gimmick", meta = (EditCondition = "bBeginStaggerPhase", ClampMin = "1.0"))
	float StaggerRequiredAmount = 100.f;
};
