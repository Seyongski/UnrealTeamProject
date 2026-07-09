// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotifyState_BossCounter.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	// 서버 권한일 때만 카운터 컴포넌트 반환 (창 토글은 서버 전용)
	UBossCounterComponent* GetServerCounterComponent(USkeletalMeshComponent* MeshComp)
	{
		AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
		if (!Owner || !Owner->HasAuthority())
		{
			return nullptr;
		}
		return Owner->FindComponentByClass<UBossCounterComponent>();
	}
}

void UAnimNotifyState_BossCounter::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UBossCounterComponent* Counter = GetServerCounterComponent(MeshComp))
	{
		Counter->OpenWindow(CounterType, RequiredPlayers);
	}
}

void UAnimNotifyState_BossCounter::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (UBossCounterComponent* Counter = GetServerCounterComponent(MeshComp))
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
