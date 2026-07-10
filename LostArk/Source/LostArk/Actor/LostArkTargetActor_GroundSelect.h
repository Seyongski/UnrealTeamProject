#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "LostArkTargetActor_GroundSelect.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetSelectedSignature, const FVector&, SelectedLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTargetCancelledSignature);

UCLASS()
class LOSTARK_API ALostArkTargetActor_GroundSelect : public AGameplayAbilityTargetActor
{
	GENERATED_BODY()

public:
	ALostArkTargetActor_GroundSelect();

	virtual void StartTargeting(UGameplayAbility* Ability) override;
	virtual void ConfirmTargetingAndContinue() override;
	virtual void CancelTargeting() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Targeting")
	class UDecalComponent* DecalComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	float TargetRadius = 200.f;

	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FOnTargetSelectedSignature OnTargetSelected;

	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FOnTargetCancelledSignature OnTargetCancelled;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool GetMouseCursorLocation(FVector& OutLocation);

	bool bCanConfirm = false;
	float ActivatedTime = 0.f;
	bool bPrevLMB = false;
	bool bPrevRMB = false;
};

