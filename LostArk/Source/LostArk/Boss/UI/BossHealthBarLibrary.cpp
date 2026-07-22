// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/UI/BossHealthBarLibrary.h"

float UBossHealthBarLibrary::GetHealthFraction(float Health, float MaxHealth)
{
	return (MaxHealth > 0.f) ? FMath::Clamp(Health / MaxHealth, 0.f, 1.f) : 0.f;
}

int32 UBossHealthBarLibrary::GetBarsRemaining(float Health, float MaxHealth, int32 TotalBars)
{
	TotalBars = FMath::Max(TotalBars, 1);

	const float Frac = GetHealthFraction(Health, MaxHealth);
	if (Frac <= 0.f)
	{
		return 0; // 사망
	}

	// 지금 깎이는 줄까지 포함해서 셈 -> 아직 살아있으면 최소 1줄.
	const float BarsFloat = Frac * static_cast<float>(TotalBars);
	return FMath::Clamp(FMath::CeilToInt(BarsFloat), 1, TotalBars);
}

float UBossHealthBarLibrary::GetCurrentBarFill(float Health, float MaxHealth, int32 TotalBars)
{
	TotalBars = FMath::Max(TotalBars, 1);

	const float Frac = GetHealthFraction(Health, MaxHealth);
	if (Frac <= 0.f)
	{
		return 0.f; // 마지막 줄까지 다 깎임 -> 검정
	}

	const float BarsFloat = Frac * static_cast<float>(TotalBars);
	const int32 BarsRemaining = FMath::CeilToInt(BarsFloat); // 지금 깎이는 줄 포함한 남은 줄 수

	// 현재 줄의 채움 = 전체 줄값에서 '이미 지나간 줄(BarsRemaining-1)'을 뺀 나머지.
	// 정수 경계에서는 1.0(가득) -> 바로 아래로 내려가며 감소 -> 0 근처에서 다음 줄로 리필.
	const float Fill = BarsFloat - static_cast<float>(BarsRemaining - 1);
	return FMath::Clamp(Fill, 0.f, 1.f);
}
