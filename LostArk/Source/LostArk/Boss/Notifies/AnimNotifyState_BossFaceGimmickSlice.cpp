// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotifyState_BossFaceGimmickSlice.h"
#include "Boss/Notifies/BossNotifyHelpers.h"
#include "Boss/Gimmick/BossTerrainGimmickComponent.h"

void UAnimNotifyState_BossFaceGimmickSlice::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (bEndStaggerPhaseOnBegin)
	{
		if (UBossTerrainGimmickComponent* Gimmick = BossNotify::GetServerComponent<UBossTerrainGimmickComponent>(MeshComp))
		{
			Gimmick->EndStaggerPhase();
		}
	}
}

void UAnimNotifyState_BossFaceGimmickSlice::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	AActor* Owner = BossNotify::GetServerOwner(MeshComp);
	if (!Owner)
	{
		return;
	}

	UBossTerrainGimmickComponent* Gimmick = Owner->FindComponentByClass<UBossTerrainGimmickComponent>();
	FVector GimmickLocation;
	if (!Gimmick || !Gimmick->GetGimmickSliceLocation(GimmickLocation))
	{
		return;
	}

	// 평지 기준 yaw 만 기믹 위치로 보간 (캡슐+메시 함께. 회전은 액터 복제로 클라 전파)
	FVector To = GimmickLocation - Owner->GetActorLocation();
	To.Z = 0.f;
	if (!To.Normalize())
	{
		return;
	}

	const FRotator Current = Owner->GetActorRotation();
	FRotator Goal = To.Rotation();
	Goal.Pitch = Current.Pitch;
	Goal.Roll = Current.Roll;
	Owner->SetActorRotation(FMath::RInterpTo(Current, Goal, FrameDeltaTime, RotationInterpSpeed));
}

FString UAnimNotifyState_BossFaceGimmickSlice::GetNotifyName_Implementation() const
{
	return TEXT("Gimmick: Face Slice");
}
