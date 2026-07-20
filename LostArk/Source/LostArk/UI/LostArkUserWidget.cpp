#include "LostArk/UI/LostArkUserWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

UAbilitySystemComponent* ULostArkUserWidget::GetAbilitySystemComponent() const
{
	if (APawn* OwningPawn = GetOwningPlayerPawn())
	{
		return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningPawn);
	}
	return nullptr;
}

