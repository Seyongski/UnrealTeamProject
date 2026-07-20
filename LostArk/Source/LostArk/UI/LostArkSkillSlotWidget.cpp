#include "LostArk/UI/LostArkSkillSlotWidget.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectTypes.h"

void ULostArkSkillSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (ASC)
	{
		// 荑⑤떎???쒓렇 諛붿씤??		if (CooldownTag.IsValid())
		{
			CooldownTagDelegateHandle = ASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ULostArkSkillSlotWidget::OnCooldownTagChanged);
		}

		// 肄ㅻ낫 ?대깽??諛붿씤??		if (ComboEventTag.IsValid())
		{
			ComboEventDelegateHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(ComboEventTag).AddUObject(this, &ULostArkSkillSlotWidget::OnComboEventReceived);
		}
	}
}

void ULostArkSkillSlotWidget::NativeDestruct()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (ASC)
	{
		if (CooldownTag.IsValid() && CooldownTagDelegateHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved).Remove(CooldownTagDelegateHandle);
		}

		if (ComboEventTag.IsValid() && ComboEventDelegateHandle.IsValid())
		{
			ASC->GenericGameplayEventCallbacks.FindOrAdd(ComboEventTag).Remove(ComboEventDelegateHandle);
		}
	}

	Super::NativeDestruct();
}

void ULostArkSkillSlotWidget::OnCooldownTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (NewCount > 0)
	{
		UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
		if (ASC)
		{
			FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(CooldownTag));
			TArray<float> Durations = ASC->GetActiveEffectsTimeRemaining(Query);
			
			if (Durations.Num() > 0)
			{
				float TimeRemaining = Durations[0];
				
				// ?꾩껜 Duration??李얘린 ?꾪빐 Effect ?뺣낫 議고쉶
				float TotalDuration = TimeRemaining; 
				TArray<float> AllDurations = ASC->GetActiveEffectsDuration(Query);
				if(AllDurations.Num() > 0)
				{
					TotalDuration = AllDurations[0];
				}

				OnCooldownStarted(TimeRemaining, TotalDuration);
			}
			else
			{
				// Effect 荑쇰━媛 ?ㅽ뙣?덉뼱???꾩쓽??荑⑤떎???곗텧 ?쒖옉 (?鍮꾩슜)
				OnCooldownStarted(1.0f, 1.0f);
			}
		}
	}
	else
	{
		OnCooldownEnded();
	}
}

void ULostArkSkillSlotWidget::OnComboEventReceived(const FGameplayEventData* Payload)
{
	if (Payload)
	{
		// Payload??EventMagnitude瑜??몃뜳?ㅻ줈 ?ъ슜 (LostArkComboSkillAbility?먯꽌 ?섍꺼以??덉젙)
		int32 NextIconIndex = FMath::RoundToInt(Payload->EventMagnitude);
		
		// 釉붾（?꾨┛?몃줈 蹂寃??대깽?몃? ?꾨떖
		OnComboIconChanged(NextIconIndex);
	}
}


