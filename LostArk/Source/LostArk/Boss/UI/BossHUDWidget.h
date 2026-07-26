// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BossHUDWidget.generated.h"

class ABossBase;

/**
 * 화면 상단 보스 체력 HUD(WBP_BossHUD)의 C++ 베이스.
 * WBP_BossHUD 를 이 클래스로 '부모 재지정(Reparent)'해서 쓴다.
 *
 * 위젯 생성/배치는 ALostArkPlayerController 가 담당(보스 레벨에서만). 컨트롤러가 보스를 찾아
 * InitForBoss 로 주입하면, 이 베이스가 ABossBase::OnBossHealthChanged 에 바인딩하고
 * 값이 바뀔 때마다 BP 이벤트 OnBossHealthUpdated 로 넘긴다. 실제 막대/줄수/퍼센트 표시는
 * WBP 가 UBossHealthBarLibrary 로 계산해서 그린다(보스 포인터 불필요).
 *
 * WBP 사용법:
 *  - 부모를 이 클래스로 재지정
 *  - 그래프에서 OnBossHealthUpdated(NewHealth, MaxHealth) 이벤트 구현
 *    -> UBossHealthBarLibrary(GetHealthFraction / GetBarsRemaining / GetCurrentBarFill)로 표시 갱신.
 *    줄 수(TotalBars)는 Boss->TotalHealthBars 사용.
 *  - (선택) OnRaidCleared 오버라이드로 클리어 시 페이드아웃 연출. 기본 동작은 Collapsed.
 */
UCLASS(Abstract)
class LOSTARK_API UBossHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 컨트롤러가 위젯 생성 직후 호출. 보스 포인터 주입 + OnBossHealthChanged 바인딩 +
	 * 현재값으로 1회 초기화. 클리어(사망) 시 숨김을 위해 GameState.OnRaidCleared 도 구독.
	 */
	void InitForBoss(ABossBase* InBoss);

	/** 바인딩된 보스 (InitForBoss 이후 유효). WBP 가 이름/줄수(TotalHealthBars) 참조용 */
	UPROPERTY(BlueprintReadOnly, Category = "Boss|HUD")
	TObjectPtr<ABossBase> Boss;

protected:
	virtual void NativeDestruct() override;

	/** 보스 체력 변동 콜백 (델리게이트 바인딩) -> BP 이벤트로 전달 */
	UFUNCTION()
	void HandleBossHealthChanged(float NewHealth, float MaxHealth);

	/** 레이드 클리어 콜백 (GameState.OnRaidCleared 바인딩) -> OnRaidCleared 호출 */
	UFUNCTION()
	void HandleRaidCleared();

	/**
	 * 체력이 갱신될 때마다 호출(생성 직후 초기값으로도 1회).
	 * WBP 가 여기서 UBossHealthBarLibrary 로 막대/줄수/퍼센트를 갱신한다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|HUD")
	void OnBossHealthUpdated(float NewHealth, float MaxHealth);

	/** 레이드 클리어(보스 사망) 시. 기본 구현은 위젯 숨김(Collapsed). WBP 에서 연출로 오버라이드 가능 */
	UFUNCTION(BlueprintNativeEvent, Category = "Boss|HUD")
	void OnRaidCleared();
	virtual void OnRaidCleared_Implementation();
};
