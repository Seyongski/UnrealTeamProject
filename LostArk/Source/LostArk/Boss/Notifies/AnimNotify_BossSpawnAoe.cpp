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

	// 초기 스폰 트랜스폼: 소켓 지정 시 소켓, 아니면 보스 위치 (+오프셋)
	FTransform SpawnTM = Boss->GetActorTransform();
	if (SpawnSocketName != NAME_None && MeshComp->DoesSocketExist(SpawnSocketName))
	{
		SpawnTM = MeshComp->GetSocketTransform(SpawnSocketName);
		// 회전은 보스 기준으로 (소켓 트위스트 무시하고 장판 방향 일관성 유지)
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
	// InitAoe 를 BeginPlay(ResolveOrigin) 이전에 호출하려면 Deferred 스폰 사용
	ABossPatternActorBase* Aoe = World->SpawnActorDeferred<ABossPatternActorBase>(
		AoeClass, SpawnTM, Boss, Cast<APawn>(Boss),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Aoe)
	{
		return nullptr;
	}

	Aoe->InitAoe(Boss, Target, DamageCoefficient);
	if (bOverrideCommon)
	{
		Aoe->ApplyCommonOverride(CommonOverride);	// 패턴별 공통값 주입
	}
	if (bOverrideGrab)
	{
		// OnHitEffect 는 Instanced 라 이 스폰 인스턴스만 덮어씀 (BP 클래스 디폴트는 불변)
		if (UBossAoeGrabEffect* Grab = Cast<UBossAoeGrabEffect>(Aoe->GetOnHitEffect()))
		{
			Grab->ApplyOverride(GrabOverride);
		}
	}
	if (bOverrideKnockback)
	{
		Aoe->SetKnockbackOverride(KnockbackOverride);	// 패턴별 피격 리액션(넉백/낙사) 주입
	}
	// 본체 VFX 노티파이 오버라이드 (BeginPlay 의 SpawnBodyEffect 전에 주입, null 은 BP 기본 유지)
	if (BodyEffectOverride || BodyEffectCascadeOverride)
	{
		Aoe->SetBodyEffectOverride(BodyEffectOverride, BodyEffectCascadeOverride);
	}

	ConfigureAoe(Aoe);	// 도형별 파라미터 주입 (BeginPlay 전)
	Aoe->FinishSpawning(SpawnTM);
	return Aoe;
}

FString UAnimNotify_BossSpawnAoe::GetNotifyName_Implementation() const
{
	if (AoeClass)
	{
		return FString::Printf(TEXT("SpawnAOE: %s"), *AoeClass->GetName());
	}
	return TEXT("SpawnAOE");
}
