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
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
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

	UE_LOG(LogTemp, Warning, TEXT("[Charge] StartEncounter 진입 (조우 배분 시작)"));

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

	// 부여/복제 정착 후 +/- 인원 균형 보정 (즉시 조회는 타이밍상 불안정 -> 짧게 늦춰 실행)
	ScheduleChargeRebalance();

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
	GetWorldTimerManager().ClearTimer(ChargeRebalanceTimer);

	bEncounterStarted = false;
}

void ABossRaidGameMode::SpaceOutSpawn(APawn* Pawn)
{
	if (!Pawn || PlayerSpawnSpacingRadius <= 0.f)
	{
		return;
	}

	// 첫 플레이어의 스폰 위치를 파티 링의 중심으로 고정 (이후 전원 이 중심 기준으로 흩어짐)
	if (!bPartySpawnOriginSet)
	{
		PartySpawnOrigin = Pawn->GetActorLocation();
		bPartySpawnOriginSet = true;
	}

	// 황금각으로 슬롯 배치 -> 몇 명이든 서로 겹치지 않게 고르게 흩어진다
	const float AngleRad = FMath::DegreesToRadians(SpawnSlotIndex * 137.5f);
	const FVector Offset(FMath::Cos(AngleRad) * PlayerSpawnSpacingRadius,
		FMath::Sin(AngleRad) * PlayerSpawnSpacingRadius, 0.f);
	++SpawnSlotIndex;

	// 중심의 지면 높이를 유지(튕겨 오른 Z 무시)하고 링 위 슬롯으로 순간이동 + 튕김 속도 제거
	FVector Target = PartySpawnOrigin + Offset;
	Target.Z = PartySpawnOrigin.Z;
	Pawn->TeleportTo(Target, Pawn->GetActorRotation());

	if (UPawnMovementComponent* Move = Pawn->GetMovementComponent())
	{
		Move->StopMovementImmediately();	// 캡슐 겹침으로 실린 튕김 속도 제거
	}
}

bool ABossRaidGameMode::ShouldGiveRedCharge(const APawn* Pawn) const
{
	int32 PlayerId = 0;
	if (Pawn)
	{
		if (const AController* C = Pawn->GetController())
		{
			if (const APlayerState* PS = C->PlayerState)
			{
				PlayerId = PS->GetPlayerId();
			}
		}
	}
	// 짝수 PlayerId = 빨강(+), 홀수 = 파랑(-). 접속 순서대로 연속 부여되는 값이라 교대 -> 균형.
	return (PlayerId % 2) == 0;
}

void ABossRaidGameMode::AssignRandomCharges()
{
	TArray<APawn*> PlayerPawns;
	UBossCombatStatics::GetPlayerPawns(GetWorld(), PlayerPawns);
	for (APawn* Pawn : PlayerPawns)
	{
		AssignBalancedChargeTo(Pawn);
	}
}

void ABossRaidGameMode::AssignBalancedChargeTo(APawn* Pawn)
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

	// 타인 상태를 읽지 않고 이 플레이어 자신의 PlayerId 홀짝으로 임시 결정 (최종 균형은 RebalanceCharges).
	// 전하는 GE 가 아니라 '복제 루스 태그'로 부여 -> 애셋 불필요 + 서버/클라 즉시 조회 가능.
	const bool bGiveRed = ShouldGiveRedCharge(Pawn);
	UBossCombatStatics::AddReplicatedLooseTag(ASC,
		bGiveRed ? LostArkTags::State_Charge_Red.GetTag() : LostArkTags::State_Charge_Blue.GetTag());

	// 진단: 방금 부여한 태그가 같은 ASC 에서 즉시 조회되는지 (루스 태그라 서버에서 즉시 true 여야 정상)
	const bool bVerifyRed = ASC->HasMatchingGameplayTag(LostArkTags::State_Charge_Red);
	const bool bVerifyBlue = ASC->HasMatchingGameplayTag(LostArkTags::State_Charge_Blue);

	int32 PlayerId = -1;
	if (const AController* C = Pawn->GetController())
	{
		if (const APlayerState* PS = C->PlayerState)
		{
			PlayerId = PS->GetPlayerId();
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[Charge] 부여 %s => %s (PlayerId=%d ASC=%p 즉시확인 R=%d B=%d)"),
		*GetNameSafe(Pawn), bGiveRed ? TEXT("RED(+)") : TEXT("BLUE(-)"), PlayerId, ASC,
		bVerifyRed ? 1 : 0, bVerifyBlue ? 1 : 0);
}

void ABossRaidGameMode::ScheduleChargeRebalance()
{
	// 재호출 시 기존 예약을 덮어써(디바운스) 마지막 접속 기준으로 한 번만 실행되게 한다.
	GetWorldTimerManager().SetTimer(
		ChargeRebalanceTimer, this, &ABossRaidGameMode::RebalanceCharges,
		FMath::Max(ChargeRebalanceDelay, 0.05f), false);
}

void ABossRaidGameMode::RebalanceCharges()
{
	TArray<APawn*> PlayerPawns;
	UBossCombatStatics::GetPlayerPawns(GetWorld(), PlayerPawns);

	UE_LOG(LogTemp, Warning, TEXT("[Charge] Rebalance 시작: World=%s GetPlayerPawns=%d"),
		*GetNameSafe(GetWorld()), PlayerPawns.Num());

	// 정착된 전하(타이머 실행이라 신뢰 가능)를 편으로 모은다
	TArray<UAbilitySystemComponent*> Reds;
	TArray<UAbilitySystemComponent*> Blues;
	for (APawn* Pawn : PlayerPawns)
	{
		UAbilitySystemComponent* ASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn);
		const bool bR = ASC && ASC->HasMatchingGameplayTag(LostArkTags::State_Charge_Red);
		const bool bB = ASC && ASC->HasMatchingGameplayTag(LostArkTags::State_Charge_Blue);
		UE_LOG(LogTemp, Warning, TEXT("  [Charge] pawn=%s ASC=%p red=%d blue=%d"),
			*GetNameSafe(Pawn), ASC, bR ? 1 : 0, bB ? 1 : 0);
		if (!ASC)
		{
			continue;
		}
		if (bR)
		{
			Reds.Add(ASC);
		}
		else if (bB)
		{
			Blues.Add(ASC);
		}
	}

	// 많은 쪽에서 (차이/2) 명을 반대로 뒤집으면 |R-B| <= 1
	const int32 Diff = Reds.Num() - Blues.Num();
	const int32 FlipCount = FMath::Abs(Diff) / 2;
	TArray<UAbilitySystemComponent*>& From = (Diff > 0) ? Reds : Blues;
	for (int32 i = 0; i < FlipCount && From.Num() > 0; ++i)
	{
		UAbilitySystemComponent* ASC = From.Pop(/*bAllowShrinking=*/false);
		UBossCombatStatics::FlipChargeLoose(ASC,
			LostArkTags::State_Charge_Red.GetTag(), LostArkTags::State_Charge_Blue.GetTag());
	}

	UE_LOG(LogTemp, Warning, TEXT("[Charge] Rebalance: 반전전 R=%d B=%d, %d명 반전 => 균형"),
		Reds.Num(), Blues.Num(), FlipCount);
}

void ABossRaidGameMode::SetupRaidComponentsForPlayers()
{
	ABossBase* Boss = FindBoss();

	TArray<APawn*> PlayerPawns;
	UBossCombatStatics::GetPlayerPawns(GetWorld(), PlayerPawns);
	for (APawn* Pawn : PlayerPawns)
	{
		SetupRaidComponentsForPawn(Pawn, Boss);
	}
}

void ABossRaidGameMode::SetupRaidComponentsForPawn(APawn* Pawn, ABossBase* Boss)
{
	if (!Pawn)
	{
		return;
	}

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

void ABossRaidGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	// 조우가 아직 시작 전이면 StartEncounter 가 일괄 처리하므로 여기선 건드리지 않는다.
	// 조우 시작 뒤 늦게 possess 된 플레이어(원격 클라 join 지연 등)만 즉시 세팅해 누락을 메운다.
	APawn* Pawn = NewPlayer ? NewPlayer->GetPawn() : nullptr;

	// 스폰 분산은 조우 여부와 무관하게 모든 플레이어에 적용 (한 점 겹침 튕김 방지)
	if (Pawn)
	{
		SpaceOutSpawn(Pawn);
	}

	UE_LOG(LogTemp, Warning, TEXT("[Charge] HandleStartingNewPlayer: PC=%s bEncounterStarted=%d Pawn=%s"),
		*GetNameSafe(NewPlayer), bEncounterStarted ? 1 : 0, *GetNameSafe(Pawn));

	if (!bEncounterStarted || !Pawn)
	{
		return;	// 조우 전이면 StartEncounter 가 일괄 처리 / 폰 없으면 다음 possess 시 재호출
	}

	AssignBalancedChargeTo(Pawn);
	SetupRaidComponentsForPawn(Pawn, FindBoss());

	// 늦게 들어온 인원까지 포함해 +/- 균형 재보정 (디바운스 -> 마지막 접속 기준 1회)
	ScheduleChargeRebalance();

	// 조우 시작 시의 일괄 SetViewTargetForAll 을 놓친 늦은 접속자에게도 아레나 카메라 지정.
	// (아레나 카메라가 살아있으면 = 조우 진행 중. 클리어 시퀀스로 넘어가면 EndEncounter 가 정리해 null)
	if (ArenaCamera)
	{
		NewPlayer->SetViewTargetWithBlend(ArenaCamera, CameraBlendTime, VTBlend_Cubic);
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
