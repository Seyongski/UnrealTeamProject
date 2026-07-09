// Fill out your copyright notice in the Description page of Project Settings.


#include "Boss/Raid/ArenaKillVolume.h"
#include "Boss/BossGameplayTags.h"
#include "Components/BoxComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Pawn.h"

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
		// 폴백: 사망 태그만 부여 (실제 사망 처리는 플레이어 쪽 시스템)
		ASC->AddLooseGameplayTag(LostArkTags::State_Dead);
		ASC->AddReplicatedLooseGameplayTag(LostArkTags::State_Dead);
	}
}
