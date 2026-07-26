// Fill out your copyright notice in the Description page of Project Settings.


#include "Boss/Raid/BossRaidGameState.h"
#include "Blueprint/UserWidget.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "GameFramework/PlayerState.h"

void ABossRaidGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABossRaidGameState, DestroyedSliceMask);
	DOREPLIFETIME(ABossRaidGameState, SliceCount);
	DOREPLIFETIME(ABossRaidGameState, ArenaCenter);
	DOREPLIFETIME(ABossRaidGameState, ArenaFloorZ);
	DOREPLIFETIME(ABossRaidGameState, bRaidCleared);
	DOREPLIFETIME(ABossRaidGameState, RaidBgm);
	DOREPLIFETIME(ABossRaidGameState, PlayerDamageList);
}

void ABossRaidGameState::AddPlayerDamage(APlayerState* PlayerState, float Damage)
{
	if (!HasAuthority() || !PlayerState) return;

	for (FPlayerDamageInfo& Info : PlayerDamageList)
	{
		if (Info.PlayerState == PlayerState)
		{
			Info.DamageDealt += Damage;
			ForceNetUpdate();
			return;
		}
	}

	PlayerDamageList.Add(FPlayerDamageInfo(PlayerState, Damage));
	ForceNetUpdate();
}

bool ABossRaidGameState::IsSliceDestroyed(int32 SliceIndex) const
{
	if (SliceIndex < 0 || SliceIndex >= SliceCount)
	{
		return false;
	}
	return (DestroyedSliceMask & (1 << SliceIndex)) != 0;
}

int32 ABossRaidGameState::GetSliceIndexAt(const FVector& WorldLocation) const
{
	if (SliceCount <= 0)
	{
		return INDEX_NONE;
	}

	const FVector Local = WorldLocation - ArenaCenter;
	float Angle = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));	// [-180, 180]
	if (Angle < 0.f)
	{
		Angle += 360.f;	// [0, 360)
	}

	const float Step = 360.f / SliceCount;
	return FMath::Clamp(FMath::FloorToInt32(Angle / Step), 0, SliceCount - 1);
}

FVector ABossRaidGameState::GetSliceCenterLocation(int32 SliceIndex, float Radius) const
{
	if (SliceCount <= 0 || SliceIndex < 0 || SliceIndex >= SliceCount)
	{
		return ArenaCenter;
	}

	// GetSliceIndexAt 과 동일 규약: 슬라이스 N = [N*Step, (N+1)*Step) 도. 중심각 = (N+0.5)*Step.
	const float Step = 360.f / SliceCount;
	const float CenterAngleRad = FMath::DegreesToRadians((SliceIndex + 0.5f) * Step);
	FVector Loc = ArenaCenter +
		FVector(FMath::Cos(CenterAngleRad), FMath::Sin(CenterAngleRad), 0.f) * Radius;
	Loc.Z = !FMath::IsNearlyZero(ArenaFloorZ) ? ArenaFloorZ : ArenaCenter.Z;
	return Loc;
}

void ABossRaidGameState::MarkSliceDestroyed(int32 SliceIndex)
{
	if (!HasAuthority() || SliceIndex < 0 || SliceIndex >= SliceCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Arena] MarkSliceDestroyed 거부: Slice=%d SliceCount=%d Auth=%d (범위 밖이면 SliceCount 확인)"),
			SliceIndex, SliceCount, HasAuthority() ? 1 : 0);
		return;
	}
	if (IsSliceDestroyed(SliceIndex))
	{
		return;
	}

	DestroyedSliceMask |= (1 << SliceIndex);
	UE_LOG(LogTemp, Warning, TEXT("[Arena] MarkSliceDestroyed OK: Slice=%d Mask=%d -> OnArenaSlicesChanged 방송"),
		SliceIndex, DestroyedSliceMask);
	ForceNetUpdate();

	// OnRep 은 서버에서 안 불리므로 서버도 직접 방송
	OnArenaSlicesChanged.Broadcast();
}

void ABossRaidGameState::OnRep_DestroyedSliceMask()
{
	OnArenaSlicesChanged.Broadcast();
}

void ABossRaidGameState::MarkRaidCleared()
{
	if (!HasAuthority() || bRaidCleared)
	{
		return;
	}
	bRaidCleared = true;
	ForceNetUpdate();

	// OnRep 은 서버에서 안 불리므로 서버(리슨 호스트 포함)도 직접 처리
	HandleRaidCleared();
}

void ABossRaidGameState::OnRep_RaidCleared()
{
	if (bRaidCleared)
	{
		HandleRaidCleared();
	}
}

void ABossRaidGameState::HandleRaidCleared()
{
	OnRaidCleared.Broadcast();
	ShowClearBannerLocally();
}

void ABossRaidGameState::SetRaidBgm(USoundBase* InBgm)
{
	if (!HasAuthority() || !InBgm)
	{
		return;
	}
	RaidBgm = InBgm;
	ForceNetUpdate();

	// OnRep 은 서버에서 안 불리므로 리슨 호스트도 직접 재생 (데디 서버면 PlayBgmLocally 가 알아서 스킵)
	PlayBgmLocally();
}

void ABossRaidGameState::OnRep_RaidBgm()
{
	PlayBgmLocally();
}

void ABossRaidGameState::PlayBgmLocally()
{
	// 데디 서버는 들을 로컬 플레이어가 없음 / 이미 재생했으면 스킵 (재복제로 인한 중복 방지)
	if (bBgmStarted || !RaidBgm || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	bBgmStarted = true;

	// 핸들 보관 -> 추후 페이즈/클리어 시 정지·전환 가능. Looping 은 SoundWave 에셋에서 설정.
	BgmComponent = UGameplayStatics::SpawnSound2D(this, RaidBgm);
}

void ABossRaidGameState::ShowClearBannerLocally()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 이 머신의 로컬 플레이어에게만 배너 (데디 서버는 로컬 PC 없음 -> 자연스럽게 스킵)
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || !PC->IsLocalController())
		{
			continue;
		}

		if (ClearWidgetClass)
		{
			if (UUserWidget* Widget = CreateWidget<UUserWidget>(PC, ClearWidgetClass))
			{
				Widget->AddToViewport(/*ZOrder=*/100);	// 다른 HUD 위에
				ActiveClearWidgets.Add(Widget);
			}
		}
		else if (GEngine)
		{
			// WBP 없이도 흐름을 테스트할 수 있는 폴백
			GEngine->AddOnScreenDebugMessage(-1, FMath::Max(ClearWidgetLifetime, 3.f), FColor::Yellow,
				TEXT("== RAID CLEAR == (GameState BP 의 ClearWidgetClass 미지정 폴백)"));
		}
	}

	if (ActiveClearWidgets.Num() > 0 && ClearWidgetLifetime > 0.f)
	{
		World->GetTimerManager().SetTimer(
			ClearWidgetTimer, this, &ABossRaidGameState::RemoveClearWidgets,
			ClearWidgetLifetime, false);
	}
}

void ABossRaidGameState::RemoveClearWidgets()
{
	for (UUserWidget* Widget : ActiveClearWidgets)
	{
		if (Widget)
		{
			Widget->RemoveFromParent();
		}
	}
	ActiveClearWidgets.Empty();

	ShowMvpWindowLocally();
}

void ABossRaidGameState::ShowMvpWindowLocally()
{
	UWorld* World = GetWorld();
	if (!World) return;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || !PC->IsLocalController()) continue;

		if (MvpWidgetClass)
		{
			if (UUserWidget* Widget = CreateWidget<UUserWidget>(PC, MvpWidgetClass))
			{
				Widget->AddToViewport(100);
				PC->SetShowMouseCursor(true);
				
				FInputModeUIOnly InputMode;
				InputMode.SetWidgetToFocus(Widget->TakeWidget());
				PC->SetInputMode(InputMode);
			}
		}
	}
}
