// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotify_BossSpawnRadial.h"
#include "Boss/Damage/BossAoe_Circle.h"
#include "Boss/Damage/BossAoe_Rect.h"
#include "Boss/Damage/BossAoe_Sector.h"
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
	// 1) 도형별 파라미터 주입 — 선택한 Shape 에 맞는 값만 적용한다.
	//    (Cast 실패 = Shape 와 AoeClass BP 불일치. 이땐 아무것도 적용 안 돼 오설정이 드러난다)
	switch (Shape)
	{
	case ERadialShape::Circle:
		if (ABossAoe_Circle* Circle = Cast<ABossAoe_Circle>(Aoe))
		{
			Circle->Radius = Radius;
			Circle->InnerRadius = InnerRadius;
		}
		break;

	case ERadialShape::Rect:
		if (ABossAoe_Rect* Rect = Cast<ABossAoe_Rect>(Aoe))
		{
			Rect->HalfLength = RectHalfLength;
			Rect->HalfWidth = RectHalfWidth;
			Rect->ForwardOffset = RectForwardOffset;
		}
		break;

	case ERadialShape::Sector:
		if (ABossAoe_Sector* Sector = Cast<ABossAoe_Sector>(Aoe))
		{
			Sector->Radius = SectorRadius;
			Sector->InnerRadius = SectorInnerRadius;
			Sector->StartAngle = SectorStartAngle;
			Sector->EndAngle = SectorEndAngle;
		}
		break;
	}

	// 2) 발사 셋업 강제 주입 — Straight + SpawnOrigin=SpawnTransform 고정이라
	//    BP/CommonOverride 설정이 어떻든 스폰 트랜스폼 방향으로 확실히 뻗어나간다.
	//    (ConfigureAoe 는 ApplyCommonOverride 이후에 불리므로 여기 값이 최종 우선)
	Aoe->SetupStraightProjectile(Speed, Range, HitInterval, CastTime);
}

FString UAnimNotify_BossSpawnRadial::GetNotifyName_Implementation() const
{
	const FString ShapeName = AoeClass ? AoeClass->GetName() : TEXT("None");
	return FString::Printf(TEXT("Radial x%d: %s"), ProjectileCount, *ShapeName);
}
