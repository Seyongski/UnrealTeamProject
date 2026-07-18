// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Gimmick/BossGimmickTower.h"
#include "Boss/Combat/BossCombatStatics.h"
#include "Boss/BossGameplayTags.h"
#include "Boss/Raid/BossRaidGameState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

ABossGimmickTower::ABossGimmickTower()
{
	PrimaryActorTick.bCanEverTick = false;

	// 서버 권위: 스폰/파괴가 복제로 클라에 전파. 이동이 없으므로 무브먼트 복제 불필요
	bReplicates = true;

	TowerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TowerMesh"));
	SetRootComponent(TowerMesh);
	// 판정은 장판(도형 수식)으로 하므로 콜리전은 시각 차폐/이동 차단 용도만. 세부는 BP에서
	TowerMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void ABossGimmickTower::InitTower(AActor* InBoss, int32 InSliceIndex)
{
	Boss = InBoss;
	SliceIndex = InSliceIndex;
}

void ABossGimmickTower::BeginPlay()
{
	Super::BeginPlay();

	// 감전 장판 스폰/슬라이스 감시는 서버 전용 (파괴는 복제로 전파)
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		ShockSpawnTimer, this, &ABossGimmickTower::SpawnShockAoe,
		FMath::Max(ShockSpawnInterval, 0.1f), true, FMath::Max(FirstShockDelay, 0.f));

	TryBindToGameState();
}

void ABossGimmickTower::TryBindToGameState()
{
	if (ABossRaidGameState* GS = GetWorld()->GetGameState<ABossRaidGameState>())
	{
		GS->OnArenaSlicesChanged.AddDynamic(this, &ABossGimmickTower::HandleSlicesChanged);
		HandleSlicesChanged();	// 스폰 시점에 이미 파괴된 슬라이스 위인 경우 즉시 정리
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			BindRetryTimer, this, &ABossGimmickTower::TryBindToGameState, 0.25f, false);
	}
}

void ABossGimmickTower::HandleSlicesChanged()
{
	const ABossRaidGameState* GS = GetWorld()->GetGameState<ABossRaidGameState>();
	if (GS && SliceIndex != INDEX_NONE && GS->IsSliceDestroyed(SliceIndex))
	{
		// 레이저로 못 부쉈어도 발 딛던 지형이 사라지면 함께 소멸
		DestroyTower(/*bByLaser=*/false);
	}
}

void ABossGimmickTower::OnBossLaserHit()
{
	DestroyTower(/*bByLaser=*/true);
}

void ABossGimmickTower::DestroyTower(bool bByLaser)
{
	if (bDying || !HasAuthority())
	{
		return;
	}
	bDying = true;

	GetWorldTimerManager().ClearTimer(ShockSpawnTimer);
	GetWorldTimerManager().ClearTimer(BindRetryTimer);

	// 연출(전 머신) -> 유예 후 실제 소멸 (Destroy 는 복제로 클라 전파)
	MulticastTowerDestroyed(bByLaser);
	SetLifeSpan(FMath::Max(DestroyFXDelay, 0.01f));
}

void ABossGimmickTower::MulticastTowerDestroyed_Implementation(bool bByLaser)
{
	OnTowerDestroyed(bByLaser);
}

void ABossGimmickTower::SpawnShockAoe()
{
	UWorld* World = GetWorld();
	if (!World || bDying || !ShockAoeClass)
	{
		return;
	}

	// 생존 플레이어 중 랜덤 1명 (없으면 이번 주기 스킵)
	TArray<APawn*> PlayerPawns;
	UBossCombatStatics::GetPlayerPawns(World, PlayerPawns);
	PlayerPawns.RemoveAll([](const APawn* Pawn)
	{
		return !UBossCombatStatics::IsAliveActor(Pawn, LostArkTags::State_Dead.GetTag());
	});
	if (PlayerPawns.Num() == 0)
	{
		return;
	}
	APawn* Target = PlayerPawns[FMath::RandRange(0, PlayerPawns.Num() - 1)];

	// 타워 위치에서 타겟 방향(수평)으로 Rect 를 눕힌다
	FVector Dir = Target->GetActorLocation() - GetActorLocation();
	Dir.Z = 0.f;
	const FRotator Rot = Dir.Normalize() ? Dir.Rotation() : GetActorRotation();
	const FTransform SpawnTM(Rot, GetActorLocation());

	ABossPatternActorBase* Aoe = World->SpawnActorDeferred<ABossPatternActorBase>(
		ShockAoeClass, SpawnTM, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Aoe)
	{
		return;
	}

	// 시전자 = 보스 (데미지 귀속/바닥 폴백). 보스가 사라진 예외 상황에선 타워 자신
	Aoe->InitAoe(Boss.IsValid() ? Boss.Get() : this, Target, ShockDamageCoefficient);
	if (bOverrideCommon)
	{
		Aoe->ApplyCommonOverride(CommonOverride);
	}
	// BP 의 SpawnOrigin 설정과 무관하게 '타워 위치·방향 그대로' 강제
	// (CasterLocation 등으로 돼 있으면 장판이 보스 위치로 끌려가는 사고 방지)
	Aoe->SetSpawnOriginPolicy(EAoeSpawnOrigin::SpawnTransform);
	Aoe->FinishSpawning(SpawnTM);
}
