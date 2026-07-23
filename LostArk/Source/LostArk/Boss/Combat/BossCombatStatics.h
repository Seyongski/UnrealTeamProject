// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "BossCombatStatics.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;

/** 공격자가 서 있는 보스 기준 존 (순수 지오메트리, 약점포착과 무관) */
UENUM(BlueprintType)
enum class EBossHitZone : uint8
{
	None	UMETA(DisplayName = "측면 (보너스 없음)"),
	Head	UMETA(DisplayName = "헤드 (정면)"),
	Back	UMETA(DisplayName = "백 (후면)"),
};

/** 스킬에 붙는 위치 판정 속성. 플레이어 스킬 데이터가 하나 들고 있으면 됨 */
UENUM(BlueprintType)
enum class EBossSkillPosition : uint8
{
	None		UMETA(DisplayName = "없음 (어디서 때려도 보너스 없음)"),
	BackAttack	UMETA(DisplayName = "백어택 스킬"),
	HeadAttack	UMETA(DisplayName = "헤드어택 스킬"),
};

/** 위치 판정 결과. 어떤 보너스를 적용하고 어떤 표기를 띄울지 */
UENUM(BlueprintType)
enum class EBossPositionalBonus : uint8
{
	None		UMETA(DisplayName = "보너스 없음"),
	BackAttack	UMETA(DisplayName = "백어택"),
	HeadAttack	UMETA(DisplayName = "헤드어택"),
	WeakPoint	UMETA(DisplayName = "약점포착"),
};

/**
 * 보스 공통 위치 판정 유틸 (모든 보스 + 플레이어 스킬 쪽에서 공용 사용).
 *
 * 규칙:
 *  - 백어택 스킬을 백 존에서 적중 -> 백어택 보너스 / 헤드어택 스킬을 헤드 존에서 -> 헤드어택 보너스
 *  - 위치 속성이 None 인 스킬은 어디서 때려도 보너스 없음
 *  - 보스에 State.Boss.WeakPointExposed(약점포착, 지형 파괴 후)가 있으면 위치와 무관하게
 *    스킬의 위치 보너스가 적용되고, 표기는 백/헤드어택 대신 '약점포착'으로 포괄된다
 *  - (중요) 카운터 판정은 약점포착과 무관하게 항상 '실제 헤드 존' 적중이어야 한다
 *    -> 카운터 쪽은 EvaluatePositionalBonus 가 아니라 GetHitZone(지오메트리)만 본다
 *
 * 조합 예 (카운터 창 열림 + 백어택 속성 + 카운터 판정 스킬):
 *  - 백에서 적중: 백어택 보너스 O, 카운터 X (헤드가 아니므로)
 *  - 헤드에서 적중: 백어택 보너스 X, 카운터 O
 *  판정 두 개가 독립이라 스킬 쪽은 EvaluatePositionalBonus 결과와
 *  Event.Boss.CounterHit 전송을 각각 처리하면 된다.
 *
 * 존 각도는 ABossBase 의 HeadZoneHalfAngle / BackZoneHalfAngle 로 보스별 편집 가능.
 */
UCLASS()
class LOSTARK_API UBossCombatStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 공격자 위치가 보스의 어느 존인지 (수평면 각도, 약점포착 무시한 순수 지오메트리) */
	UFUNCTION(BlueprintPure, Category = "Boss|Combat")
	static EBossHitZone GetHitZone(const AActor* BossActor, const FVector& AttackerLocation);

	/** 스킬 위치 속성 + 공격자 위치 + 약점포착 상태를 종합한 최종 보너스 판정 */
	UFUNCTION(BlueprintPure, Category = "Boss|Combat")
	static EBossPositionalBonus EvaluatePositionalBonus(const AActor* BossActor,
		const FVector& AttackerLocation, EBossSkillPosition SkillPosition);

	/** 카운터 성립 위치인지 (= 헤드 존). 약점포착이어도 헤드 존만 인정 */
	UFUNCTION(BlueprintPure, Category = "Boss|Combat")
	static bool IsHeadZoneHit(const AActor* BossActor, const FVector& AttackerLocation);

	/**
	 * 보스 무력화 게이지 딜 (서버 전용). 플레이어 스킬 적중 시 스킬의 무력화 수치만큼 호출하면 됨.
	 * 게이지가 0이 되면 (무력화 페이즈 중일 때) 보스가 즉시 그로기 — 처리는 보스 쪽이 알아서 한다.
	 * @param BossActor 보스
	 * @param Amount    깎을 양 (양수)
	 */
	UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
	static void ApplyStaggerDamage(AActor* BossActor, float Amount);

	// ─── 공용 헬퍼 (보스 시스템 전반에서 재사용, C++ 전용) ───

	/** 접속 중인 플레이어 컨트롤러들의 폰 수집 (null 폰 제외). 장판 판정/타겟팅/전하 부여 등이 공용 */
	static void GetPlayerPawns(const UWorld* World, TArray<APawn*>& OutPawns);

	/**
	 * 대상이 생존 상태인지 (DeadTag 기준).
	 * 규약: 액터 없음 = false / 태그 미지정·ASC 없음 = 판정 불가 -> 생존 간주(대상 포함)
	 */
	static bool IsAliveActor(const AActor* Actor, const FGameplayTag& DeadTag);

	/**
	 * 루스 태그를 서버 로컬 + 복제 양쪽에 부여 (클라 UI/입력 게이트까지 전파).
	 * 서버 권위에서만 호출할 것. 짝은 반드시 RemoveReplicatedLooseTag 로 회수.
	 */
	static void AddReplicatedLooseTag(UAbilitySystemComponent* ASC, const FGameplayTag& Tag);

	/** AddReplicatedLooseTag 의 회수 짝 */
	static void RemoveReplicatedLooseTag(UAbilitySystemComponent* ASC, const FGameplayTag& Tag);

	/**
	 * GE 1개 적용 공용 헬퍼 (서버 전용 규약).
	 * Context 생성 -> SourceObject/Instigator 주입 -> Spec 생성 -> SetByCaller -> 적용
	 * 관용구를 한 곳에 모았다 (데미지/상태이상/전하/그로기/낙사 등 전부 이 경로).
	 *
	 * @param SourceASC          시전자 ASC. null 이면 TargetASC 자기 자신이 소스(자가 적용)
	 * @param TargetASC          적용 대상 ASC (필수)
	 * @param SourceObject       Context.AddSourceObject (장판/볼륨/컴포넌트 등 원인 오브젝트. null 허용)
	 * @param Instigator         Context.AddInstigator 의 주체 (보스 등. EffectCauser 와 둘 다 null 이면 생략)
	 * @param SetByCallerTag     유효하면 Magnitude 를 SetByCaller 로 실어 보냄 (Data.Damage / Data.Duration 등)
	 * @return 적용된 이펙트 핸들 (실패 시 무효 핸들. 잡기처럼 나중에 제거할 때 보관)
	 */
	static FActiveGameplayEffectHandle ApplyEffect(
		UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC,
		TSubclassOf<UGameplayEffect> EffectClass, UObject* SourceObject,
		AActor* Instigator = nullptr, AActor* EffectCauser = nullptr,
		const FGameplayTag& SetByCallerTag = FGameplayTag(), float SetByCallerMagnitude = 0.f);

	/** ApplyEffect 의 자가 적용(소스=대상) 축약형. 대부분의 셀프 버프/디버프/사망 GE 는 이걸로 충분 */
	static FActiveGameplayEffectHandle ApplyEffectToSelf(
		UAbilitySystemComponent* ASC, TSubclassOf<UGameplayEffect> EffectClass, UObject* SourceObject,
		const FGameplayTag& SetByCallerTag = FGameplayTag(), float SetByCallerMagnitude = 0.f)
	{
		return ApplyEffect(nullptr, ASC, EffectClass, SourceObject,
			nullptr, nullptr, SetByCallerTag, SetByCallerMagnitude);
	}

	/**
	 * 전하 반전: 빨강/파랑 중 가진 쪽 전하 GE 를 제거하고 반대쪽을 부여.
	 * (전하 변환장판과 과충전 게이지 반전이 공유하는 로직)
	 * @return 반전 수행 여부. 전하 미부여(또는 비정상 중복)면 아무것도 안 하고 false
	 */
	static bool FlipCharge(UAbilitySystemComponent* ASC,
		TSubclassOf<UGameplayEffect> RedEffect, TSubclassOf<UGameplayEffect> BlueEffect,
		UObject* SourceObject, const FGameplayTag& RedTag, const FGameplayTag& BlueTag);
};
