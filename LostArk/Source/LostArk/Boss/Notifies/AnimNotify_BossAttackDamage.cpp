// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotify_BossAttackDamage.h"
#include "Boss/Notifies/BossNotifyHelpers.h"
#include "Boss/BossBase.h"
#include "Boss/Combat/BossCombatStatics.h"
#include "Boss/BossGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

UAnimNotify_BossAttackDamage::UAnimNotify_BossAttackDamage()
{
}

void UAnimNotify_BossAttackDamage::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* ServerOwner = BossNotify::GetServerOwner(MeshComp);
	if (!ServerOwner)
	{
		return;
	}

	ABossBase* Boss = Cast<ABossBase>(ServerOwner);
	if (!Boss)
	{
		return;
	}

	UAbilitySystemComponent* BossASC = Boss->GetAbilitySystemComponent();
	if (!BossASC)
	{
		return;
	}

	// 1. 적용할 데미지 이펙트 결정
	TSubclassOf<UGameplayEffect> EffectToApply = CustomDamageEffectClass;
	if (!EffectToApply)
	{
		EffectToApply = Boss->DefaultDamageEffectClass;
	}

	// 2. 최종 데미지 양 계산
	const float FinalDamage = Boss->BaseAttackDamage * FMath::Max(0.1f, DamageCoefficient);

	// 3. 판정 중심 위치 계산
	const FVector BossLoc = Boss->GetActorLocation();
	const FVector FrontDir = Boss->GetActorForwardVector();
	const FVector DamageCenter = BossLoc + FrontDir * FrontOffset;

	// 4. 범위 내 생존 플레이어 탐색 후 데미지 적용
	TArray<APawn*> PlayerPawns;
	UBossCombatStatics::GetPlayerPawns(Boss->GetWorld(), PlayerPawns);

	int32 HitPlayerCount = 0;
	for (APawn* PlayerPawn : PlayerPawns)
	{
		if (!PlayerPawn) continue;

		const float DistSq = FVector::DistSquared(DamageCenter, PlayerPawn->GetActorLocation());
		if (DistSq <= FMath::Square(AttackRadius))
		{
			UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerPawn);
			if (TargetASC)
			{
				UBossCombatStatics::ApplyEffect(
					BossASC, TargetASC, EffectToApply, Boss, Boss, Boss,
					LostArkTags::Data_Damage.GetTag(), FinalDamage);

				HitPlayerCount++;
			}
		}
	}

	if (GEngine && HitPlayerCount > 0)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Orange,
			FString::Printf(TEXT("[보스 공격 피격!] 데미지: %.0f (피격 플레이어 %d명)"), FinalDamage, HitPlayerCount));
	}
}
