// Fill out your copyright notice in the Description page of Project Settings.


#include "Boss/Raid/BossArenaCamera.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ABossArenaCamera::ABossArenaCamera()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);

	// 클라에 존재해야 ViewTarget 지정 가능. 항상 관련(거리 릴러번시로 끊기면 안 됨)
	bReplicates = true;
	bAlwaysRelevant = true;
	// 움직임은 복제하지 않는다 — 각 클라가 자기 로컬 플레이어 기준으로 계산
}

void ABossArenaCamera::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABossArenaCamera, Boss);
}

void ABossArenaCamera::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 데디 서버는 렌더링하지 않으므로 계산 불필요
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 이 머신의 로컬 플레이어 폰
	APawn* LocalPawn = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->IsLocalController())
		{
			LocalPawn = PC->GetPawn();
			break;
		}
	}

	if (!LocalPawn || !Boss)
	{
		return;
	}

	const FVector BossLoc = Boss->GetActorLocation();
	const FVector PawnLoc = LocalPawn->GetActorLocation();

	// 카메라가 바라볼 방위각 = 플레이어 -> 보스 방향의 yaw (보스를 화면 위로)
	FVector ToBoss = BossLoc - PawnLoc;
	ToBoss.Z = 0.f;
	float Yaw = LastYaw;
	if (ToBoss.SizeSquared() > 1.f)
	{
		Yaw = ToBoss.Rotation().Yaw;
		LastYaw = Yaw;
	}

	// 피치는 고정(탑다운 유지), yaw만 회전 -> 리지드 붐
	const FRotator DesiredRot(Pitch, Yaw, 0.f);

	// 포커스 = 플레이어(살짝 보스 쪽으로 당김) + 높이. 카메라는 붐 길이만큼 뒤로
	const FVector Focus = FMath::Lerp(PawnLoc, BossLoc, FocusBiasToBoss) + FVector(0.f, 0.f, FocusHeightOffset);
	const FVector DesiredLoc = Focus - DesiredRot.Vector() * BoomLength;

	if (!bSnapped)
	{
		SetActorLocationAndRotation(DesiredLoc, DesiredRot);
		bSnapped = true;
		return;
	}

	SetActorLocation(FMath::VInterpTo(GetActorLocation(), DesiredLoc, DeltaTime, LocationInterpSpeed));
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), DesiredRot, DeltaTime, RotationInterpSpeed));
}
