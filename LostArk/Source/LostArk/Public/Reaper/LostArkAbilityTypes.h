#pragma once

#include "CoreMinimal.h"
#include "LostArkAbilityTypes.generated.h"

UENUM(BlueprintType)
enum class ELostArkAbilityInputID : uint8
{
	None,
	Confirm,
	Cancel,
	ComboAttack,
	SkillQ,
	SkillW,
	SkillE,
	SkillR,
	SkillA,
	SkillS,
	SkillD,
	SkillF
};
