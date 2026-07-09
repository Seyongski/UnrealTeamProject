// Fill out your copyright notice in the Description page of Project Settings.


#include "Boss/Raid/ArenaSliceActor.h"
#include "Boss/Raid/BossRaidGameState.h"
#include "Components/StaticMeshComponent.h"
#include "NavModifierComponent.h"
#include "NavAreas/NavArea_Null.h"
#include "Engine/World.h"
#include "TimerManager.h"

AArenaSliceActor::AArenaSliceActor()
{
	PrimaryActorTick.bCanEverTick = false;

	FloorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FloorMesh"));
	SetRootComponent(FloorMesh);
	FloorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	FloorMesh->SetCollisionResponseToAllChannels(ECR_Block);
}

void AArenaSliceActor::BeginPlay()
{
	Super::BeginPlay();
	TryBindToGameState();
}

void AArenaSliceActor::TryBindToGameState()
{
	if (ABossRaidGameState* GS = GetWorld()->GetGameState<ABossRaidGameState>())
	{
		GS->OnArenaSlicesChanged.AddDynamic(this, &AArenaSliceActor::HandleSlicesChanged);
		HandleSlicesChanged();	// 늦은 참여/리로드 대비 현재 상태 즉시 반영
	}
	else
	{
		// 클라에서 GameState 복제가 액터 BeginPlay보다 늦을 수 있음 -> 재시도
		GetWorldTimerManager().SetTimer(
			BindRetryTimer, this, &AArenaSliceActor::TryBindToGameState, 0.25f, false);
	}
}

void AArenaSliceActor::HandleSlicesChanged()
{
	const ABossRaidGameState* GS = GetWorld()->GetGameState<ABossRaidGameState>();
	if (GS && GS->IsSliceDestroyed(SliceIndex))
	{
		ApplyDestroyedState();
	}
}

void AArenaSliceActor::ApplyDestroyedState()
{
	if (bDestroyedApplied)
	{
		return;
	}
	bDestroyedApplied = true;

	// 바닥 제거: 시각 + 콜리전 (콜리전이 사라지면 다이내믹 내비메시에서도 구멍이 됨)
	FloorMesh->SetVisibility(false, true);
	FloorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 내비 카빙: 혹시 남는 내비 폴리곤까지 확실히 막는다 (클릭 이동 차단)
	UNavModifierComponent* NavModifier = NewObject<UNavModifierComponent>(this, TEXT("DestroyedNavModifier"));
	NavModifier->SetAreaClass(UNavArea_Null::StaticClass());
	NavModifier->RegisterComponent();

	// 파괴 연출 (BP)
	OnSliceDestroyedFX();
}
