// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossTerrainGimmickComponent.generated.h"

class ABossGimmickTower;
class ABossRaidGameState;
class UAbilitySystemComponent;
class UGameplayEffect;
struct FOnAttributeChangeData;
struct FGameplayEventData;

/** 무력화 게이지가 0이 됐을 때의 처리 방식 (BeginStaggerPhase 에서 지정) */
UENUM(BlueprintType)
enum class EStaggerResolve : uint8
{
	Groggy		UMETA(DisplayName = "그로기 (기믹)"),		// 보스 그로기 GE
	GrabRelease	UMETA(DisplayName = "잡기 해제 (구출)")		// 이 보스가 잡은 대상 전원 해제
};

/** 무력화 게이지 변경 방송 (Current, Max). 서버+클라 모두 발동 -> WBP_StaggerGauge 바인딩 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaggerGaugeChanged, float, Gauge, float, MaxGauge);

/**
 * 지형파괴 기믹 오케스트레이터 (서버 전용 로직).
 *
 * 기믹 한 라운드 = 페이즈 전환 기믹 패턴(TransitionGimmick) 하나가 실행하는 흐름:
 *  1) SpawnGimmickTower(TargetSlice) : 몽타주가 지정한 파괴 대상 슬라이스 확정 + 타워 스폰
 *     (타워는 미파괴 && 파괴대상 아닌 슬라이스에, 마지막 라운드엔 파괴대상 위에 폴백)
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
	 * 라운드 시작: 이번 라운드 파괴 대상 슬라이스 확정(TargetSlice) + 타워 스폰 (서버).
	 * TargetSlice 는 몽타주(AN_SpawnTower)가 명시 지정 — 바라보기(FaceSlice)/파괴(DestroySlice)가 모두 이 값을 따른다.
	 * 지정된 슬라이스가 이미 파괴됐으면 남은 슬라이스 중 랜덤으로 대체한다(라운드가 헛돌지 않게).
	 *
	 * 타워 스폰 후보 = [미파괴 && 이번 파괴대상(TargetSlice) 아님 && 그 슬라이스에 살아있는 타워 없음].
	 * 후보 중 랜덤 1곳. 설 곳이 없으면(마지막 라운드 등) 타워 없이 라운드를 진행한다 —
	 * 이미 파괴된 지형이나 곧 파괴될 지형 위에는 절대 스폰하지 않는다.
	 *
	 * 스폰 지점이 '어느 슬라이스 위인가'는 배열 인덱스가 아니라 그 지점의 월드 위치로 판정한다
	 * (GetSliceIndexAt — 바닥 조각을 무너뜨리는 판정과 동일 함수).
	 * @return 스폰된 타워 (스폰 못 하면 nullptr — 파괴 대상 확정은 그래도 수행됨)
	 */
	UFUNCTION(BlueprintCallable, Category = "Boss|Gimmick")
	ABossGimmickTower* SpawnGimmickTower(int32 TargetSlice);

	/**
	 * 무력화 페이즈 시작 (서버): 게이지를 RequiredAmount 로 세팅(Max 도 동일) + UI 태그 부여.
	 * 게이지 0 도달 시 Resolve 에 따라 그로기(기믹) 또는 잡기 해제(구출)를 실행한다.
	 *
	 * GrabRelease 모드에서 HoldSection/ReleaseSection 을 주면, 무력화 성공(게이지 0)이나
	 * 시간초과(WindowDuration) 시 현재 재생 중인 보스 몽타주의 HoldSection next 를
	 * ReleaseSection 으로 바꿔(=SetNextSectionName) 붙잡기 루프를 자연스럽게 빠져나가 던지게 한다.
	 * (실제 던짐/데미지는 ReleaseSection 의 'Boss Grab Release' 노티파이가 실행)
	 *
	 * @param RequiredAmount 이 무력화의 요구량(=최대치). 패턴/노티파이가 지정, 디폴트 100
	 * @param Resolve        0 도달 시 처리 방식 (그로기 / 잡기 해제)
	 * @param WindowDuration 구출 윈도우(초, GrabRelease 전용). >0 이면 이 시간 후 시간초과로 풀림. 0=타이머 없음
	 * @param HoldSection    붙잡고 버티는 루프 섹션 이름 (비우면 폴백: 즉시 직접 해제)
	 * @param ReleaseSection 풀리는 모션 + GrabRelease 노티파이가 있는 섹션 이름
	 */
	UFUNCTION(BlueprintCallable, Category = "Boss|Gimmick")
	void BeginStaggerPhase(float RequiredAmount = 100.f, EStaggerResolve Resolve = EStaggerResolve::Groggy,
		float WindowDuration = 0.f, FName HoldSection = NAME_None, FName ReleaseSection = NAME_None);

	/** 무력화 페이즈 종료: UI 태그 회수 (중복 호출 안전) */
	UFUNCTION(BlueprintCallable, Category = "Boss|Gimmick")
	void EndStaggerPhase();

	/**
	 * 무력화 게이지 감소 (서버). 페이즈 중일 때만 유효. 0 도달 시 BeginStaggerPhase 의 Resolve 처리.
	 * 디버그 E / 스킬(Event.Boss.StaggerHit)이 호출. 잡힌 플레이어는 어차피 입력 불가라 팀원만 깎는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Boss|Gimmick")
	void ApplyStaggerHit(float Amount);

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

	/**
	 * 타워 스폰 후보 위치들 (월드 좌표). 슬라이스당 하나씩, SliceCount 개를 채운다.
	 * (각 슬라이스 = 바닥 조각 2개이므로, 그 슬라이스를 대표하는 한 지점을 넣으면 된다)
	 *
	 * 배열 순서는 참고용이다 — 어느 슬라이스 위인지는 좌표로 판정하므로 순서가 어긋나도
	 * 파괴된 지형에 스폰되지 않는다. 다만 어긋나면 로그 경고가 뜨니 맞춰두는 게 좋다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Gimmick")
	TArray<FVector> TowerSpawnPoints;

	/** 보스가 파괴 슬라이스를 바라볼 때의 기준 거리(cm). 아레나 중심에서 슬라이스 방향으로 이만큼 앞 */
	UPROPERTY(EditAnywhere, Category = "Boss|Gimmick", meta = (ClampMin = "1.0"))
	float GimmickLookRadius = 500.f;

	/** 스폰할 타워 클래스 (BP_BossGimmickTower) */
	UPROPERTY(EditAnywhere, Category = "Boss|Gimmick")
	TSubclassOf<ABossGimmickTower> TowerClass;

	/** 무력화 성공(게이지 0) 시 적용할 그로기 GE. 기본 UBossGroggyEffect */
	UPROPERTY(EditAnywhere, Category = "Boss|Gimmick|Stagger")
	TSubclassOf<UGameplayEffect> GroggyEffectClass;

	/** 무력화 성공 그로기 지속시간(초) */
	UPROPERTY(EditAnywhere, Category = "Boss|Gimmick|Stagger", meta = (ClampMin = "0.1"))
	float StaggerGroggyDuration = 6.f;

	/** 무력화 게이지 변경 방송 (Current, Max). WBP_StaggerGauge 가 여기 바인딩 (잡기/기믹 공용) */
	UPROPERTY(BlueprintAssignable, Category = "Boss|Gimmick|Stagger")
	FOnStaggerGaugeChanged OnStaggerGaugeChanged;

protected:
	virtual void BeginPlay() override;

private:
	UAbilitySystemComponent* GetASC() const;

	/** 무력화 게이지 변화 감시(서버+클라): UI 방송 + 서버 0 도달 시 Resolve 처리 */
	void HandleStaggerGaugeChanged(const FOnAttributeChangeData& Data);

	/** Event.Boss.StaggerHit 수신 (서버) -> ApplyStaggerHit(EventMagnitude) */
	void HandleStaggerHitEvent(const FGameplayEventData* Payload);

	/** 이 보스가 잡고 있는 모든 잡기 장판 즉시 해제(던짐). 섹션 폴백/구버전 경로에서만 사용 */
	void ReleaseBossGrabs();

	/**
	 * 붙잡기 루프를 풀림 섹션으로 빠져나가게 한다 (무력화 성공/시간초과 공용).
	 * 현재 재생 중인 보스 몽타주의 HoldSection next 를 ReleaseSection 으로 교체(SetNextSectionName).
	 * 섹션 미지정이거나 몽타주가 GAS 재생이 아니면 폴백으로 ReleaseBossGrabs() 직접 해제.
	 */
	void BreakGrabHoldToRelease();

	/** 구출 윈도우 만료(시간초과): 게이지 내리고 풀림 섹션으로 (던짐은 Release 노티파이) */
	void OnStaggerWindowTimeout();

	/** 그로기 종료: State.Boss.Groggy 복제 루스 태그 회수 (GroggyTimer 콜백) */
	void EndGroggy();

	/** 예약된 슬라이스 파괴 실행 */
	void ExecuteDestroySlice();

	/** 소멸된 타워 정리 (약참조 무효 항목 제거) */
	void CleanupDeadTowers();

	/** 슬라이스 인덱스 -> 살아있는 타워 (같은 슬라이스 중복 스폰 방지) */
	TMap<int32, TWeakObjectPtr<ABossGimmickTower>> LiveTowers;

	/** 이번 라운드의 파괴 대상 (파괴 요청 시 INDEX_NONE 으로 리셋 -> 이중 파괴 방지) */
	int32 CurrentSliceIndex = INDEX_NONE;

	/** 이번(직전) 라운드의 기믹 위치. 다음 라운드 시작까지 유지 (바라보기 회전용) */
	FVector CurrentGimmickLocation = FVector::ZeroVector;
	bool bHasGimmickLocation = false;

	/** 파괴 예약된 슬라이스 (타이머 콜백용) */
	int32 PendingDestroySliceIndex = INDEX_NONE;

	bool bStaggerPhaseActive = false;

	/** 이번 무력화 페이즈가 0 도달 시 무엇을 할지 (BeginStaggerPhase 에서 설정) */
	EStaggerResolve CurrentStaggerResolve = EStaggerResolve::Groggy;

	/** GrabRelease 모드: 붙잡기 루프 섹션 / 풀림 섹션 이름 (BeginStaggerPhase 에서 설정) */
	FName GrabHoldSection = NAME_None;
	FName GrabReleaseSection = NAME_None;

	/** 구출 윈도우 타이머 (만료 시 시간초과 -> 풀림 섹션) */
	FTimerHandle StaggerWindowTimer;

	FTimerHandle DestroySliceTimer;

	/** 그로기 지속시간 타이머 (만료 시 EndGroggy 로 태그 회수) */
	FTimerHandle GroggyTimer;
};
