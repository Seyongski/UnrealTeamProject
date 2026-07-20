#include "LostArk/UI/LostArkHUDWidget.h"
#include "AbilitySystemComponent.h"
#include "LostArk/Character/LostArkAttributeSet.h"

void ULostArkHUDWidget::BindAttributeDelegates()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	// Health
	ASC->GetGameplayAttributeValueChangeDelegate(ULostArkAttributeSet::GetHealthAttribute()).AddUObject(this, &ULostArkHUDWidget::HealthChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(ULostArkAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ULostArkHUDWidget::MaxHealthChanged);

	// Mana
	ASC->GetGameplayAttributeValueChangeDelegate(ULostArkAttributeSet::GetManaAttribute()).AddUObject(this, &ULostArkHUDWidget::ManaChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(ULostArkAttributeSet::GetMaxManaAttribute()).AddUObject(this, &ULostArkHUDWidget::MaxManaChanged);

	// Identity
	ASC->GetGameplayAttributeValueChangeDelegate(ULostArkAttributeSet::GetIdentityGaugeAttribute()).AddUObject(this, &ULostArkHUDWidget::IdentityGaugeChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(ULostArkAttributeSet::GetMaxIdentityGaugeAttribute()).AddUObject(this, &ULostArkHUDWidget::MaxIdentityGaugeChanged);

	// 珥덇린媛?諛섏쁺???꾪빐 媛뺤젣 ?몄텧
	bool bFound = false;
	CachedHealth = ASC->GetGameplayAttributeValue(ULostArkAttributeSet::GetHealthAttribute(), bFound);
	CachedMaxHealth = ASC->GetGameplayAttributeValue(ULostArkAttributeSet::GetMaxHealthAttribute(), bFound);
	CachedMana = ASC->GetGameplayAttributeValue(ULostArkAttributeSet::GetManaAttribute(), bFound);
	CachedMaxMana = ASC->GetGameplayAttributeValue(ULostArkAttributeSet::GetMaxManaAttribute(), bFound);
	CachedIdentityGauge = ASC->GetGameplayAttributeValue(ULostArkAttributeSet::GetIdentityGaugeAttribute(), bFound);
	CachedMaxIdentityGauge = ASC->GetGameplayAttributeValue(ULostArkAttributeSet::GetMaxIdentityGaugeAttribute(), bFound);

	OnHealthChanged(CachedHealth, CachedMaxHealth);
	OnManaChanged(CachedMana, CachedMaxMana);
	OnIdentityGaugeChanged(CachedIdentityGauge, CachedMaxIdentityGauge);
}

void ULostArkHUDWidget::HealthChanged(const FOnAttributeChangeData& Data)
{
	CachedHealth = Data.NewValue;
	OnHealthChanged(CachedHealth, CachedMaxHealth);
}

void ULostArkHUDWidget::MaxHealthChanged(const FOnAttributeChangeData& Data)
{
	CachedMaxHealth = Data.NewValue;
	OnHealthChanged(CachedHealth, CachedMaxHealth);
}

void ULostArkHUDWidget::ManaChanged(const FOnAttributeChangeData& Data)
{
	CachedMana = Data.NewValue;
	OnManaChanged(CachedMana, CachedMaxMana);
}

void ULostArkHUDWidget::MaxManaChanged(const FOnAttributeChangeData& Data)
{
	CachedMaxMana = Data.NewValue;
	OnManaChanged(CachedMana, CachedMaxMana);
}

void ULostArkHUDWidget::IdentityGaugeChanged(const FOnAttributeChangeData& Data)
{
	CachedIdentityGauge = Data.NewValue;
	OnIdentityGaugeChanged(CachedIdentityGauge, CachedMaxIdentityGauge);
}

void ULostArkHUDWidget::MaxIdentityGaugeChanged(const FOnAttributeChangeData& Data)
{
	CachedMaxIdentityGauge = Data.NewValue;
	OnIdentityGaugeChanged(CachedIdentityGauge, CachedMaxIdentityGauge);
}

