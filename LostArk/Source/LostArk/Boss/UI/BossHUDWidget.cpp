// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/UI/BossHUDWidget.h"
#include "Boss/BossBase.h"
#include "Boss/Raid/BossRaidGameState.h"
#include "Engine/Engine.h" // [임시 디버그] 온스크린 메시지용 — 검증 끝나면 제거

void UBossHUDWidget::InitForBoss(ABossBase* InBoss)
{
	if (!InBoss)
	{
		return;
	}

	Boss = InBoss;

	// 체력 변동 방송 구독 (서버/클라 공통). 중복 바인딩은 AddUnique 성격의 델리게이트가 아니므로
	// InitForBoss 는 1회만 호출되는 계약(컨트롤러가 위젯 생성 직후 1회 호출).
	Boss->OnBossHealthChanged.AddDynamic(this, &UBossHUDWidget::HandleBossHealthChanged);

	// 현재값으로 최초 1회 반영 (첫 피격 전까지는 변동 방송이 없으므로 초기값을 직접 그린다)
	OnBossHealthUpdated(Boss->GetCurrentHealth(), Boss->GetMaxHealthValue());

	// [임시 디버그] 바인딩/초기값 확인. 검증 끝나면 이 블록 삭제.
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(2001, 5.f, FColor::Cyan,
			FString::Printf(TEXT("[BossHUD] InitForBoss: H=%.0f Max=%.0f"), Boss->GetCurrentHealth(), Boss->GetMaxHealthValue()));
	}

	// 클리어(사망) 시 숨김
	if (ABossRaidGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABossRaidGameState>() : nullptr)
	{
		if (GS->IsRaidCleared())
		{
			// 이미 클리어된 상태로 늦게 만들어졌다면 즉시 숨김
			OnRaidCleared();
		}
		else
		{
			GS->OnRaidCleared.AddDynamic(this, &UBossHUDWidget::HandleRaidCleared);
		}
	}
}

void UBossHUDWidget::NativeDestruct()
{
	if (Boss)
	{
		Boss->OnBossHealthChanged.RemoveDynamic(this, &UBossHUDWidget::HandleBossHealthChanged);
	}
	if (ABossRaidGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABossRaidGameState>() : nullptr)
	{
		GS->OnRaidCleared.RemoveDynamic(this, &UBossHUDWidget::HandleRaidCleared);
	}

	Super::NativeDestruct();
}

void UBossHUDWidget::HandleBossHealthChanged(float NewHealth, float MaxHealth)
{
	// [임시 디버그] 델리게이트가 위젯까지 도달하는지 확인. 검증 끝나면 이 블록 삭제.
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(2002, 3.f, FColor::Green,
			FString::Printf(TEXT("[BossHUD] HealthChanged: H=%.0f Max=%.0f"), NewHealth, MaxHealth));
	}

	OnBossHealthUpdated(NewHealth, MaxHealth);
}

void UBossHUDWidget::HandleRaidCleared()
{
	OnRaidCleared();
}

void UBossHUDWidget::OnRaidCleared_Implementation()
{
	// 기본: 체력바 숨김. WBP 에서 페이드아웃 등 연출로 오버라이드 가능.
	SetVisibility(ESlateVisibility::Collapsed);
}
