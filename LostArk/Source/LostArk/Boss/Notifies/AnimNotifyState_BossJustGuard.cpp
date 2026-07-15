// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotifyState_BossJustGuard.h"
#include "Boss/Combat/BossJustGuardComponent.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	// 서버 권한일 때만 저스트가드 컴포넌트 반환 (창 토글은 서버 전용)
	UBossJustGuardComponent* GetServerJustGuardComponent(USkeletalMeshComponent* MeshComp)
	{
		AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
		if (!Owner || !Owner->HasAuthority())
		{
			return nullptr;
		}
		return Owner->FindComponentByClass<UBossJustGuardComponent>();
	}
}

void UAnimNotifyState_BossJustGuard::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UBossJustGuardComponent* JustGuard = GetServerJustGuardComponent(MeshComp))
	{
		JustGuard->OpenWindow();
	}
}

void UAnimNotifyState_BossJustGuard::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (UBossJustGuardComponent* JustGuard = GetServerJustGuardComponent(MeshComp))
	{
		JustGuard->CloseWindow();
	}
}

FString UAnimNotifyState_BossJustGuard::GetNotifyName_Implementation() const
{
	return TEXT("Just Guard Window");
}
