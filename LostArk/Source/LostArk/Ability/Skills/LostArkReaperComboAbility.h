#pragma once

#include "CoreMinimal.h"
#include "LostArk/Ability/LostArkComboSkillAbility.h"
#include "LostArkReaperComboAbility.generated.h"

UCLASS()
class LOSTARK_API ULostArkReaperComboAbility : public ULostArkComboSkillAbility
{
	GENERATED_BODY()

public:
	ULostArkReaperComboAbility();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	TSoftClassPtr<class ALostArkShadowClone> ShadowCloneClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	TSoftObjectPtr<class UAnimMontage> LeftCloneMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	TSoftObjectPtr<class UAnimMontage> RightCloneMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	float CloneSpawnOffsetForward;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	float CloneSpawnOffsetRight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	FDamageShapeParams CloneDamageShapeParams;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Dash")
	float FinalDashDistance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Dash")
	float FinalDashDuration;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Dash")
	FDamageShapeParams FinalDashDamageShapeParams;

protected:
	virtual void PlayComboSegment(int32 Index) override;
	virtual void OnHitCheckReceived(FGameplayEventData Payload) override;
};
