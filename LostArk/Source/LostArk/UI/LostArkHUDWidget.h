#pragma once

#include "CoreMinimal.h"
#include "LostArk/UI/LostArkUserWidget.h"
#include "GameplayEffectTypes.h"
#include "LostArkHUDWidget.generated.h"

/**
 * ?뚮젅?댁뼱??硫붿씤 HUD ?꾩젽 (HP, MP, Identity 寃뚯씠吏 愿由?
 */
UCLASS()
class LOSTARK_API ULostArkHUDWidget : public ULostArkUserWidget
{
	GENERATED_BODY()

public:
	// ?꾩젽???앹꽦?섍퀬 ?쒖뒪??而댄룷?뚰듃媛 ?좏슚?????몄텧?섏뿬 ?몃━寃뚯씠?몃? 諛붿씤?⑺빀?덈떎.
	UFUNCTION(BlueprintCallable, Category = "UI|HUD")
	virtual void BindAttributeDelegates();

protected:
	// ?띿꽦 蹂寃???釉붾（?꾨┛?몃줈 ?대깽???꾨떖 (UI ?곗텧??
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|HUD")
	void OnHealthChanged(float CurrentHealth, float MaxHealth);

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|HUD")
	void OnManaChanged(float CurrentMana, float MaxMana);

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|HUD")
	void OnIdentityGaugeChanged(float CurrentGauge, float MaxGauge);

private:
	void HealthChanged(const FOnAttributeChangeData& Data);
	void MaxHealthChanged(const FOnAttributeChangeData& Data);
	void ManaChanged(const FOnAttributeChangeData& Data);
	void MaxManaChanged(const FOnAttributeChangeData& Data);
	void IdentityGaugeChanged(const FOnAttributeChangeData& Data);
	void MaxIdentityGaugeChanged(const FOnAttributeChangeData& Data);

	// ?꾩옱 ?띿꽦 媛?罹먯떛
	float CachedHealth = 0.f;
	float CachedMaxHealth = 0.f;
	float CachedMana = 0.f;
	float CachedMaxMana = 0.f;
	float CachedIdentityGauge = 0.f;
	float CachedMaxIdentityGauge = 0.f;
};

