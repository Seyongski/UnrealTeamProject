#pragma once

#include "CoreMinimal.h"
#include "LostArk/UI/LostArkUserWidget.h"
#include "LostArk/Core/LostArkAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "LostArkSkillSlotWidget.generated.h"

/**
 * ?ㅽ궗 ?듭뒳濡??꾩젽 (荑⑤떎??諛?肄ㅻ낫 ?ㅽ궗 ?곹깭 ?곕룞)
 */
UCLASS()
class LOSTARK_API ULostArkSkillSlotWidget : public ULostArkUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Slot")
	ELostArkAbilityInputID InputID;

	// ?대떦 ?щ’??由ъ뒪?앺븷 荑⑤떎???쒓렇
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Slot")
	FGameplayTag CooldownTag;

	// 肄ㅻ낫 吏꾪뻾 ???섏떊???대깽???쒓렇
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Slot")
	FGameplayTag ComboEventTag;

protected:
	// 釉붾（?꾨┛?몄뿉 荑⑤떎???좊땲硫붿씠???몃━嫄?(吏꾪뻾??癒명떚由ъ뼹 ?먮뒗 ?꾨줈洹몃옒?ㅻ컮)
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Skill Slot")
	void OnCooldownStarted(float TimeRemaining, float Duration);

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Skill Slot")
	void OnCooldownEnded();

	// Event
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Skill Slot")
	void OnComboIconChanged(int32 NextIconIndex);

private:
	virtual void OnCooldownTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	virtual void OnComboEventReceived(const FGameplayEventData* Payload);

	FDelegateHandle CooldownTagDelegateHandle;
	FDelegateHandle ComboEventDelegateHandle;
};

