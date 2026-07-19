// Fill out your copyright notice in the Description page of Project Settings.


#include "Boss/Raid/BossRaidGameMode.h"
#include "Boss/Raid/BossRaidGameState.h"
#include "Boss/Raid/BossArenaCamera.h"
#include "Boss/Raid/BossChargeGaugeComponent.h"
#include "Boss/Raid/BossReviveComponent.h"
#include "Monster/ArenaSliceActor.h"
#include "Boss/BossBase.h"
#include "Boss/BossGameplayTags.h"
#include "Boss/Combat/BossCombatStatics.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ABossRaidGameMode::ABossRaidGameMode()
{
	GameStateClass = ABossRaidGameState::StaticClass();
	ChargeGaugeComponentClass = UBossChargeGaugeComponent::StaticClass();
	ReviveComponentClass = UBossReviveComponent::StaticClass();
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

void ABossRaidGameMode::SetViewTargetForAll(AActor* NewViewTarget, float BlendTime)
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		// 서버에서 지정 -> 원격 PC에는 ClientSetViewTarget 으로 전파됨
		AActor* Target = NewViewTarget ? NewViewTarget : static_cast<AActor*>(PC->GetPawn());
		if (Target)
		{
			PC->SetViewTargetWithBlend(Target, BlendTime, VTBlend_Cubic);
		}
	}
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

	// 조우 시 전하 랜덤 부여 + 전하 게이지/부활 컴포넌트 부착
	AssignRandomCharges();
	SetupRaidComponentsForPlayers();

	// 전하 공명 주기 시작 (균등이면 상쇄라 자동 무피해 -> 켜둬도 안전)
	if (ResonanceDamageEffect && ChargeResonanceInterval > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			ResonanceTimer, this, &ABossRaidGameMode::ApplyChargeResonancePulse,
			ChargeResonanceInterval, true);
	}

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
			SetViewTargetForAll(ArenaCamera, CameraBlendTime);
		}
	}
}

void ABossRaidGameMode::EndEncounter()
{
	SetViewTargetForAll(/*NewViewTarget=각자 자기 폰*/nullptr, CameraBlendTime);

	if (ArenaCamera)
	{
		ArenaCamera->Destroy();
		ArenaCamera = nullptr;
	}

	GetWorldTimerManager().ClearTimer(ResonanceTimer);

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
	UBossCombatStatics::ApplyEffectToSelf(ASC, ChargeGE, this);
}

void ABossRaidGameMode::SetupRaidComponentsForPlayers()
{
	ABossBase* Boss = FindBoss();

	TArray<APawn*> PlayerPawns;
	UBossCombatStatics::GetPlayerPawns(GetWorld(), PlayerPawns);
	for (APawn* Pawn : PlayerPawns)
	{
		// 전하 게이지 (충전 패턴 피격 -> 절반 시 전하 반전 / 가득 시 과충전 폭발)
		if (ChargeGaugeComponentClass && !Pawn->FindComponentByClass<UBossChargeGaugeComponent>())
		{
			UBossChargeGaugeComponent* Gauge =
				NewObject<UBossChargeGaugeComponent>(Pawn, ChargeGaugeComponentClass, TEXT("BossChargeGauge"));
			Gauge->InitChargeGauge(Boss, RedChargeEffect, BlueChargeEffect);
			Gauge->RegisterComponent();
		}

		// 부활 (30초 자동 / 지정 시간 후 클릭 부활)
		if (ReviveComponentClass && !Pawn->FindComponentByClass<UBossReviveComponent>())
		{
			UBossReviveComponent* Revive =
				NewObject<UBossReviveComponent>(Pawn, ReviveComponentClass, TEXT("BossRevive"));
			Revive->RegisterComponent();
		}
	}
}

void ABossRaidGameMode::ApplyChargeResonancePulse()
{
	if (!ResonanceDamageEffect)
	{
		return;
	}

	// 생존자의 전하 인원수 집계
	TArray<APawn*> PlayerPawns;
	UBossCombatStatics::GetPlayerPawns(GetWorld(), PlayerPawns);

	TArray<UAbilitySystemComponent*> AliveASCs;
	int32 RedCount = 0;
	int32 BlueCount = 0;
	for (APawn* Pawn : PlayerPawns)
	{
		if (!UBossCombatStatics::IsAliveActor(Pawn, LostArkTags::State_Dead.GetTag()))
		{
			continue;
		}
		UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn);
		if (!ASC)
		{
			continue;
		}
		AliveASCs.Add(ASC);
		if (ASC->HasMatchingGameplayTag(LostArkTags::State_Charge_Red))
		{
			++RedCount;
		}
		else if (ASC->HasMatchingGameplayTag(LostArkTags::State_Charge_Blue))
		{
			++BlueCount;
		}
	}

	// 균등(4:4)이면 상쇄 -> 무피해. 어긋난 만큼(3:5 -> 2, 0:8 -> 8) 전원이 아프다
	const int32 Imbalance = FMath::Abs(RedCount - BlueCount);
	if (Imbalance == 0)
	{
		return;
	}
	const float Magnitude = Imbalance * ResonanceDamagePerImbalance;

	for (UAbilitySystemComponent* ASC : AliveASCs)
	{
		UBossCombatStatics::ApplyEffectToSelf(ASC, ResonanceDamageEffect, this,
			LostArkTags::Data_Damage.GetTag(), Magnitude);
	}
}

void ABossRaidGameMode::NotifyBossDied(ABossBase* Boss)
{
	if (bBossDied)
	{
		return;
	}
	bBossDied = true;

	// 시체에 전하 공명 딜이 계속 들어가지 않게 즉시 정지
	GetWorldTimerManager().ClearTimer(ResonanceTimer);

	// 1) 클리어 카메라: 보스 정면에서 내려다보는 고정 샷.
	//    복제로 스폰해야 원격 클라의 ClientSetViewTarget 이 같은 액터를 찾는다.
	if (Boss)
	{
		const FVector BossLoc = Boss->GetActorLocation();
		FVector Forward = Boss->GetActorForwardVector();
		Forward.Z = 0.f;
		if (!Forward.Normalize())
		{
			Forward = FVector::ForwardVector;
		}

		const FVector CamLoc = BossLoc + Forward * ClearCameraDistance + FVector(0.f, 0.f, ClearCameraHeight);
		const FVector Focus = BossLoc + FVector(0.f, 0.f, ClearCameraFocusHeight);
		const FTransform CamTM((Focus - CamLoc).Rotation(), CamLoc);

		ClearCamera = GetWorld()->SpawnActorDeferred<ACameraActor>(ACameraActor::StaticClass(), CamTM);
		if (ClearCamera)
		{
			ClearCamera->SetReplicates(true);
			if (UCameraComponent* Cam = ClearCamera->GetCameraComponent())
			{
				Cam->bConstrainAspectRatio = false;	// 레터박스(위아래 검은 띠) 방지
			}
			ClearCamera->FinishSpawning(CamTM);
			SetViewTargetForAll(ClearCamera, ClearCameraBlendTime);
		}
	}

	// 2) 글로벌 슬로모 (WorldSettings.TimeDilation 은 복제 -> 전 머신 동시 슬로모)
	if (ClearSlomoDilation < 1.f - KINDA_SMALL_NUMBER && ClearSlomoDuration > 0.f)
	{
		UGameplayStatics::SetGlobalTimeDilation(this, ClearSlomoDilation);

		// 월드 타이머는 딜레이션 영향을 받으므로 '실제 ClearSlomoDuration초' 후 복구되도록 환산
		GetWorldTimerManager().SetTimer(
			ClearSlomoTimer, this, &ABossRaidGameMode::RestoreTimeDilation,
			FMath::Max(ClearSlomoDuration * ClearSlomoDilation, 0.05f), false);
	}

	// 3) 배너 -> 종료 예약 (게임시간 기준. 슬로모 구간만큼 실제로는 살짝 늦게 온다 — 의도된 여유)
	GetWorldTimerManager().SetTimer(
		ClearBannerTimer, this, &ABossRaidGameMode::ShowClearBanner,
		FMath::Max(ClearBannerDelay, 0.05f), false);
	GetWorldTimerManager().SetTimer(
		ClearEndTimer, this, &ABossRaidGameMode::FinishClearSequence,
		FMath::Max(ClearBannerDelay + ClearHoldTime, 0.1f), false);
}

void ABossRaidGameMode::ShowClearBanner()
{
	if (ABossRaidGameState* GS = GetGameState<ABossRaidGameState>())
	{
		GS->MarkRaidCleared();	// 복제 -> 전 머신에서 OnRaidCleared 방송 + 배너 표시
	}
}

void ABossRaidGameMode::RestoreTimeDilation()
{
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
}

void ABossRaidGameMode::FinishClearSequence()
{
	RestoreTimeDilation();	// 어떤 경로로 오든 슬로모가 남지 않게 보증

	// 카메라 각자 캐릭터 복귀 + 아레나 카메라/공명 타이머 정리 (기존 종료 루틴 재사용)
	EndEncounter();

	// 뷰타겟이 폰으로 넘어갔으니 클리어 카메라 정리
	// (블렌드 중 파괴돼도 카메라 매니저가 폰 폴백 처리 — ArenaCamera 와 동일 관용구)
	if (ClearCamera)
	{
		ClearCamera->Destroy();
		ClearCamera = nullptr;
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
