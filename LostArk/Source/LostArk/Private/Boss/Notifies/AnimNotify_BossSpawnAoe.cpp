// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotify_BossSpawnAoe.h"
#include "Damage/BossPatternActorBase.h"
#include "Boss/Targeting/BossTargetingComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

void UAnimNotify_BossSpawnAoe::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Boss = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!Boss || !AoeClass)
	{
		return;
	}

	// 스폰/판정은 서버 권위에서만 (복제로 클라 전파)
	if (!Boss->HasAuthority())
	{
		return;
	}

	UWorld* World = Boss->GetWorld();
	if (!World)
	{
		return;
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

	// 타겟 = 보스 타겟팅 컴포넌트의 현재 타겟 (Homing/Target 원점용)
	AActor* Target = nullptr;
	if (const UBossTargetingComponent* Targeting = Boss->FindComponentByClass<UBossTargetingComponent>())
	{
		Target = Targeting->GetCurrentTarget();
	}

	FActorSpawnParameters Params;
	Params.Owner = Boss;
	Params.Instigator = Cast<APawn>(Boss);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// InitAoe 를 BeginPlay(ResolveOrigin) 이전에 호출하려면 Deferred 스폰 사용
	ABossPatternActorBase* Aoe = World->SpawnActorDeferred<ABossPatternActorBase>(
		AoeClass, SpawnTM, Boss, Cast<APawn>(Boss),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Aoe)
	{
		return;
	}

	Aoe->InitAoe(Boss, Target, DamageCoefficient);
	if (bOverrideCommon)
	{
		Aoe->ApplyCommonOverride(CommonOverride);	// 패턴별 공통값 주입
	}
	ConfigureAoe(Aoe);	// 도형별 파라미터 주입 (BeginPlay 전)
	Aoe->FinishSpawning(SpawnTM);
}

FString UAnimNotify_BossSpawnAoe::GetNotifyName_Implementation() const
{
	if (AoeClass)
	{
		return FString::Printf(TEXT("SpawnAOE: %s"), *AoeClass->GetName());
	}
	return TEXT("SpawnAOE");
}
