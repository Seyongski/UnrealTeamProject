// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Boss/Notifies/AnimNotify_BossSpawnAoe.h"
#include "AnimNotify_BossGimmickSpawnAoe.generated.h"

/**
 * '이번 기믹 라운드의 지형(기믹 위치)' 위에 장판을 스폰하는 노티파이.
 * 일반 스폰 노티파이(보스/소켓 기준)와 달리 위치를 UBossTerrainGimmickComponent 의
 * 기믹 위치로 강제한다. 회전은 (기믹 위치 -> 보스) 방향 = 저스트가드 시 플레이어가
 * 보스를 바라보고 서는 축과 일치.
 *
 * 용도 (지형파괴 기믹):
 *  - 저스트가드 장판 : AoeClass = 저스트가드 BP (OnHitEffect=UBossAoeJustGuardEffect, bTelegraphFill)
 *  - 실패 넉다운 장판: AoeClass = 넉다운 BP (bInstant, CastTime 0 부근, StatusEffects=[넉다운 GE])
 */
UCLASS(meta = (DisplayName = "Boss Gimmick: Spawn AOE At Slice"))
class LOSTARK_API UAnimNotify_BossGimmickSpawnAoe : public UAnimNotify_BossSpawnAoe
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** 스폰 위치를 기믹 위치로 강제했으므로, 액터 BP 의 SpawnOrigin 설정도 '스폰 트랜스폼 그대로'로 덮는다 */
	virtual void ConfigureAoe(ABossPatternActorBase* Aoe) const override;
};
