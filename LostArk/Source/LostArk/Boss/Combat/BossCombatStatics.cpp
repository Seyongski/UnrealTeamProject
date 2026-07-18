// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Combat/BossCombatStatics.h"
#include "Boss/BossAttributeSet.h"
#include "Boss/BossBase.h"
#include "Boss/BossGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

EBossHitZone UBossCombatStatics::GetHitZone(const AActor* BossActor, const FVector& AttackerLocation)
{
	if (!BossActor)
	{
		return EBossHitZone::None;
	}

	// 존 각도는 보스별 편집값 사용, ABossBase 가 아니면 기본값
	float HeadHalfAngle = 45.f;
	float BackHalfAngle = 45.f;
	if (const ABossBase* Boss = Cast<ABossBase>(BossActor))
	{
		HeadHalfAngle = Boss->GetHeadZoneHalfAngle();
		BackHalfAngle = Boss->GetBackZoneHalfAngle();
	}

	// 수평면(Z 제거) 기준 각도: 공중/언덕 차이로 판정이 흔들리지 않게
	FVector ToAttacker = AttackerLocation - BossActor->GetActorLocation();
	ToAttacker.Z = 0.f;
	if (!ToAttacker.Normalize())
	{
		return EBossHitZone::None;	// 보스 중심과 수평 동일 위치 -> 판정 불가
	}

	FVector Forward = BossActor->GetActorForwardVector();
	Forward.Z = 0.f;
	if (!Forward.Normalize())
	{
		return EBossHitZone::None;
	}

	const float Dot = FVector::DotProduct(Forward, ToAttacker);
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.f, 1.f)));

	if (AngleDeg <= HeadHalfAngle)
	{
		return EBossHitZone::Head;
	}
	if (AngleDeg >= 180.f - BackHalfAngle)
	{
		return EBossHitZone::Back;
	}
	return EBossHitZone::None;
}

EBossPositionalBonus UBossCombatStatics::EvaluatePositionalBonus(const AActor* BossActor,
	const FVector& AttackerLocation, EBossSkillPosition SkillPosition)
{
	// 위치 속성 없는 스킬은 약점포착이어도 보너스 없음
	if (!BossActor || SkillPosition == EBossSkillPosition::None)
	{
		return EBossPositionalBonus::None;
	}

	// 약점포착: 위치 무관 적용, 표기는 백/헤드어택 대신 '약점포착'으로 포괄
	if (const UAbilitySystemComponent* ASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(const_cast<AActor*>(BossActor)))
	{
		if (ASC->HasMatchingGameplayTag(LostArkTags::State_Boss_WeakPointExposed.GetTag()))
		{
			return EBossPositionalBonus::WeakPoint;
		}
	}

	const EBossHitZone Zone = GetHitZone(BossActor, AttackerLocation);
	if (SkillPosition == EBossSkillPosition::BackAttack && Zone == EBossHitZone::Back)
	{
		return EBossPositionalBonus::BackAttack;
	}
	if (SkillPosition == EBossSkillPosition::HeadAttack && Zone == EBossHitZone::Head)
	{
		return EBossPositionalBonus::HeadAttack;
	}
	return EBossPositionalBonus::None;
}

bool UBossCombatStatics::IsHeadZoneHit(const AActor* BossActor, const FVector& AttackerLocation)
{
	return GetHitZone(BossActor, AttackerLocation) == EBossHitZone::Head;
}

void UBossCombatStatics::ApplyStaggerDamage(AActor* BossActor, float Amount)
{
	// 게이지 조작은 서버 권위에서만 (값은 어트리뷰트 복제로 클라 전파)
	if (!BossActor || !BossActor->HasAuthority() || Amount <= 0.f)
	{
		return;
	}

	if (UAbilitySystemComponent* ASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(BossActor))
	{
		ASC->ApplyModToAttribute(
			UBossAttributeSet::GetStaggerGaugeAttribute(), EGameplayModOp::Additive, -Amount);
	}
}

void UBossCombatStatics::GetPlayerPawns(const UWorld* World, TArray<APawn*>& OutPawns)
{
	OutPawns.Reset();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (APawn* Pawn = PC ? PC->GetPawn() : nullptr)
		{
			OutPawns.Add(Pawn);
		}
	}
}

bool UBossCombatStatics::IsAliveActor(const AActor* Actor, const FGameplayTag& DeadTag)
{
	if (!Actor)
	{
		return false;
	}
	// 태그 미지정이면 판정 불가 -> 생존 간주 (대상 포함)
	if (!DeadTag.IsValid())
	{
		return true;
	}
	if (const UAbilitySystemComponent* ASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(const_cast<AActor*>(Actor)))
	{
		return !ASC->HasMatchingGameplayTag(DeadTag);
	}
	// ASC 없으면 판정 불가 -> 생존 간주
	return true;
}

void UBossCombatStatics::AddReplicatedLooseTag(UAbilitySystemComponent* ASC, const FGameplayTag& Tag)
{
	if (!ASC || !Tag.IsValid())
	{
		return;
	}
	ASC->AddLooseGameplayTag(Tag);
	ASC->AddReplicatedLooseGameplayTag(Tag);
}

void UBossCombatStatics::RemoveReplicatedLooseTag(UAbilitySystemComponent* ASC, const FGameplayTag& Tag)
{
	if (!ASC || !Tag.IsValid())
	{
		return;
	}
	ASC->RemoveLooseGameplayTag(Tag);
	ASC->RemoveReplicatedLooseGameplayTag(Tag);
}
