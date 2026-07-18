// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Gimmick/BossTerrainGimmickComponent.h"
#include "Boss/Gimmick/BossGimmickTower.h"
#include "Boss/BossAttributeSet.h"
#include "Boss/BossGameplayTags.h"
#include "Boss/Combat/BossCombatStatics.h"
#include "Boss/Combat/BossGroggyEffect.h"
#include "Boss/Raid/BossRaidGameMode.h"
#include "Boss/Raid/BossRaidGameState.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
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
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	// 무력화 게이지 감시 (페이즈 중 0 도달 -> 즉시 그로기)
	if (UAbilitySystemComponent* ASC = GetASC())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UBossAttributeSet::GetStaggerGaugeAttribute())
			.AddUObject(this, &UBossTerrainGimmickComponent::OnStaggerGaugeChanged);
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

void UBossTerrainGimmickComponent::BeginStaggerPhase()
{
	AActor* Owner = GetOwner();
	UAbilitySystemComponent* ASC = GetASC();
	if (!Owner || !Owner->HasAuthority() || !ASC || bStaggerPhaseActive)
	{
		return;
	}
	bStaggerPhaseActive = true;

	// 게이지 리필 (매 기믹 라운드 새로 깎는다)
	const float Max = ASC->GetNumericAttribute(UBossAttributeSet::GetMaxStaggerGaugeAttribute());
	ASC->SetNumericAttributeBase(UBossAttributeSet::GetStaggerGaugeAttribute(), Max);

	// 클라 무력화 게이지 UI 표시 (게이지 값 자체는 어트리뷰트 복제로 이미 전파됨)
	UBossCombatStatics::AddReplicatedLooseTag(ASC, LostArkTags::State_Boss_StaggerPhase.GetTag());
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

void UBossTerrainGimmickComponent::OnStaggerGaugeChanged(const FOnAttributeChangeData& Data)
{
	// 무력화 페이즈 중 게이지 0 -> 즉시 그로기.
	// Groggy 태그가 서는 순간 패턴 스텝 Branch('State.Boss.Groggy 보유')가 재생 중 몽타주를
	// 즉시 끊고 다음 단계로 넘어간다 (레이저 루프 즉시 종료 요구사항).
	if (!bStaggerPhaseActive || Data.NewValue > 0.f)
	{
		return;
	}

	EndStaggerPhase();
	ApplyGroggy(StaggerGroggyDuration);
}

void UBossTerrainGimmickComponent::ApplyGroggy(float Duration)
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC || !GroggyEffectClass)
	{
		return;
	}

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GroggyEffectClass, 1.f, ASC->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(LostArkTags::Data_Duration.GetTag(), FMath::Max(0.1f, Duration));
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	}
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
