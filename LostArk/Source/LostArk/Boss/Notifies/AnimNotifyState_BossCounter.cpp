// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotifyState_BossCounter.h"
#include "Boss/Notifies/BossNotifyHelpers.h"

void UAnimNotifyState_BossCounter::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UBossCounterComponent* Counter = BossNotify::GetServerComponent<UBossCounterComponent>(MeshComp))
	{
		Counter->OpenWindow(CounterType, RequiredPlayers);
	}
}

void UAnimNotifyState_BossCounter::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (UBossCounterComponent* Counter = BossNotify::GetServerComponent<UBossCounterComponent>(MeshComp))
	{
		Counter->CloseWindow();
	}
}

FString UAnimNotifyState_BossCounter::GetNotifyName_Implementation() const
{
	switch (CounterType)
	{
	case EBossCounterType::MultiCounter:	return FString::Printf(TEXT("Multi Counter x%d"), RequiredPlayers);
	case EBossCounterType::FakeCounter:		return TEXT("Fake Counter");
	default:								return TEXT("Counter");
	}
}
