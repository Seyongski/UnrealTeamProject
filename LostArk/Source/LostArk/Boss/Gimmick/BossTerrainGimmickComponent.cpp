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

ABossGimmickTower* UBossTerrainGimmickComponent::SpawnGimmickTower(int32 TargetSlice)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !Owner->HasAuthority() || !World)
	{
		return nullptr;
	}

	ABossRaidGameState* GS = World->GetGameState<ABossRaidGameState>();
	if (!GS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TerrainGimmick] GameState 없음 — 라운드 시작 불가"));
		return nullptr;
	}
	if (TargetSlice < 0 || TargetSlice >= GS->SliceCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TerrainGimmick] TargetSlice(%d) 범위 밖 (SliceCount=%d) — 파괴 대상 미확정"),
			TargetSlice, GS->SliceCount);
		return nullptr;
	}

	CleanupDeadTowers();

	// 1) 이번 라운드 파괴 대상 = 몽타주(AN_SpawnTower)가 지정한 슬라이스.
	//    바라보기(FaceSlice)/파괴(DestroySlice)가 모두 이 값을 따른다.
	CurrentSliceIndex = TargetSlice;
	CurrentGimmickLocation = GS->GetSliceCenterLocation(TargetSlice, GimmickLookRadius);
	bHasGimmickLocation = true;

	// 2) 타워 스폰 후보 = [미파괴 && 이번 파괴대상 아님 && 타워 없음 && 스폰 위치 있음].
	//    타워가 곧 무너질 지형과 함께 사라지는 것을 방지 (이번에 부술 슬라이스를 제외).
	TArray<int32> Candidates;
	for (int32 Slice = 0; Slice < GS->SliceCount; ++Slice)
	{
		if (GS->IsSliceDestroyed(Slice) || Slice == TargetSlice ||
			LiveTowers.Contains(Slice) || !TowerSpawnPoints.IsValidIndex(Slice))
		{
			continue;
		}
		Candidates.Add(Slice);
	}

	// 마지막 라운드 폴백: 남은 미파괴 지형이 파괴대상 하나뿐이라 제외 후 후보가 없으면
	// 파괴대상 슬라이스 위에 스폰한다 (그곳 말고는 설 지형이 없으므로).
	if (Candidates.Num() == 0 &&
		!LiveTowers.Contains(TargetSlice) && TowerSpawnPoints.IsValidIndex(TargetSlice))
	{
		Candidates.Add(TargetSlice);
	}

	if (Candidates.Num() == 0 || !TowerClass)
	{
		return nullptr;	// 파괴 대상만 확정, 타워는 생략 (스폰할 곳/클래스 없음)
	}

	const int32 SpawnSlice = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	const FVector TowerLoc = TowerSpawnPoints[SpawnSlice];

	const FTransform SpawnTM(FRotator::ZeroRotator, TowerLoc);
	ABossGimmickTower* Tower = World->SpawnActorDeferred<ABossGimmickTower>(
		TowerClass, SpawnTM, Owner, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Tower)
	{
		// 타워의 SliceIndex = 딛고 선 슬라이스. 그 슬라이스가 파괴되면 타워도 함께 소멸.
		Tower->InitTower(Owner, SpawnSlice);
		Tower->FinishSpawning(SpawnTM);
		LiveTowers.Add(SpawnSlice, Tower);
	}
	return Tower;
}

void UBossTerrainGimmickComponent::BeginStaggerPhase(float RequiredAmount, EStaggerResolve Resolve,
	float WindowDuration, FName HoldSection, FName ReleaseSection)
{
	AActor* Owner = GetOwner();
	UAbilitySystemComponent* ASC = GetASC();
	if (!Owner || !Owner->HasAuthority() || !ASC || bStaggerPhaseActive)
	{
		return;
	}
	bStaggerPhaseActive = true;
	CurrentStaggerResolve = Resolve;
	GrabHoldSection = HoldSection;
	GrabReleaseSection = ReleaseSection;

	// 요구량 = 최대치. Max/Current 를 함께 세팅 -> 위젯이 Current/Max 로 항상 100%부터 시작.
	// (패턴마다 요구량이 다르므로 노티파이/호출부가 지정, 디폴트 100)
	const float Required = FMath::Max(1.f, RequiredAmount);
	ASC->SetNumericAttributeBase(UBossAttributeSet::GetMaxStaggerGaugeAttribute(), Required);
	ASC->SetNumericAttributeBase(UBossAttributeSet::GetStaggerGaugeAttribute(), Required);

	// 클라 무력화 게이지 UI 표시 (게이지 값 자체는 어트리뷰트 복제로 이미 전파됨)
	UBossCombatStatics::AddReplicatedLooseTag(ASC, LostArkTags::State_Boss_StaggerPhase.GetTag());

	// 구출(GrabRelease) 모드 + 윈도우 지정 시: 시간초과 타이머. 무력화가 먼저면 EndStaggerPhase 가 취소.
	if (Resolve == EStaggerResolve::GrabRelease && WindowDuration > 0.f)
	{
		Owner->GetWorldTimerManager().SetTimer(StaggerWindowTimer, this,
			&UBossTerrainGimmickComponent::OnStaggerWindowTimeout, WindowDuration, false);
	}
}

void UBossTerrainGimmickComponent::ApplyStaggerHit(float Amount)
{
	AActor* Owner = GetOwner();
	UAbilitySystemComponent* ASC = GetASC();
	if (!Owner || !Owner->HasAuthority() || !ASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TerrainGimmick] ApplyStaggerHit 무시 — Owner/Authority/ASC 없음 (서버에서 호출됐는지 확인)"));
		return;
	}
	if (!bStaggerPhaseActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TerrainGimmick] ApplyStaggerHit 무시 — 무력화 페이즈 비활성 (BeginStaggerPhase 가 먼저 실행돼야 함)"));
		return;
	}
	if (Amount <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TerrainGimmick] ApplyStaggerHit 무시 — Amount=%.1f <= 0"), Amount);
		return;
	}

	// 감소만 (PreAttributeChange 가 [0, Max] 로 클램프). 0 도달 처리는 HandleStaggerGaugeChanged 에서.
	const float Current = ASC->GetNumericAttribute(UBossAttributeSet::GetStaggerGaugeAttribute());
	const float Next = Current - Amount;
	ASC->SetNumericAttributeBase(UBossAttributeSet::GetStaggerGaugeAttribute(), Next);
	UE_LOG(LogTemp, Log, TEXT("[TerrainGimmick] 무력화 게이지 감소: %.1f -> %.1f (Amount=%.1f)"), Current, Next, Amount);
}

void UBossTerrainGimmickComponent::HandleStaggerHitEvent(const FGameplayEventData* Payload)
{
	// 스킬이 EventMagnitude 에 자기 무력화 수치를 실어 보낸다. 미지정(0)이면 디버그 기본 10.
	const float Amount = (Payload && Payload->EventMagnitude > 0.f) ? Payload->EventMagnitude : 10.f;
	UE_LOG(LogTemp, Log, TEXT("[TerrainGimmick] Event.Boss.StaggerHit 수신 (Amount=%.1f)"), Amount);
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

void UBossTerrainGimmickComponent::BreakGrabHoldToRelease()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	// 섹션 지정 시: 현재 재생 중인 보스 몽타주(GAS 재생)의 Hold next 를 Release 로 교체.
	// SetNextSectionName 이라 돌던 Hold 루프가 끝나는 순간 자연스럽게 Release 로 흘러간다.
	// GAS(ASC) 경유라 서버 변경이 시뮬레이트 프록시(모든 클라)에 복제된다.
	// CurrentMontageSetNextSectionName 은 void 라 성공 여부를 못 받으므로,
	// GAS 몽타주가 실제 재생 중일 때만(GetCurrentMontage) 섹션 예약하고 아니면 폴백.
	if (GrabHoldSection != NAME_None && GrabReleaseSection != NAME_None)
	{
		if (UAbilitySystemComponent* ASC = GetASC())
		{
			if (ASC->GetCurrentMontage() != nullptr)
			{
				ASC->CurrentMontageSetNextSectionName(GrabHoldSection, GrabReleaseSection);
				return;	// 풀림 섹션 예약 -> 던짐은 Release 의 GrabRelease 노티파이가 실행
			}
		}
	}

	// 폴백: 섹션 미지정이거나 GAS 몽타주가 아니면 즉시 직접 해제(던짐) — 구버전/안전망
	ReleaseBossGrabs();
}

void UBossTerrainGimmickComponent::OnStaggerWindowTimeout()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !bStaggerPhaseActive)
	{
		return;
	}

	// 시간초과: 게이지 내리고(EndStaggerPhase) 붙잡기 루프를 풀림 섹션으로.
	// EndStaggerPhase 가 StaggerWindowTimer 를 지우지만 이미 만료된 상태라 무해.
	EndStaggerPhase();
	BreakGrabHoldToRelease();
}

void UBossTerrainGimmickComponent::EndStaggerPhase()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !bStaggerPhaseActive)
	{
		return;
	}
	bStaggerPhaseActive = false;

	// 구출 윈도우 타이머 취소 (무력화가 먼저 성공했거나 페이즈가 끝난 경우 시간초과가 또 뜨지 않게)
	Owner->GetWorldTimerManager().ClearTimer(StaggerWindowTimer);

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
					// 구출(무력화 성공): 붙잡기 루프를 풀림 섹션으로. 던짐/데미지는 Release 노티파이가 실행.
					// (시간초과 경로와 완전 대칭 — 차이는 '언제 빠져나가느냐'뿐)
					BreakGrabHoldToRelease();
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

	ABossRaidGameMode* GM = GetWorld()->GetAuthGameMode<ABossRaidGameMode>();
	UE_LOG(LogTemp, Warning, TEXT("[TerrainGimmick] ExecuteDestroySlice: Slice=%d GameMode=%s"),
		PendingDestroySliceIndex, GM ? TEXT("OK") : TEXT("NULL(레벨 GameMode 가 BossRaidGameMode 인지 확인)"));
	if (GM)
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
