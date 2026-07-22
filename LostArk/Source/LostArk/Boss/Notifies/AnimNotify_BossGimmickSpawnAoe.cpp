// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotify_BossGimmickSpawnAoe.h"
#include "Boss/Damage/BossPatternActorBase.h"
#include "Boss/Gimmick/BossTerrainGimmickComponent.h"

void UAnimNotify_BossGimmickSpawnAoe::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	// Super::Notify(부모 UAnimNotify_BossSpawnAoe)는 보스 기준으로 스폰해버리므로 건너뛰고,
	// 공통 준비(PrepareSpawn)만 재사용해 위치를 기믹 위치로 바꿔 직접 스폰한다
	UWorld* World = nullptr;
	AActor* Boss = nullptr;
	AActor* Target = nullptr;
	FTransform SpawnTM;
	if (!PrepareSpawn(MeshComp, World, Boss, Target, SpawnTM))
	{
		return;
	}

	UBossTerrainGimmickComponent* Gimmick = Boss->FindComponentByClass<UBossTerrainGimmickComponent>();
	FVector GimmickLocation;
	if (!Gimmick || !Gimmick->GetGimmickSliceLocation(GimmickLocation))
	{
		return;	// 기믹 라운드 미시작 (SpawnGimmickTower 가 먼저 불렸어야 함)
	}

	// 위치 = 기믹 지형 (+노티파이 LocationOffset 은 기믹 위치 기준으로 재적용),
	// 회전 = 기믹 위치 -> 보스 방향 (저스트가드 방향 판정 축과 일치)
	FVector ToBoss = Boss->GetActorLocation() - GimmickLocation;
	ToBoss.Z = 0.f;
	const FRotator Rot = ToBoss.Normalize() ? ToBoss.Rotation() : SpawnTM.Rotator();

	SpawnTM = FTransform(Rot, GimmickLocation + Rot.RotateVector(LocationOffset));
	SpawnAoeActor(World, Boss, Target, SpawnTM);
}

void UAnimNotify_BossGimmickSpawnAoe::ConfigureAoe(ABossPatternActorBase* Aoe) const
{
	Super::ConfigureAoe(Aoe);
	if (Aoe)
	{
		// BP 가 CasterLocation 등으로 설정돼 있어도 기믹 위치가 유지되게 강제
		Aoe->SetSpawnOriginPolicy(EAoeSpawnOrigin::SpawnTransform);
	}
}

FString UAnimNotify_BossGimmickSpawnAoe::GetNotifyName_Implementation() const
{
	if (AoeClass)
	{
		return FString::Printf(TEXT("Gimmick AOE: %s"), *AoeClass->GetName());
	}
	return TEXT("Gimmick AOE");
}
