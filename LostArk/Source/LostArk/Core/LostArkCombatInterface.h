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
	/** 부활 (사망 상태 해제/콜리전·이동 복구). 기본 no-op — 부활 가능한 액터(플레이어)만 오버라이드 */
	virtual void Revive() {}
	virtual bool IsDead() const = 0;
	virtual FGameplayTag GetCurrentStateTag() const = 0;
	virtual void SetCombatState(FGameplayTag NewStateTag) = 0;
};

