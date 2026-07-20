// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Gimmick/BossTerrainGimmickComponent.h"
#include "Boss/Gimmick/BossGimmickTower.h"
#include "Boss/BossAttributeSet.h"
#include "Boss/BossGameplayTags.h"
#include "Boss/Combat/BossCombatStatics.h"
#include "Boss/Combat/BossGroggyEffect.h"
#include "Boss/Damage/BossPatternActorBase.h"
#include "Boss/Damage/BossAoeGrabEffect.h"
#include "Boss/Raid/BossRaidGameMode.h"
#include "Boss/Raid/BossRaidGameState.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

UBossTerrainGimmickComponent::UBossTerrainGimmickComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	GroggyEffectClass = UBossGroggyEffect::StaticClass();
}

void UBossTerrainGimmickComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 무력화 게이지 감시. 어트리뷰트는 복제되므로 서버(0 도달 판정)와 클라(UI 방송) 모두 구독한다.
	if (UAbilitySystemComponent* ASC = GetASC())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UBossAttributeSet::GetStaggerGaugeAttribute())
			.AddUObject(this, &UBossTerrainGimmickComponent::HandleStaggerGaugeChanged);

		// 무력화 감소 이벤트(스킬/디버그)는 서버에서만 처리
		if (Owner->HasAuthority())
		{
			ASC->GenericGameplayEventCallbacks.FindOrAdd(LostArkTags::Event_Boss_StaggerHit.GetTag())
				.AddUObject(this, &UBossTerrainGimmickComponent::HandleStaggerHitEvent);
		}
	}
}

UAbilitySystemComponent* UBossTerrainGimmickComponent::GetASC() const
{
	return GetOwner() ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()) : nullptr;
}

void UBossTerrainGimmickComponent::CleanupDeadTowers()
{
	for (auto It = LiveTowers.CreateIterator(); It; ++It)
	{
		ABossGimmickTower* Tower = It->Value.Get();
		if (!Tower || Tower->IsDying())
		{
			It.RemoveCurrent();
		}
	}
}

ABossGimmickTower* UBossTerrainGimmickComponent::SpawnGimmickTower()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !Owner->HasAuthority() || !World)
	{
		return nullptr;
	}

	ABossRaidGameState* GS = World->GetGameState<ABossRaidGameState>();
	if (!GS || TowerSpawnPoints.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TerrainGimmick] GameState 없음 또는 TowerSpawnPoints 미설정 — 라운드 시작 불가"));
		return nullptr;
	}

	CleanupDeadTowers();

	// 후보 분류: 파괴 안 된 위치 전체(파괴 대상 폴백용) / 그중 타워도 없는 위치(스폰 후보)
	TArray<int32> NotDestroyed;
	TArray<int32> Spawnable;
	for (int32 i = 0; i < TowerSpawnPoints.Num(); ++i)
	{
		const int32 Slice = GS->GetSliceIndexAt(TowerSpawnPoints[i]);
		if (GS->IsSliceDestroyed(Slice))
		{
			continue;	// 이미 파괴된 지형 위에는 스폰하지 않는다
		}
		NotDestroyed.Add(i);
		if (!LiveTowers.Contains(i))
		{
			Spawnable.Add(i);
		}
	}

	if (NotDestroyed.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TerrainGimmick] 4개 위치가 전부 파괴됨 — 기믹 라운드 스킵"));
		return nullptr;
	}

	// 이번 라운드 위치 확정: 스폰 가능 위치 우선, 전부 점유면 미파괴 위치 중에서(타워 없이 파괴만 진행)
	const TArray<int32>& Pool = (Spawnable.Num() > 0) ? Spawnable : NotDestroyed;
	const int32 PointIndex = Pool[FMath::RandRange(0, Pool.Num() - 1)];

	CurrentGimmickLocation = TowerSpawnPoints[PointIndex];
	bHasGimmickLocation = true;
	CurrentSliceIndex = GS->GetSliceIndexAt(CurrentGimmickLocation);

	if (Spawnable.Num() == 0 || !TowerClass)
	{
		return nullptr;	// 파괴 대상만 확정, 타워는 없음 (점유/클래스 미설정)
	}

	const FTransform SpawnTM(FRotator::ZeroRotator, CurrentGimmickLocation);
	ABossGimmickTower* Tower = World->SpawnActorDeferred<ABossGimmickTower>(
		TowerClass, SpawnTM, Owner, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Tower)
	{
		Tower->InitTower(Owner, CurrentSliceIndex);
		Tower->FinishSpawning(SpawnTM);
		LiveTowers.Add(PointIndex, Tower);
	}
	return Tower;
}

void UBossTerrainGimmickComponent::BeginStaggerPhase(float RequiredAmount, EStaggerResolve Resolve)
{
	AActor* Owner = GetOwner();
	UAbilitySystemComponent* ASC = GetASC();
	if (!Owner || !Owner->HasAuthority() || !ASC || bStaggerPhaseActive)
	{
		return;
	}
	bStaggerPhaseActive = true;
	CurrentStaggerResolve = Resolve;

	// 요구량 = 최대치. Max/Current 를 함께 세팅 -> 위젯이 Current/Max 로 항상 100%부터 시작.
	// (패턴마다 요구량이 다르므로 노티파이/호출부가 지정, 디폴트 100)
	const float Required = FMath::Max(1.f, RequiredAmount);
	ASC->SetNumericAttributeBase(UBossAttributeSet::GetMaxStaggerGaugeAttribute(), Required);
	ASC->SetNumericAttributeBase(UBossAttributeSet::GetStaggerGaugeAttribute(), Required);

	// 클라 무력화 게이지 UI 표시 (게이지 값 자체는 어트리뷰트 복제로 이미 전파됨)
	UBossCombatStatics::AddReplicatedLooseTag(ASC, LostArkTags::State_Boss_StaggerPhase.GetTag());
}

void UBossTerrainGimmickComponent::ApplyStaggerHit(float Amount)
{
	AActor* Owner = GetOwner();
	UAbilitySystemComponent* ASC = GetASC();
	if (!Owner || !Owner->HasAuthority() || !ASC || !bStaggerPhaseActive || Amount <= 0.f)
	{
		return;
	}

	// 감소만 (PreAttributeChange 가 [0, Max] 로 클램프). 0 도달 처리는 HandleStaggerGaugeChanged 에서.
	const float Current = ASC->GetNumericAttribute(UBossAttributeSet::GetStaggerGaugeAttribute());
	ASC->SetNumericAttributeBase(UBossAttributeSet::GetStaggerGaugeAttribute(), Current - Amount);
}

void UBossTerrainGimmickComponent::HandleStaggerHitEvent(const FGameplayEventData* Payload)
{
	// 스킬이 EventMagnitude 에 자기 무력화 수치를 실어 보낸다. 미지정(0)이면 디버그 기본 10.
	const float Amount = (Payload && Payload->EventMagnitude > 0.f) ? Payload->EventMagnitude : 10.f;
	ApplyStaggerHit(Amount);
}

void UBossTerrainGimmickComponent::ReleaseBossGrabs()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 이 보스가 스폰한 잡기 장판 중 대상을 잡고 있는 것 전부 해제 (AnimNotify_BossGrabRelease 와 동일 경로)
	for (TActorIterator<ABossPatternActorBase> It(Owner->GetWorld()); It; ++It)
	{
		if (It->GetOwner() != Owner)
		{
			continue;
		}
		if (UBossAoeGrabEffect* Grab = Cast<UBossAoeGrabEffect>(It->GetOnHitEffect()))
		{
			if (Grab->HasGrabbedTargets())
			{
				Grab->ReleaseAll();
			}
		}
	}
}

void UBossTerrainGimmickComponent::EndStaggerPhase()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !bStaggerPhaseActive)
	{
		return;
	}
	bStaggerPhaseActive = false;

	if (UAbilitySystemComponent* ASC = GetASC())
	{
		UBossCombatStatics::RemoveReplicatedLooseTag(ASC, LostArkTags::State_Boss_StaggerPhase.GetTag());
	}
}

void UBossTerrainGimmickComponent::HandleStaggerGaugeChanged(const FOnAttributeChangeData& Data)
{
	// UI 방송 (서버+클라 공통). 위젯은 Current/Max 로 % 표시. 잡기/기믹 같은 게이지라 공용 델리게이트.
	float Max = Data.NewValue;
	if (UAbilitySystemComponent* ASC = GetASC())
	{
		Max = ASC->GetNumericAttribute(UBossAttributeSet::GetMaxStaggerGaugeAttribute());
	}
	OnStaggerGaugeChanged.Broadcast(Data.NewValue, Max);

	// 0 도달 처리는 서버 권위에서만
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !bStaggerPhaseActive || Data.NewValue > 0.f)
	{
		return;
	}

	// 게이지 소진. Groggy 태그가 서는 순간 패턴 스텝 Branch('State.Boss.Groggy 보유')가 재생 중
	// 몽타주를 즉시 끊고 다음 단계로 넘어간다 (레이저 루프 즉시 종료 요구사항).
	EndStaggerPhase();
	if (CurrentStaggerResolve == EStaggerResolve::GrabRelease)
	{
		ReleaseBossGrabs();	// 구출: 잡힌 팀원 해제
	}
	else
	{
		ApplyGroggy(StaggerGroggyDuration);	// 기믹: 보스 그로기
	}
}

void UBossTerrainGimmickComponent::ApplyGroggy(float Duration)
{
	UBossCombatStatics::ApplyEffectToSelf(GetASC(), GroggyEffectClass, this,
		LostArkTags::Data_Duration.GetTag(), FMath::Max(0.1f, Duration));
}

void UBossTerrainGimmickComponent::RequestDestroyGimmickSlice(float Delay)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}
	if (CurrentSliceIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TerrainGimmick] 파괴 대상 슬라이스 미확정 (SpawnGimmickTower 를 먼저 호출했는지 확인)"));
		return;
	}

	// 라운드의 파괴 대상 소모 (이중 파괴 방지). 위치는 파괴 모션 바라보기용으로 유지
	PendingDestroySliceIndex = CurrentSliceIndex;
	CurrentSliceIndex = INDEX_NONE;

	if (Delay <= KINDA_SMALL_NUMBER)
	{
		ExecuteDestroySlice();
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(
		DestroySliceTimer, this, &UBossTerrainGimmickComponent::ExecuteDestroySlice, Delay, false);
}

void UBossTerrainGimmickComponent::ExecuteDestroySlice()
{
	if (PendingDestroySliceIndex == INDEX_NONE)
	{
		return;
	}

	if (ABossRaidGameMode* GM = GetWorld()->GetAuthGameMode<ABossRaidGameMode>())
	{
		// 슬라이스 파괴 (레벨팀 ID 시스템 호출). 타워가 그 위에 살아있으면 방송을 받고 함께 소멸
		GM->DestroySlice(PendingDestroySliceIndex);
	}
	PendingDestroySliceIndex = INDEX_NONE;
}

bool UBossTerrainGimmickComponent::GetGimmickSliceLocation(FVector& OutLocation) const
{
	OutLocation = CurrentGimmickLocation;
	return bHasGimmickLocation;
}
