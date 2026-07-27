// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Boss/Damage/BossPatternActorBase.h"	// FBossAoeCommonOverride
#include "BossGimmickTower.generated.h"

class ABossPatternActorBase;
class UStaticMeshComponent;

/**
 * 지형파괴 기믹 타워 (기믹 액터).
 *
 * 규칙:
 *  - 움직이지 않는다. 일정 주기마다 '현재 생존 플레이어 중 랜덤 1명'을 향해
 *    감전 Rect 장판(ShockAoeClass)을 스폰해 플레이어를 방해한다.
 *  - 파괴 방법은 단 하나: bCanHitGimmickTargets 를 켠 보스의 레이저 장판에 피격(OnBossLaserHit).
 *    플레이어 공격으로는 파괴 불가 (콜리전/데미지 경로 자체가 없음).
 *  - 서 있는 슬라이스가 지형파괴되면 (레이저에 안 죽었어도) 자연스럽게 함께 사라진다.
 *  - 스폰/판정/파괴는 서버 권위. 파괴 연출은 멀티캐스트로 전 머신 BP 이벤트 호출.
 *
 * 스폰은 UBossTerrainGimmickComponent::SpawnGimmickTower 가 담당
 * (지정 위치 중 파괴되지 않았고 이번에 파괴되지도 않을 슬라이스에서 랜덤 선택).
 */
UCLASS(Blueprintable)
class LOSTARK_API ABossGimmickTower : public AActor
{
	GENERATED_BODY()

public:
	ABossGimmickTower();

	/** 스폰 직후 기믹 컴포넌트가 호출: 보스(장판 시전자/데미지 귀속) + 서 있는 슬라이스 인덱스 주입 */
	void InitTower(AActor* InBoss, int32 InSliceIndex);

	/** 보스 레이저 장판 적중 (ABossPatternActorBase::PerformHitCheck 가 호출) -> 타워 파괴 */
	void OnBossLaserHit();

	/** 서 있는 슬라이스 인덱스 (지형파괴 연동) */
	UFUNCTION(BlueprintPure, Category = "Gimmick")
	int32 GetSliceIndex() const { return SliceIndex; }

	/** 파괴 진행 중인지 (판정 중복 방지용) */
	bool IsDying() const { return bDying; }

protected:
	virtual void BeginPlay() override;

	/** 파괴 연출 훅 (전 머신에서 호출). bByLaser: true=레이저 파괴, false=지형파괴로 소멸 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick")
	void OnTowerDestroyed(bool bByLaser);

	/** 타워 외형 (BP에서 메시 지정). 루트라 이동 없음 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick")
	TObjectPtr<UStaticMeshComponent> TowerMesh;

	/** 주기 스폰할 감전 Rect 장판 클래스 (BP_Aoe_Rect 계열 + StatusEffects=[감전 GE]) */
	UPROPERTY(EditDefaultsOnly, Category = "Gimmick|Shock")
	TSubclassOf<ABossPatternActorBase> ShockAoeClass;

	/** 감전 장판 스폰 주기(초) */
	UPROPERTY(EditDefaultsOnly, Category = "Gimmick|Shock", meta = (ClampMin = "0.1"))
	float ShockSpawnInterval = 5.f;

	/** 첫 감전 장판까지의 대기(초). 스폰 직후 바로 쏘지 않게 */
	UPROPERTY(EditDefaultsOnly, Category = "Gimmick|Shock", meta = (ClampMin = "0.0"))
	float FirstShockDelay = 2.f;

	/** 감전 장판 공격력계수 (GE SetByCaller Data.Damage) */
	UPROPERTY(EditDefaultsOnly, Category = "Gimmick|Shock")
	float ShockDamageCoefficient = 0.5f;

	/** 켜면 아래 공통값으로 장판 BP 기본값(시전/유지/틱 등)을 덮어씀 */
	UPROPERTY(EditDefaultsOnly, Category = "Gimmick|Shock")
	bool bOverrideCommon = false;

	UPROPERTY(EditDefaultsOnly, Category = "Gimmick|Shock", meta = (EditCondition = "bOverrideCommon"))
	FBossAoeCommonOverride CommonOverride;

	/** 파괴 연출(멀티캐스트) 후 액터 실제 소멸까지의 유예(초) */
	UPROPERTY(EditDefaultsOnly, Category = "Gimmick", meta = (ClampMin = "0.0"))
	float DestroyFXDelay = 0.3f;

private:
	/** 감전 장판 1회 스폰: 생존 플레이어 랜덤 1명 방향으로 타워 위치에서 */
	void SpawnShockAoe();

	/** GameState 슬라이스 파괴 방송 구독 (클라 복제 지연 대비 재시도 — ArenaSliceActor 와 동일 관용구) */
	void TryBindToGameState();

	/** 슬라이스 파괴 방송 수신: 내 슬라이스가 파괴됐으면 소멸 */
	UFUNCTION()
	void HandleSlicesChanged();

	/** 공통 파괴 경로: 연출 방송 + 수명 예약 (중복 호출 안전) */
	void DestroyTower(bool bByLaser);

	/** 파괴 연출을 전 머신에 방송 (BP 이벤트 호출) */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastTowerDestroyed(bool bByLaser);

	/** 감전 장판 시전자로 쓸 보스 (데미지 귀속/바닥 폴백 기준) */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> Boss;

	int32 SliceIndex = INDEX_NONE;
	bool bDying = false;

	FTimerHandle ShockSpawnTimer;
	FTimerHandle BindRetryTimer;
};
