// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "BossAttributeSet.generated.h"

// 어트리뷰트 접근자(Getter/Setter/Initter) 일괄 생성 매크로
// (팀원의 다른 AttributeSet 헤더에서 같은 매크로를 정의해도 충돌하지 않도록 가드)
#ifndef ATTRIBUTE_ACCESSORS
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
#endif

/**
 * 보스 전용 AttributeSet. (플레이어 셋과 분리 -> 머지 충돌 없음)
 * - Health / MaxHealth : 체력. 페이즈 전환 판단에 사용
 * - StaggerGauge / MaxStaggerGauge : 무력화 게이지. 0이 되면 그로기
 */
UCLASS()
class LOSTARK_API UBossAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UBossAttributeSet();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, Category = "Vital", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UBossAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Vital", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UBossAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, Category = "Stagger", ReplicatedUsing = OnRep_StaggerGauge)
	FGameplayAttributeData StaggerGauge;
	ATTRIBUTE_ACCESSORS(UBossAttributeSet, StaggerGauge)

	UPROPERTY(BlueprintReadOnly, Category = "Stagger", ReplicatedUsing = OnRep_MaxStaggerGauge)
	FGameplayAttributeData MaxStaggerGauge;
	ATTRIBUTE_ACCESSORS(UBossAttributeSet, MaxStaggerGauge)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_StaggerGauge(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxStaggerGauge(const FGameplayAttributeData& OldValue);
};
