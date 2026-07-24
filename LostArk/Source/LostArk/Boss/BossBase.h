// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Core/LostArkCombatInterface.h"
#include "GameplayTagContainer.h"
#include "BossBase.generated.h"

class UAnimMontage;
class UBackHeadDecalComponent;
class UAbilitySystemComponent;
class UBossAttributeSet;
class UBossCounterComponent;
class UBossJustGuardComponent;
class UBossPatternComponent;
class UBossTargetingComponent;
class UBossTerrainGimmickComponent;
class UBossWeaponComponent;
struct FOnAttributeChangeData;

/** 보스 체력 변동 방송 (서버/클라 모두). 체력바 위젯이 구독해서 갱신 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBossHealthChanged, float, NewHealth, float, MaxHealth);

/** 보스 사망 방송 (서버에서 1회). 클라 표현은 State.Dead 복제 태그/사망 몽타주 멀티캐스트가 담당 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBossDied);

UCLASS()
class LOSTARK_API ABossBase : public ACharacter, public IAbilitySystemInterface, public ILostArkCombatInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABossBase();

	virtual void PossessedBy(AController* NewController) override;

	//~ IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame (기본 비활성. bDrawFacingDebug 켤 때만 틱)
	virtual void Tick(float DeltaTime) override;

	/** 캡슐 반경에 맞춰 백헤드 데칼 크기/위치 갱신. 캡슐 크기가 바뀔 때(버프 등) 호출 */
	UFUNCTION(BlueprintCallable, Category = "Boss|BackHead")
	void UpdateBackHeadDecal();

	/** 회전 검증용: 캡슐(액터) forward 방향을 화살표로 표시 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Debug")
	bool bDrawFacingDebug = false;

	/** 체력 변동 방송 (서버/클라 모두). 보스 체력바 위젯이 여기 바인딩해서 갱신 */
	UPROPERTY(BlueprintAssignable, Category = "Boss|UI")
	FOnBossHealthChanged OnBossHealthChanged;

	/** 체력바를 몇 '줄'로 나눌지. 예) 500 -> 100%에서 x500줄, 50%에서 x250줄 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|UI", meta = (ClampMin = "1"))
	int32 TotalHealthBars = 500;

	/** 위젯 최초 구성용: 현재 체력 (없으면 0) */
	UFUNCTION(BlueprintPure, Category = "Boss|UI")
	float GetCurrentHealth() const;

	/** 위젯 최초 구성용: 최대 체력 (없으면 0) */
	UFUNCTION(BlueprintPure, Category = "Boss|UI")
	float GetMaxHealthValue() const;

	/** 사망 방송 (서버에서 1회. FX/사운드 등 BP 훅용) */
	UPROPERTY(BlueprintAssignable, Category = "Boss|Death")
	FOnBossDied OnBossDied;

	/** 사망 여부 (서버 기준. 클라는 State.Dead 복제 태그로 판단할 것) */
	UFUNCTION(BlueprintPure, Category = "Boss|Death")
	virtual bool IsDead() const override { return bDead; }

	//~ ILostArkCombatInterface
	virtual void Die() override;
	virtual void Revive() override {}
	virtual FGameplayTag GetCurrentStateTag() const override { return CurrentStateTag; }
	virtual void SetCombatState(FGameplayTag NewStateTag) override;
	virtual void ShowDamageText(float DamageAmount) override;

	/** 데미지 텍스트 액터 클래스 (몬스터/캐릭터와 동일한 방식) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|UI")
	TSubclassOf<class ALostArkDamageTextActor> DamageTextClass;

	/** 보스 기본 공격력 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Stats")
	float BaseAttackDamage = 100.f;

	/** 플레이어 데미지 적용 이펙트 클래스 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Combat")
	TSubclassOf<class UGameplayEffect> DefaultDamageEffectClass;

	/** 위치 판정 존 각도 (UBossCombatStatics 가 읽어감) */
	float GetHeadZoneHalfAngle() const { return HeadZoneHalfAngle; }
	float GetBackZoneHalfAngle() const { return BackZoneHalfAngle; }

protected:
	/** 초기 체력/무력화 게이지를 어트리뷰트에 세팅 (서버) */
	void InitializeAttributes();

	/** 체력 변화 콜백 -> 퍼센트 계산 후 패턴 컴포넌트에 전달 (지연 페이즈 전환). 0 도달 시 사망 진입 */
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	/**
	 * 사망 처리 (서버, 1회 가드).
	 * State.Dead 복제 태그 -> 패턴 정지 + 어빌리티 취소 -> 장판/타워 정리 -> 이동 정지 +
	 * 플레이어 통과 허용 -> 사망 몽타주 멀티캐스트 -> OnBossDied 방송 + 레이드 게임모드에 통지.
	 */
	virtual void HandleDeath();

	/**
	 * 사망 몽타주 재생 (전 머신). 종료 시 마지막 포즈로 고정(bPauseAnims) — 일어나기 방지.
	 * 이어서 bDestroyAfterDeathMontage 면 소멸 시퀀스를 예약한다
	 * (DeathDisappearDelay -> OnBossDisappear 방송 -> DisappearFXDuration -> Destroy).
	 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayDeathMontage();

	/** 사망 몽타주 (BP 에서 지정. 미지정 시 로그만 남기고 포즈 유지) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Death")
	TObjectPtr<UAnimMontage> DeathMontage;

	/** 사망 몽타주가 끝나면 보스를 월드에서 소멸시킬지. 끄면 시체가 그대로 남는다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Death")
	bool bDestroyAfterDeathMontage = true;

	/**
	 * 몽타주 종료 후 소멸 연출까지의 대기(초). 시체를 잠깐 보여주는 여유.
	 * 클리어 카메라/배너 타이밍(BossRaidGameMode 의 ClearBannerDelay 등)과 맞춰서 조절할 것.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Death",
		meta = (EditCondition = "bDestroyAfterDeathMontage", ClampMin = "0.0"))
	float DeathDisappearDelay = 2.f;

	/** 소멸 연출(OnBossDisappear) 방송 후 실제 액터 소멸까지의 유예(초). 디졸브 길이에 맞출 것 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Death",
		meta = (EditCondition = "bDestroyAfterDeathMontage", ClampMin = "0.0"))
	float DisappearFXDuration = 0.f;

	/**
	 * 소멸 연출 훅 (전 머신에서 호출). 디졸브 머티리얼/파티클/사운드를 BP 에서 붙인다.
	 * 이게 불린 뒤 DisappearFXDuration 초 후 서버가 액터를 Destroy 한다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|Death")
	void OnBossDisappear();

	/** 백/헤드 어택 방향을 표시하는 지면 데칼 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|BackHead")
	TObjectPtr<UBackHeadDecalComponent> BackHeadDecal;

	/** GAS 어빌리티 시스템 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** 보스 어트리뷰트 (체력/무력화) */
	UPROPERTY()
	TObjectPtr<UBossAttributeSet> AttributeSet;

	/** 패턴 흐름 브레인 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Pattern")
	TObjectPtr<UBossPatternComponent> PatternComponent;

	/** 타겟 선정 + 추적 회전 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Targeting")
	TObjectPtr<UBossTargetingComponent> TargetingComponent;

	/** 카운터 창/판정 (창 열림은 AnimNotifyState_BossCounter 가 토글) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Combat")
	TObjectPtr<UBossCounterComponent> CounterComponent;

	/** 저스트가드 창/판정 (창 열림은 AnimNotifyState_BossJustGuard 가 토글) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Combat")
	TObjectPtr<UBossJustGuardComponent> JustGuardComponent;

	/** 무기 착용 상태 표시 (맨손/양손/합체 — 전환은 AnimNotify_BossSetWeapon 이 호출) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Weapon")
	TObjectPtr<UBossWeaponComponent> WeaponComponent;

	/** 지형파괴 기믹 (타워 스폰/무력화 페이즈/슬라이스 파괴 — 각 단계는 AnimNotify_BossGimmick* 가 호출) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Gimmick")
	TObjectPtr<UBossTerrainGimmickComponent> TerrainGimmickComponent;

	/** 헤드어택 존: 보스 정면 기준 반각(도). 백헤드 데칼 표시 각도와 맞춰둘 것 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Combat", meta = (ClampMin = "0", ClampMax = "180"))
	float HeadZoneHalfAngle = 45.f;

	/** 백어택 존: 보스 후면 기준 반각(도) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Combat", meta = (ClampMin = "0", ClampMax = "180"))
	float BackZoneHalfAngle = 45.f;

	/** 시작 최대 체력 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stats")
	float InitialMaxHealth = 1000000.f;

	/** 시작 최대 무력화 게이지 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stats")
	float InitialMaxStaggerGauge = 1000.f;

	/** 소멸 연출 방송 (전 머신). BP 의 OnBossDisappear 를 호출 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastBossDisappear();

private:
	/** 사망 몽타주 종료 콜백: 마지막 포즈 고정 + (옵션) 소멸 예약 */
	void OnDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** 소멸 예약 (서버 전용, 1회). DeathDisappearDelay 후 BeginDisappear */
	void ScheduleDisappear();

	/** 소멸 연출 방송 + DisappearFXDuration 후 실제 Destroy (서버) */
	void BeginDisappear();

	/** 소멸 시퀀스 타이머 (대기 -> 연출 -> Destroy 공용) */
	FTimerHandle DisappearTimer;

	/** 소멸 예약 1회 가드 (몽타주 종료가 여러 번 와도 안전) */
	bool bDisappearScheduled = false;

	/** 재빙의 시 어트리뷰트 리셋/델리게이트 중복 바인딩/전투 재시작 방지 */
	bool bGASInitialized = false;

	/** 사망 처리 1회 가드 (서버) */
	bool bDead = false;
protected:
	/** 현재 상태 태그 (전투 인터페이스용) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|State")
	FGameplayTag CurrentStateTag;
};
