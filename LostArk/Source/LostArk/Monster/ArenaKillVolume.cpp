// Fill out your copyright notice in the Description page of Project Settings.


#include "Monster/ArenaKillVolume.h"
#include "Boss/BossGameplayTags.h"
#include "Components/BoxComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Pawn.h"
#include "Character/LostArkAttributeSet.h"
#include "Core/LostArkCombatInterface.h"

AArenaKillVolume::AArenaKillVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(5000.f, 5000.f, 200.f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Box->SetGenerateOverlapEvents(true);
}

void AArenaKillVolume::BeginPlay()
{
	Super::BeginPlay();
	Box->OnComponentBeginOverlap.AddDynamic(this, &AArenaKillVolume::OnBoxBeginOverlap);
}

void AArenaKillVolume::OnBoxBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	// 낙사 판정은 서버 권위
	if (!HasAuthority() || !Cast<APawn>(OtherActor))
	{
		return;
	}

	UAbilitySystemComponent* ASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);
	if (!ASC)
	{
		return;
	}

	// ★ 이미 사망 상태이거나 부활 직후 무적(State.Invincible)인 경우 중복 낙사 처리 금지!
	if (ASC->HasMatchingGameplayTag(LostArkTags::State_Dead) || 
		ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Invincible"))))
	{
		return;
	}

	if (FallDeathEffect)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(FallDeathEffect, 1.f, Context);
		if (Spec.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
	}
	else
	{
		// 사망 태그 부여 + 체력 0 세팅
		ASC->AddLooseGameplayTag(LostArkTags::State_Dead);
		ASC->AddReplicatedLooseGameplayTag(LostArkTags::State_Dead);

		ASC->SetNumericAttributeBase(ULostArkAttributeSet::GetHealthAttribute(), 0.f);
	}

	// 캐릭터 CombatInterface 사망 함수 연동 (콜리전/이동 모드/애니메이션 정지 처리)
	if (ILostArkCombatInterface* Combat = Cast<ILostArkCombatInterface>(OtherActor))
	{
		if (!Combat->IsDead())
		{
			Combat->Die();
		}
	}
}

