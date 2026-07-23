// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossTerrainGimmickComponent.generated.h"

class ABossGimmickTower;
class UAbilitySystemComponent;
class UGameplayEffect;
struct FOnAttributeChangeData;

/**
 * 지형파괴 기믹 오케스트레이터 (서버 전용 로직).
 *
 * 기믹 한 라운드 = 페이즈 전환 기믹 패턴(TransitionGimmick) 하나가 실행하는 흐름:
 *  1) SpawnGimmickTower : 4개 지정 위치 중 (지형 미파괴 + 타워 없는) 곳 랜덤 선정
 *     -> 이번 라운드의 '파괴 대상 슬라이스' 확정 + 타워 스폰
 *  2) BeginStaggerPhase : 무력화 게이지 리필 + State.Boss.StaggerPhase 복제 태그 (UI 게이지 표시)
 *     -> 게이지가 0이 되면 즉시 그로기 GE 적용 (Groggy 태그 -> 패턴 Branch 가 루프를 즉시 끊음)
 *  3) EndStaggerPhase   : 저스트가드 준비 구간 진입 시 게이지 UI 내림
 *  4) RequestDestroyGimmickSlice : 저스트가드 성공(그로기)/실패(넉다운) 뒤 지연 후 지형 파괴
 *
 * 흐름 제어(몽타주/분기)는 전부 패턴 데이터가 하고, 이 컴포넌트는 상태(어느 슬라이스인가,
 * 무력화 중인가)와 부수효과(타워 스폰, 게이지, 파괴 호출)만 담당한다.
 * 각 단계는 대응하는 애님 노티파이(AnimNotify_BossGimmick*)가 호출한다.
 */
UCLASS(ClassGroup = (Boss), meta = (BlueprintSpawnableComponent))
class LOSTARK_API UBossTerrainGimmickComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossTerrainGimmickComponent();

	/**
	 * 라운드 시작: 스폰 위치 선정 + 타워 스폰 (서버).
	 * 후보 = TowerSpawnPoints 중 [슬라이스 미파괴 && 살아있는 타워 없음].
	 * 후보가 없으면 타워는 안 뽑고 '파괴 대상 슬라이스'만 미파괴 위치 중에서 선정.
	 * @return 스폰된 타워 (스폰 못 하면 nullptr — 파괴 대상 선정은 그래도 수행됨)
	 */
	UFUNCTION(BlueprintCallable, Category = "Boss|Gimmick")
	ABossGimmickTower* SpawnGimmickTower();

	/** 무력화 페이즈 시작: 게이지 리필 + UI 태그 부여. 게이지 0 -> 즉시 그로기 (서버) */
	UFUNCTION(BlueprintCallable, Category = "Boss|Gimmick")
	void BeginStaggerPhase();

	/** 무력화 페이즈 종료: UI 태그 회수 (중복 호출 안전) */
	UFUNCTION(BlueprintCallable, Category = "Boss|Gimmick")
	void EndStaggerPhase();

	/** 그로기 GE 적용 (SetByCaller Data.Duration). 무력화 성공 시 내부 호출되지만 외부(노티파이)도 사용 가능 */
	UFUNCTION(BlueprintCallable, Category = "Boss|Gimmick")
	void ApplyGroggy(float Duration);

	/** Delay(초) 후 이번 라운드의 대상 슬라이스를 파괴 (GameMode::DestroySlice). 라운드 종료 */
	UFUNCTION(BlueprintCallable, Category = "Boss|Gimmick")
	void RequestDestroyGimmickSlice(float Delay);

	/**
	 * 이번(또는 직전) 라운드의 기믹 위치. 보스가 '파괴할 지형 바라보기' 회전 기준.
	 * 파괴 요청 후에도 위치는 유지된다 (파괴 모션 동안 계속 바라봐야 하므로).
	 * @return 유효한 기믹 위치가 있으면 true
	 */
	UFUNCTION(BlueprintPure, Category = "Boss|Gimmick")
	bool GetGimmickSliceLocation(FVector& OutLocation) const;

	/** 타워 스폰 후보 4개 위치 (월드 좌표. 보스 BP에서 레벨에 맞게 세팅) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Gimmick")
	TArray<FVector> TowerSpawnPoints;

	/** 스폰할 타워 클래스 (BP_BossGimmickTower) */
	UPROPERTY(EditAnywhere, Category = "Boss|Gimmick")
	TSubclassOf<ABossGimmickTower> TowerClass;

	/** 무력화 성공(게이지 0) 시 적용할 그로기 GE. 기본 UBossGroggyEffect */
	UPROPERTY(EditAnywhere, Category = "Boss|Gimmick|Stagger")
	TSubclassOf<UGameplayEffect> GroggyEffectClass;

	/** 무력화 성공 그로기 지속시간(초) */
	UPROPERTY(EditAnywhere, Category = "Boss|Gimmick|Stagger", meta = (ClampMin = "0.1"))
	float StaggerGroggyDuration = 6.f;

protected:
	virtual void BeginPlay() override;

private:
	UAbilitySystemComponent* GetASC() const;

	/** 무력화 게이지 변화 감시: 페이즈 중 0 도달 -> 즉시 그로기 */
	void OnStaggerGaugeChanged(const FOnAttributeChangeData& Data);

	/** 예약된 슬라이스 파괴 실행 */
	void ExecuteDestroySlice();

	/** 소멸된 타워 정리 (약참조 무효 항목 제거) */
	void CleanupDeadTowers();

	/** 스폰 위치 인덱스 -> 살아있는 타워 (점유 위치 재스폰 방지) */
	TMap<int32, TWeakObjectPtr<ABossGimmickTower>> LiveTowers;

	/** 이번 라운드의 파괴 대상 (파괴 요청 시 INDEX_NONE 으로 리셋 -> 이중 파괴 방지) */
	int32 CurrentSliceIndex = INDEX_NONE;

	/** 이번(직전) 라운드의 기믹 위치. 다음 라운드 시작까지 유지 (바라보기 회전용) */
	FVector CurrentGimmickLocation = FVector::ZeroVector;
	bool bHasGimmickLocation = false;

	/** 파괴 예약된 슬라이스 (타이머 콜백용) */
	int32 PendingDestroySliceIndex = INDEX_NONE;

	bool bStaggerPhaseActive = false;

	FTimerHandle DestroySliceTimer;
};
