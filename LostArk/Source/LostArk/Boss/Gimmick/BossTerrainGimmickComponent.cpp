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

	// 1) 이번 라운드 파괴 대상 = '고정 순서(DestructionOrder)'의 다음 미파괴 슬라이스.
	//    타워 위치와 완전히 독립 — 정해진 순서대로 번호를 부른다.
	CurrentSliceIndex = NextDestructionSlice(GS);
	if (CurrentSliceIndex != INDEX_NONE)
	{
		// 보스가 바라볼 위치 = 그 슬라이스 방향 (아레나 중심 기준)
		CurrentGimmickLocation = GS->GetSliceCenterLocation(CurrentSliceIndex, GimmickLookRadius);
		bHasGimmickLocation = true;
	}

	// 2) 타워 스폰: 지정 위치 중 [지형 미파괴 && 타워 없음] 곳에 랜덤. (파괴 대상 슬라이스와 무관)
	TArray<int32> Spawnable;
	for (int32 i = 0; i < TowerSpawnPoints.Num(); ++i)
	{
		const int32 Slice = GS->GetSliceIndexAt(TowerSpawnPoints[i]);
		if (!GS->IsSliceDestroyed(Slice) && !LiveTowers.Contains(i))
		{
			Spawnable.Add(i);
		}
	}

	if (Spawnable.Num() == 0 || !TowerClass)
	{
		return nullptr;	// 파괴 대상만 확정, 타워는 생략 (스폰할 곳/클래스 없음)
	}

	const int32 PointIndex = Spawnable[FMath::RandRange(0, Spawnable.Num() - 1)];
	const FVector TowerLoc = TowerSpawnPoints[PointIndex];

	const FTransform SpawnTM(FRotator::ZeroRotator, TowerLoc);
	ABossGimmickTower* Tower = World->SpawnActorDeferred<ABossGimmickTower>(
		TowerClass, SpawnTM, Owner, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Tower)
	{
		// 타워의 SliceIndex = '딛고 선' 지형 (그 지형이 나중에 파괴되면 함께 소멸용). 파괴 대상과는 별개.
		Tower->InitTower(Owner, GS->GetSliceIndexAt(TowerLoc));
		Tower->FinishSpawning(SpawnTM);
		LiveTowers.Add(PointIndex, Tower);
	}
	return Tower;
}

int32 UBossTerrainGimmickComponent::NextDestructionSlice(ABossRaidGameState* GS)
{
	if (!GS)
	{
		return INDEX_NONE;
	}

	if (DestructionOrder.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[TerrainGimmick] DestructionOrder 미설정 — 보스 기믹 컴포넌트에 파괴 순서를 지정할 것 (파괴 스킵)"));
		return INDEX_NONE;
	}

	// 정해진 순서를 따라가며 이미 파괴된 번호는 건너뛴다 (한 바퀴 다 돌아 전부 파괴면 종료)
	for (int32 n = 0; n < DestructionOrder.Num(); ++n)
	{
		const int32 Idx = DestructionOrderIndex % DestructionOrder.Num();
		++DestructionOrderIndex;
		const int32 Candidate = DestructionOrder[Idx];
		if (!GS->IsSliceDestroyed(Candidate))
		{
			return Candidate;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[TerrainGimmick] DestructionOrder 슬라이스가 전부 파괴됨 — 더 부술 지형 없음"));
	return INDEX_NONE;
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

	// 게이지 소진. EndStaggerPhase 는 즉시 (재진입 방지: bStaggerPhaseActive=false + UI 태그 회수).
	EndStaggerPhase();

	// 그로기 GE 적용/잡기 해제는 '다음 틱'으로 미룬다.
	// 여기(HandleStaggerGaugeChanged)는 무력화 게이지 어트리뷰트 변경 콜백 안이라,
	// 이 스택에서 바로 ApplyGroggy(=GE 적용)를 하면 GAS 가 적용을 무시/지연해서
	// State.Boss.Groggy 태그가 안 붙는다(→ 패턴 Branch 가 그로기를 못 잡음).
	// 콜백 스택을 빠져나온 다음 틱에 적용하면 태그가 정상적으로 서고 Branch 가 인터럽트한다.
	const EStaggerResolve Resolve = CurrentStaggerResolve;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
			[this, Resolve]()
			{
				if (Resolve == EStaggerResolve::GrabRelease)
				{
					ReleaseBossGrabs();	// 구출: 잡힌 팀원 해제
				}
				else
				{
					ApplyGroggy(StaggerGroggyDuration);	// 기믹: 보스 그로기
				}
			}));
	}
}

void UBossTerrainGimmickComponent::ApplyGroggy(float Duration)
{
	AActor* Owner = GetOwner();
	UAbilitySystemComponent* ASC = GetASC();
	if (!Owner || !Owner->HasAuthority() || !ASC)
	{
		return;
	}

	const float Dur = FMath::Max(0.1f, Duration);

	// 그로기 = State.Boss.Groggy 를 Dur 초 동안 부여.
	// (UBossGroggyEffect 의 TargetTags 컴포넌트가 이 프로젝트에서 태그를 안 붙이는 문제가 있어,
	//  무력화 페이즈 태그와 동일하게 '복제 루스 태그 + 타이머'로 직접 부여 — 서버 분기 + 클라 연출 모두 전파)
	if (!ASC->HasMatchingGameplayTag(LostArkTags::State_Boss_Groggy.GetTag()))
	{
		UBossCombatStatics::AddReplicatedLooseTag(ASC, LostArkTags::State_Boss_Groggy.GetTag());
	}

	// 지속시간 타이머 (중첩 시 갱신). 만료되면 EndGroggy 가 태그를 회수 ->
	// 그로기 루프 스텝의 'NOT State.Boss.Groggy' 분기가 종료 몽타주로 넘어간다.
	Owner->GetWorldTimerManager().SetTimer(GroggyTimer, this,
		&UBossTerrainGimmickComponent::EndGroggy, Dur, false);
}

void UBossTerrainGimmickComponent::EndGroggy()
{
	if (UAbilitySystemComponent* ASC = GetASC())
	{
		UBossCombatStatics::RemoveReplicatedLooseTag(ASC, LostArkTags::State_Boss_Groggy.GetTag());
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
