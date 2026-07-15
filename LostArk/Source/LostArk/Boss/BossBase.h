// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "BossBase.generated.h"

class UBackHeadDecalComponent;
class UAbilitySystemComponent;
class UBossAttributeSet;
class UBossCounterComponent;
class UBossJustGuardComponent;
class UBossPatternComponent;
class UBossTargetingComponent;
struct FOnAttributeChangeData;

UCLASS()
class LOSTARK_API ABossBase : public ACharacter, public IAbilitySystemInterface
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

	/** 위치 판정 존 각도 (UBossCombatStatics 가 읽어감) */
	float GetHeadZoneHalfAngle() const { return HeadZoneHalfAngle; }
	float GetBackZoneHalfAngle() const { return BackZoneHalfAngle; }

protected:
	/** 초기 체력/무력화 게이지를 어트리뷰트에 세팅 (서버) */
	void InitializeAttributes();

	/** 체력 변화 콜백 -> 퍼센트 계산 후 패턴 컴포넌트에 전달 (지연 페이즈 전환) */
	void OnHealthChanged(const FOnAttributeChangeData& Data);

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

private:
	/** 재빙의 시 어트리뷰트 리셋/델리게이트 중복 바인딩/전투 재시작 방지 */
	bool bGASInitialized = false;
};
