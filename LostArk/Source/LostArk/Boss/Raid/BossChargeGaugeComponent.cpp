// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Raid/BossChargeGaugeComponent.h"
#include "Boss/Damage/BossPatternActorBase.h"
#include "Boss/BossGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

UBossChargeGaugeComponent::UBossChargeGaugeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);	// 런타임 부착이라도 Gauge 복제가 동작하게
}

void UBossChargeGaugeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UBossChargeGaugeComponent, Gauge);
}

void UBossChargeGaugeComponent::InitChargeGauge(AActor* InBoss,
	TSubclassOf<UGameplayEffect> InRedChargeEffect, TSubclassOf<UGameplayEffect> InBlueChargeEffect)
{
	Boss = InBoss;
	RedChargeEffect = InRedChargeEffect;
	BlueChargeEffect = InBlueChargeEffect;
}

UAbilitySystemComponent* UBossChargeGaugeComponent::GetOwnerASC() const
{
	return GetOwner() ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()) : nullptr;
}

void UBossChargeGaugeComponent::AddGauge(float Amount)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	const float Old = Gauge;
	Gauge = FMath::Clamp(Old + Amount, 0.f, MaxGauge);
	if (Gauge == Old)
	{
		return;
	}
	OnChargeGaugeChanged.Broadcast(Gauge, MaxGauge);	// 클라는 OnRep 에서 방송

	// 절반 '상향 돌파' 1회: 전하 반전 (내려갔다 다시 넘으면 또 반전 — 게이지 감소 수단이 생겨도 일관)
	const float Half = MaxGauge * 0.5f;
	if (Old < Half && Gauge >= Half)
	{
		FlipOwnerCharge();
	}

	// 가득 차는 순간: 과충전 폭발. 게이지는 유지(기본) — 이미 가득이면 재발동 없음
	if (Old < MaxGauge && Gauge >= MaxGauge)
	{
		TriggerOvercharge();
		if (bResetGaugeAfterOvercharge)
		{
			Gauge = 0.f;
			OnChargeGaugeChanged.Broadcast(Gauge, MaxGauge);
		}
	}
}

void UBossChargeGaugeComponent::ResetGauge()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || Gauge == 0.f)
	{
		return;
	}
	Gauge = 0.f;
	OnChargeGaugeChanged.Broadcast(Gauge, MaxGauge);
}

void UBossChargeGaugeComponent::FlipOwnerCharge()
{
	// 전하 변환장판(UBossAoeChargeSwapEffect)과 동일 로직: 가진 쪽 GE 제거 + 반대 GE 부여
	UAbilitySystemComponent* ASC = GetOwnerASC();
	if (!ASC)
	{
		return;
	}

	const bool bHasRed = ASC->HasMatchingGameplayTag(LostArkTags::State_Charge_Red);
	const bool bHasBlue = ASC->HasMatchingGameplayTag(LostArkTags::State_Charge_Blue);
	if (bHasRed == bHasBlue)
	{
		return;	// 전하 미부여(또는 비정상 중복)면 스킵
	}

	const TSubclassOf<UGameplayEffect> RemoveGE = bHasRed ? RedChargeEffect : BlueChargeEffect;
	const TSubclassOf<UGameplayEffect> AddGE = bHasRed ? BlueChargeEffect : RedChargeEffect;

	if (RemoveGE)
	{
		ASC->RemoveActiveGameplayEffectBySourceEffect(RemoveGE, nullptr);
	}
	if (AddGE)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(AddGE, 1.f, Context);
		if (Spec.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
	}
}

void UBossChargeGaugeComponent::TriggerOvercharge()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || !OverchargeAoeClass)
	{
		return;
	}

	// 이 캐릭터 위치에서 스폰. 추적(FollowTarget)/예고/반경은 장판 BP 설정이 담당
	const FTransform SpawnTM(Owner->GetActorRotation(), Owner->GetActorLocation());
	ABossPatternActorBase* Aoe = World->SpawnActorDeferred<ABossPatternActorBase>(
		OverchargeAoeClass, SpawnTM, Boss.Get(), Cast<APawn>(Boss.Get()),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Aoe)
	{
		return;
	}

	// 시전자=보스(데미지 귀속), 타겟=과충전된 이 플레이어 (FollowTarget 이 이 폰을 따라감)
	Aoe->InitAoe(Boss.Get(), Owner, OverchargeDamageCoefficient);
	Aoe->FinishSpawning(SpawnTM);
}

void UBossChargeGaugeComponent::OnRep_Gauge()
{
	OnChargeGaugeChanged.Broadcast(Gauge, MaxGauge);
}
