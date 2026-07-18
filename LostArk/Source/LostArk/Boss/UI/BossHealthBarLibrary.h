// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BossHealthBarLibrary.generated.h"

/**
 * 보스 체력바('줄' 막대) 표시용 순수 계산 함수 모음.
 *
 * 로스트아크식 표기: 보스 체력이 매우 크므로 전체를 T개의 '줄'로 나눠서 본다.
 *  - 퍼센트  = (현재/최대)            : 실제 체력 비율
 *  - xN줄    = ceil(비율 * T)         : 남은 줄 수 (지금 깎이는 줄 포함). 100% -> xT, 50% -> xT/2
 *  - 바 채움 = 지금 깎이는 '한 줄'의 0~1 : 줄 경계마다 1.0으로 리필(루프),
 *                                        마지막 1줄에서만 0(검정)까지 빠진다.
 *
 * 위젯은 (NewHealth, MaxHealth)만 알면 세 값을 모두 여기서 뽑을 수 있다(보스 포인터 불필요).
 */
UCLASS()
class LOSTARK_API UBossHealthBarLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 0~1 체력 비율 (현재/최대). Max<=0 이면 0. 퍼센트 텍스트는 여기에 *100. */
	UFUNCTION(BlueprintPure, Category = "Boss|HealthBar")
	static float GetHealthFraction(float Health, float MaxHealth);

	/** 남은 '줄' 수 (xN 표기). 예) 50% * 500줄 = x250. 아직 살아있으면 최소 1, 0이면 사망. */
	UFUNCTION(BlueprintPure, Category = "Boss|HealthBar")
	static int32 GetBarsRemaining(float Health, float MaxHealth, int32 TotalBars);

	/**
	 * 지금 깎이는 '한 줄'의 채움 0~1.
	 * 줄이 끝나면 다음 줄로 넘어가며 1.0으로 리필되고, 마지막 1줄에서만 0(검정)까지 빠진다.
	 * (위젯에서 이 값을 목표로 매 프레임 보간하면 '피가 닳는' 연출이 된다.)
	 */
	UFUNCTION(BlueprintPure, Category = "Boss|HealthBar")
	static float GetCurrentBarFill(float Health, float MaxHealth, int32 TotalBars);
};
