// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotify_BossSpawnAoe.h"
#include "Boss/Damage/BossPatternActorBase.h"
#include "Boss/Targeting/BossTargetingComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

void UAnimNotify_BossSpawnAoe::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	UWorld* World = nullptr;
	AActor* Boss = nullptr;
	AActor* Target = nullptr;
	FTransform SpawnTM;
	if (!PrepareSpawn(MeshComp, World, Boss, Target, SpawnTM))
	{
		return;
	}

	SpawnAoeActor(World, Boss, Target, SpawnTM);
}

bool UAnimNotify_BossSpawnAoe::PrepareSpawn(USkeletalMeshComponent* MeshComp, UWorld*& OutWorld,
	AActor*& OutBoss, AActor*& OutTarget, FTransform& OutBaseTM) const
{
	AActor* Boss = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!Boss || !AoeClass)
	{
		return false;
	}

	// 스폰/판정은 서버 권위에서만 (복제로 클라 전파)
	if (!Boss->HasAuthority())
	{
		return false;
	}

	UWorld* World = Boss->GetWorld();
	if (!World)
	{
		return false;
	}

	// 초기 스폰 트랜스폼: 소켓 지정 시 소켓 위치, 아니면 보스 위치 (+오프셋)
	const bool bHasSocket = (SpawnSocketName != NAME_None && MeshComp->DoesSocketExist(SpawnSocketName));
	FTransform SpawnTM = bHasSocket ? MeshComp->GetSocketTransform(SpawnSocketName) : Boss->GetActorTransform();

	// 회전(=장판 발사 방향) 결정
	if (bAimBySocketForward)
	{
		// 레이저 등: 헤드(소켓) 정면의 X/Y 만 사용 (Z 무시 = pitch/roll 0, yaw 만).
		// 소켓이 없으면 보스 액터 정면을 평면으로 눕혀 쓴다.
		const FQuat SrcRot = bHasSocket ? MeshComp->GetSocketQuaternion(SpawnSocketName) : Boss->GetActorQuat();
		FVector Fwd = SrcRot.GetForwardVector();
		Fwd.Z = 0.f;
		SpawnTM.SetRotation(Fwd.Normalize() ? Fwd.ToOrientationQuat() : Boss->GetActorQuat());
	}
	else if (bHasSocket)
	{
		// 기존 동작: 소켓 트위스트 무시하고 보스 액터 회전으로 방향 일관성 유지
		SpawnTM.SetRotation(Boss->GetActorQuat());
	}
	SpawnTM.AddToTranslation(SpawnTM.GetRotation().RotateVector(LocationOffset));

	// 시전자(거대 보스) 스케일이 AOE 액터에 실리면 데칼/판정 크기가 왜곡된다.
	// AOE 는 월드 cm 절대값으로 판정하므로 항상 스케일 1로 스폰.
	SpawnTM.SetScale3D(FVector::OneVector);

	// 타겟 = 보스 타겟팅 컴포넌트의 현재 타겟 (Homing/Target 원점용)
	AActor* Target = nullptr;
	if (const UBossTargetingComponent* Targeting = Boss->FindComponentByClass<UBossTargetingComponent>())
	{
		Target = Targeting->GetCurrentTarget();
	}

	OutWorld = World;
	OutBoss = Boss;
	OutTarget = Target;
	OutBaseTM = SpawnTM;
	return true;
}

ABossPatternActorBase* UAnimNotify_BossSpawnAoe::SpawnAoeActor(UWorld* World, AActor* Boss,
	AActor* Target, const FTransform& SpawnTM) const
{
	// 공용 팩토리가 [Deferred 스폰 -> InitAoe -> 오버라이드 주입 -> FinishSpawning] 순서를 보증
	return ABossPatternActorBase::SpawnAoeDeferred(World, AoeClass, SpawnTM,
		/*SpawnOwner=*/Boss, /*Caster=*/Boss, Target, DamageCoefficient,
		[this](ABossPatternActorBase& Aoe)
		{
			if (bOverrideCommon)
			{
				Aoe.ApplyCommonOverride(CommonOverride);	// 패턴별 공통값 주입
			}
			if (bOverrideGrab)
			{
				// OnHitEffect 는 Instanced 라 이 스폰 인스턴스만 덮어씀 (BP 클래스 디폴트는 불변)
				if (UBossAoeGrabEffect* Grab = Cast<UBossAoeGrabEffect>(Aoe.GetOnHitEffect()))
				{
					Grab->ApplyOverride(GrabOverride);
				}
			}
			if (bOverrideKnockback)
			{
				Aoe.SetKnockbackOverride(KnockbackOverride);	// 패턴별 피격 리액션(넉백/낙사) 주입
			}
			// 판정 중심 오프셋 (BeginPlay 의 ResolveOrigin 전에 주입, 0 은 BP 기본 유지).
			// 같은 장판 BP 를 노티파이마다 다른 좌/우·전후 배치로 재사용하는 용도.
			if (!ShapeOffset.IsNearlyZero())
			{
				Aoe.SetShapeOffset(ShapeOffset);
			}
			// 본체 VFX 노티파이 오버라이드 (BeginPlay 의 SpawnBodyEffect 전에 주입, null 은 BP 기본 유지)
			if (BodyEffectOverride || BodyEffectCascadeOverride)
			{
				Aoe.SetBodyEffectOverride(BodyEffectOverride, BodyEffectCascadeOverride);
			}

			ConfigureAoe(&Aoe);	// 도형별 파라미터 주입 (BeginPlay 전)
		});
}

FString UAnimNotify_BossSpawnAoe::GetNotifyName_Implementation() const
{
	if (AoeClass)
	{
		return FString::Printf(TEXT("SpawnAOE: %s"), *AoeClass->GetName());
	}
	return TEXT("SpawnAOE");
}
