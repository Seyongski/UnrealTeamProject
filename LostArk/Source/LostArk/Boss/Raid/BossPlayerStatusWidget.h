// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BossPlayerStatusWidget.generated.h"

class UBossChargeGaugeComponent;

/**
 * 플레이어 머리 위 전하 아이콘 / 발밑 게이지 위젯의 C++ 베이스.
 * WBP_ChargeIcon, WBP_ChargeGauge 를 이 클래스로 '부모 재지정(Reparent)'해서 쓴다.
 *
 * WidgetComponent 안에서 만들어진 위젯은 GetOwningPlayerPawn 으로 '자기가 붙은 폰'을 못 찾는다
 * (그건 로컬 플레이어를 준다). 그래서 UBossChargeGaugeComponent 가 위젯 생성 직후 여기에
 * 자기 자신을 주입(InitStatus)하고, OnStatusInitialized 이벤트로 알린다.
 *
 * WBP 사용법:
 *  - 부모를 이 클래스로 재지정
 *  - 그래프에서 OnStatusInitialized 이벤트를 오버라이드 -> ChargeComponent 의 델리게이트
 *    (OnChargeSideChanged / OnChargeGaugeChanged)에 바인딩 + 현재값으로 1회 초기화
 */
UCLASS(Abstract)
class LOSTARK_API UBossPlayerStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 컴포넌트가 위젯 생성 직후 호출. 포인터 주입 후 BP 이벤트 방송 */
	void InitStatus(UBossChargeGaugeComponent* InChargeComponent);

	/** 이 위젯이 표시하는 대상 플레이어의 전하 게이지 컴포넌트 (InitStatus 이후 유효) */
	UPROPERTY(BlueprintReadOnly, Category = "Boss|Status")
	TObjectPtr<UBossChargeGaugeComponent> ChargeComponent;

protected:
	/** 초기화 완료 (ChargeComponent 세팅됨). WBP 가 여기서 델리게이트 바인딩 + 초기값 반영 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|Status")
	void OnStatusInitialized();
};
