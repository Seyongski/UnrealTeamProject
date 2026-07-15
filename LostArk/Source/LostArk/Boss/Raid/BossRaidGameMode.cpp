// Fill out your copyright notice in the Description page of Project Settings.


#include "Boss/Raid/BossRaidGameMode.h"
#include "Boss/Raid/BossRaidGameState.h"
#include "Boss/Raid/BossArenaCamera.h"
#include "Monster/ArenaSliceActor.h"
#include "Boss/BossBase.h"
#include "Boss/BossGameplayTags.h"
#include "Boss/Combat/BossCombatStatics.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ABossRaidGameMode::ABossRaidGameMode()
{
	GameStateClass = ABossRaidGameState::StaticClass();
}

void ABossRaidGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 테스트 편의: 폰 possess 이후 자동 조우 시작
	if (bAutoStartOnBeginPlay)
	{
		FTimerHandle Tmp;
		GetWorldTimerManager().SetTimer(
			Tmp, this, &ABossRaidGameMode::StartEncounter,
			FMath::Max(AutoStartDelay, 0.01f), false);
	}
}

ABossBase* ABossRaidGameMode::FindBoss() const
{
	for (TActorIterator<ABossBase> It(GetWorld()); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ABossRaidGameMode::StartEncounter()
{
	if (bEncounterStarted)
	{
		return;
	}
	bEncounterStarted = true;

	ABossBase* Boss = FindBoss();

	// 아레나 중심: 슬라이스 액터(관례: 액터 위치=중심) > 보스 위치 순으로 결정
	if (ABossRaidGameState* GS = GetGameState<ABossRaidGameState>())
	{
		FVector Center = GS->ArenaCenter;
		bool bFound = false;
		for (TActorIterator<AArenaSliceActor> It(GetWorld()); It; ++It)
		{
			Center = It->GetActorLocation();
			bFound = true;
			break;
		}
		if (!bFound && Boss)
		{
			Center = Boss->GetActorLocation();
		}
		GS->ArenaCenter = Center;
	}

	// 조우 시 전하 랜덤 부여
	AssignRandomCharges();

	// 보스 레벨 전용 카메라로 전환 (카메라-플레이어-보스 일직선)
	if (ArenaCameraClass)
	{
		const FVector SpawnLoc = Boss ? Boss->GetActorLocation() + FVector(0, 0, 500.f) : FVector(0, 0, 500.f);
		ArenaCamera = GetWorld()->SpawnActorDeferred<ABossArenaCamera>(
			ArenaCameraClass, FTransform(SpawnLoc));
		if (ArenaCamera)
		{
			ArenaCamera->SetBoss(Boss);
			ArenaCamera->FinishSpawning(FTransform(SpawnLoc));

			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				if (APlayerController* PC = It->Get())
				{
					// 서버에서 지정 -> 원격 PC에는 ClientSetViewTarget 으로 전파됨
					PC->SetViewTargetWithBlend(ArenaCamera, CameraBlendTime, VTBlend_Cubic);
				}
			}
		}
	}
}

void ABossRaidGameMode::EndEncounter()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->GetPawn())
		{
			PC->SetViewTargetWithBlend(PC->GetPawn(), CameraBlendTime, VTBlend_Cubic);
		}
	}

	if (ArenaCamera)
	{
		ArenaCamera->Destroy();
		ArenaCamera = nullptr;
	}

	bEncounterStarted = false;
}

void ABossRaidGameMode::AssignRandomCharges()
{
	TArray<APawn*> PlayerPawns;
	UBossCombatStatics::GetPlayerPawns(GetWorld(), PlayerPawns);
	for (APawn* Pawn : PlayerPawns)
	{
		ApplyChargeTo(Pawn);
	}
}

void ABossRaidGameMode::ApplyChargeTo(APawn* Pawn)
{
	UAbilitySystemComponent* ASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn);
	if (!ASC)
	{
		return;	// 플레이어 ASC 미구성 (팀원 파트) -> 조용히 스킵
	}

	// 이미 전하 보유 시 유지
	if (ASC->HasMatchingGameplayTag(LostArkTags::State_Charge_Red) ||
		ASC->HasMatchingGameplayTag(LostArkTags::State_Charge_Blue))
	{
		return;
	}

	const TSubclassOf<UGameplayEffect> ChargeGE = FMath::RandBool() ? RedChargeEffect : BlueChargeEffect;
	if (!ChargeGE)
	{
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(ChargeGE, 1.f, Context);
	if (Spec.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	}
}

void ABossRaidGameMode::DestroySlice(int32 SliceIndex)
{
	ABossRaidGameState* GS = GetGameState<ABossRaidGameState>();
	if (!GS)
	{
		return;
	}

	GS->MarkSliceDestroyed(SliceIndex);

	// 첫 지형 파괴 -> 약점포착: 어디서 때려도 백/헤드어택 보너스
	// (데미지 계산에서 방향 검사 || 이 태그 보유 로 체크하기로 합의)
	if (ABossBase* Boss = FindBoss())
	{
		if (UAbilitySystemComponent* BossASC = Boss->GetAbilitySystemComponent())
		{
			const FGameplayTag WeakTag = LostArkTags::State_Boss_WeakPointExposed;
			if (!BossASC->HasMatchingGameplayTag(WeakTag))
			{
				UBossCombatStatics::AddReplicatedLooseTag(BossASC, WeakTag);	// 클라 UI 표시용 복제 포함
			}
		}
	}
}
