#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "LostArkCombatInterface.generated.h"

UINTERFACE(MinimalAPI)
class ULostArkCombatInterface : public UInterface
{
	GENERATED_BODY()
};

class LOSTARK_API ILostArkCombatInterface
{
	GENERATED_BODY()

public:
	virtual void Die() = 0;
	virtual bool IsDead() const = 0;
	virtual FGameplayTag GetCurrentStateTag() const = 0;
	virtual void SetCombatState(FGameplayTag NewStateTag) = 0;
};
