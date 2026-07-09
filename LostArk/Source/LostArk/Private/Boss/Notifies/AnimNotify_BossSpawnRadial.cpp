// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotify_BossSpawnRadial.h"
#include "Boss/Damage/BossAoe_Circle.h"
#include "Engine/World.h"

void UAnimNotify_BossSpawnRadial::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	// 부모(BossSpawnAoe)의 Notify 는 단일 스폰이므로 호출하지 않고, 준비 로직(PrepareSpawn)만 재사용해
	// 방향만 바꿔 여러 번 SpawnAoeActor 한다.
	UWorld* World = nullptr;
	AActor* Boss = nullptr;
	AActor* Target = nullptr;
	FTransform BaseTM;
	if (!PrepareSpawn(MeshComp, World, Boss, Target, BaseTM))
	{
		return;
	}

	const int32 Count = FMath::Max(ProjectileCount, 1);
	const float Step = (AngleStepDeg > KINDA_SMALL_NUMBER) ? AngleStepDeg : (360.f / Count);

	for (int32 i = 0; i < Count; ++i)
	{
		const float Yaw = StartAngleDeg + Step * i;

		// 월드 Z축 기준 Yaw 를 보스 회전 위에 얹어 각 투사체의 발사 방향을 만든다.
		FTransform TM = BaseTM;
		const FQuat WorldYaw = FRotator(0.f, Yaw, 0.f).Quaternion();
		TM.SetRotation(WorldYaw * BaseTM.GetRotation());

		SpawnAoeActor(World, Boss, Target, TM);
	}
}

void UAnimNotify_BossSpawnRadial::ConfigureAoe(ABossPatternActorBase* Aoe) const
{
	if (ABossAoe_Circle* Circle = Cast<ABossAoe_Circle>(Aoe))
	{
		Circle->Radius = Radius;
	}
}

FString UAnimNotify_BossSpawnRadial::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("SpawnRadial x%d"), ProjectileCount);
}
