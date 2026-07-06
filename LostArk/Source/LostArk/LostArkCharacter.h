#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayAbilitySpec.h"
#include "LostArkCombatInterface.h"
#include "LostArkAbilityTypes.h"
#include "LostArkCharacter.generated.h"

USTRUCT(BlueprintType)
struct FLostArkSkillInputBind
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	class UInputAction* InputAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ELostArkAbilityInputID InputID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<class UGameplayAbility> AbilityClass;
};

UCLASS(Blueprintable)
class ALostArkCharacter : public ACharacter, public IAbilitySystemInterface, public ILostArkCombatInterface
{
	GENERATED_BODY()

public:
	ALostArkCharacter();

	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void PossessedBy(AController* NewController) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void Die() override;

	virtual bool IsDead() const override { return bIsDead; }

	virtual FGameplayTag GetCurrentStateTag() const override { return CurrentStateTag; }

	virtual void SetCombatState(FGameplayTag NewStateTag) override;

	UFUNCTION(BlueprintCallable, Category = "Character|Abilities")
	void RequestComboAttackInput();

	FORCEINLINE class UCameraComponent* GetTopDownCameraComponent() const { return TopDownCameraComponent; }
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE class USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

public:
	UPROPERTY(BlueprintReadWrite, Category = "Character|Anim")
	bool bIsLeftFootForward;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
	class UAbilitySystemComponent* AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
	class ULostArkAttributeSet* AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	class USkeletalMeshComponent* WeaponMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Abilities")
	TSubclassOf<class UGameplayAbility> ComboAttackAbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Abilities")
	TArray<FLostArkSkillInputBind> SkillInputBinds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Movement")
	float BaseRunSpeed;

	UPROPERTY(BlueprintReadOnly, Category = "Character")
	bool bIsDead;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|State")
	FGameplayTag CurrentStateTag;

private:
	void OnSkillInputPressed(ELostArkAbilityInputID InputID);
	void OnSkillInputReleased(ELostArkAbilityInputID InputID);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* TopDownCameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	FGameplayAbilitySpecHandle ComboAttackAbilityHandle;
};



