// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "BossChargeGaugeComponent.generated.h"

class ABossPatternActorBase;
class UAbilitySystemComponent;
class UGameplayEffect;
class UUserWidget;
class UWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnChargeGaugeChanged, float, NewGauge, float, MaxGauge);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChargeSideChanged, bool, bIsRedCharge);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMarkedChanged, bool, bMarked);

/**
 * 플레이어 전하 게이지 (레이드 조우 시 BossRaidGameMode 가 각 플레이어 폰에 런타임 부착, 복제).
 *
 * 규칙:
 *  - '게이지를 채우는' 보스 패턴(UBossAoeChargeGaugeEffect 행동을 단 장판)에 맞으면 게이지 상승
 *  - 절반(50%)을 '넘는 순간' 1회: 전하 반전 (Plus<->Minus = Red<->Blue GE 스왑)
 *  - 가득 차는 순간: 게이지/전하는 그대로 두고, 이 캐릭터를 따라다니는
 *    거대 원형 장판(OverchargeAoeClass)이 예고 후 폭발 (과충전 페널티)
 *
 * UI 바인딩:
 *  - 발밑 게이지 바   : OnChargeGaugeChanged 델리게이트 (Gauge 는 복제되어 클라에서도 발동)
 *  - 머리 위 전하 아이콘: OnChargeSideChanged 델리게이트 (State.Charge.Red/Blue 태그 변화를
 *    감시해 대신 흘려주므로, 위젯은 GAS 태그를 몰라도 됨. BeginPlay 시 현재 상태 1회 즉시 방송)
 *
 * 주의: 전하는 AddLooseGameplayTag 가 아니라 진짜 GE(RedChargeEffect/BlueChargeEffect)로 부여된다.
 * 다른 클라에서도 이 태그가 보이려면 플레이어 ASC 의 ReplicationMode 가 Full(기본값)이어야 한다.
 * 누군가 성능상 Minimal 로 바꾸면 원격 클라의 머리 위 아이콘이 멈춘다 — 바꿀 일 있으면 공유할 것.
 */
UCLASS(Blueprintable, ClassGroup = (Boss), meta = (BlueprintSpawnableComponent))
class LOSTARK_API UBossChargeGaugeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossChargeGaugeComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 부착 직후 GameMode 가 호출: 보스(과충전 장판 시전자) + 전하 GE 클래스 주입 */
	void InitChargeGauge(AActor* InBoss,
		TSubclassOf<UGameplayEffect> InRedChargeEffect, TSubclassOf<UGameplayEffect> InBlueChargeEffect);

	/** 게이지 증감 (서버). 절반 상향 돌파 시 전하 반전, 가득 차는 순간 과충전 장판 */
	UFUNCTION(BlueprintCallable, Category = "Boss|Charge")
	void AddGauge(float Amount);

	/** 게이지 0으로 리셋 (서버. 기믹 연출 등 외부에서 필요 시) */
	UFUNCTION(BlueprintCallable, Category = "Boss|Charge")
	void ResetGauge();

	UFUNCTION(BlueprintPure, Category = "Boss|Charge")
	float GetGauge() const { return Gauge; }

	UFUNCTION(BlueprintPure, Category = "Boss|Charge")
	float GetMaxGauge() const { return MaxGauge; }

	/** 게이지 변경 방송 (서버 로직 + 클라 OnRep 양쪽에서 발동 -> 발밑 게이지 UI 바인딩) */
	UPROPERTY(BlueprintAssignable, Category = "Boss|Charge")
	FOnChargeGaugeChanged OnChargeGaugeChanged;

	/**
	 * 전하(플러스/마이너스) 변경 방송. 서버/클라 모두 태그 변화를 직접 감시해 발동하므로
	 * 반전(FlipOwnerCharge)이든 GameMode 의 최초 랜덤 부여든 원인 무관하게 반응한다.
	 * BeginPlay 시 현재 상태로 1회 즉시 방송 -> 늦게 생성된 위젯도 초기값을 놓치지 않음.
	 * 머리 위 전하 아이콘(빨강 플러스/파랑 마이너스) 위젯이 여기 바인딩.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Boss|Charge")
	FOnChargeSideChanged OnChargeSideChanged;

	/** 현재 빨강(플러스) 전하인지. 파랑(마이너스)이면 false. 위젯 Construct 시 초기값 조회용 */
	UFUNCTION(BlueprintPure, Category = "Boss|Charge")
	bool IsRedCharge() const;

	/**
	 * 어그로 표식(State.Player.Marked) 변경 방송. 서버/클라 모두 태그 변화를 직접 감시 → 원인 무관 반응.
	 * 몸통 마커 위젯이 여기 바인딩해 Visible/Hidden 을 토글한다(태그 = 지정된 1명 게이트).
	 * 표식은 UBossTargetingComponent::MarkCurrentTarget/ClearMark 로만 켜지므로, 일반 타겟 선정으론 안 뜬다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Boss|Charge")
	FOnMarkedChanged OnMarkedChanged;

	/** 현재 이 플레이어가 어그로 표식 대상인지. 위젯 Construct 시 초기값 조회용 */
	UFUNCTION(BlueprintPure, Category = "Boss|Charge")
	bool IsMarked() const;

	/** 게이지 최대치 */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Charge", meta = (ClampMin = "1.0"))
	float MaxGauge = 100.f;

	/**
	 * 과충전(가득) 시 이 캐릭터 기준으로 스폰할 거대 원형 장판.
	 * BP 권장 설정: SpawnOrigin=TargetLocation, TargetingMode=FollowTarget(따라다님),
	 * CastTime=예고 시간, Duration=0(1회 폭발), 큰 Radius.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Charge|Overcharge")
	TSubclassOf<ABossPatternActorBase> OverchargeAoeClass;

	/** 과충전 장판 공격력계수 (GE SetByCaller Data.Damage) */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Charge|Overcharge")
	float OverchargeDamageCoefficient = 3.f;

	/** 과충전 폭발 후 게이지를 0으로 리셋할지 (기본: 유지 — '게이지는 변하지 않는다') */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Charge|Overcharge")
	bool bResetGaugeAfterOvercharge = false;

	// ─── 자동 부착 UI (BP에 WidgetComponent 를 안 넣어도 모든 캐릭터가 UI를 받게) ───
	// 이 컴포넌트는 GameMode 가 모든 플레이어 폰에 붙이므로, 여기서 위젯을 생성하면
	// 캐릭터 BP 종류와 무관하게 전원이 동일한 전하 UI를 받는다. (Screen 스페이스 = 항상 카메라 향함)

	/** 머리 위 전하 아이콘 위젯 (UBossPlayerStatusWidget 자식 WBP). 비우면 아이콘 안 뜸 */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Charge|UI")
	TSubclassOf<UUserWidget> OverheadIconWidgetClass;

	/** 머리 위 아이콘의 폰 기준 Z 오프셋(cm). 캡슐 위로 띄우는 값 */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Charge|UI")
	float OverheadIconZOffset = 110.f;

	/** 머리 위 아이콘 드로우 크기(px) */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Charge|UI")
	FVector2D OverheadIconDrawSize = FVector2D(48.f, 48.f);

	/** 발밑 게이지 위젯 (UBossPlayerStatusWidget 자식 WBP). 비우면 게이지 안 뜸 */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Charge|UI")
	TSubclassOf<UUserWidget> FootGaugeWidgetClass;

	/** 발밑 게이지의 폰 기준 Z 오프셋(cm). 발밑이면 음수 */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Charge|UI")
	float FootGaugeZOffset = -85.f;

	/** 발밑 게이지 드로우 크기(px) */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Charge|UI")
	FVector2D FootGaugeDrawSize = FVector2D(120.f, 16.f);

	// ─── 몸통 어그로 마커 (지정된 1명에게만 표시. State.Player.Marked 태그가 게이트) ───

	/** 몸통 어그로 마커 위젯 (UBossPlayerStatusWidget 자식 WBP). 비우면 마커 안 뜸.
	 *  WBP 는 OnMarkedChanged 에 바인딩해 표식 있을 때만 Visible 로 만든다 */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Charge|UI")
	TSubclassOf<UUserWidget> BodyMarkWidgetClass;

	/** 몸통 마커의 폰 기준 Z 오프셋(cm). 몸통 중앙 근처(전하=머리 위, 게이지=발밑 사이) */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Charge|UI")
	float BodyMarkZOffset = 30.f;

	/** 몸통 마커 드로우 크기(px) */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Charge|UI")
	FVector2D BodyMarkDrawSize = FVector2D(64.f, 64.f);

protected:
	virtual void BeginPlay() override;

private:
	UAbilitySystemComponent* GetOwnerASC() const;

	/** 소유 플레이어의 전하 반전 (Red<->Blue GE 스왑 — 변환장판과 동일 로직) */
	void FlipOwnerCharge();

	/** Red/Blue 태그 변화 감시 콜백 -> OnChargeSideChanged 방송 (서버/클라 공통, 원인 무관) */
	void HandleChargeTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** State.Player.Marked 태그 변화 감시 콜백 -> OnMarkedChanged 방송 (서버/클라 공통) */
	void HandleMarkedTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** 렌더링 머신(데디서버 제외)에서 머리 위 아이콘/발밑 게이지 위젯 컴포넌트를 생성 + 주입 */
	void SetupStatusWidgets();

	/** 위젯 컴포넌트 1개 생성 헬퍼: Screen 스페이스로 폰에 부착 + UBossPlayerStatusWidget::InitStatus */
	UWidgetComponent* CreateStatusWidget(TSubclassOf<UUserWidget> WidgetClass,
		float ZOffset, const FVector2D& DrawSize, const TCHAR* CompName);

	/** 생성한 위젯 컴포넌트(정리/중복 생성 방지용) */
	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> OverheadIconComp;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> FootGaugeComp;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> BodyMarkComp;

	/** 과충전: 이 캐릭터를 따라다니는 거대 원장판 스폰 */
	void TriggerOvercharge();

	UFUNCTION()
	void OnRep_Gauge();

	/** 현재 게이지 (복제 -> 클라 UI) */
	UPROPERTY(ReplicatedUsing = OnRep_Gauge)
	float Gauge = 0.f;

	/** 과충전 장판 시전자 (보스. 데미지 귀속) */
	TWeakObjectPtr<AActor> Boss;

	TSubclassOf<UGameplayEffect> RedChargeEffect;
	TSubclassOf<UGameplayEffect> BlueChargeEffect;
};
