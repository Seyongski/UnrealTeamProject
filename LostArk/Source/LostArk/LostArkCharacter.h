// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "LostArkAttributeSet.h"
#include "LostArkCharacter.generated.h"

UCLASS(Blueprintable)
class ALostArkCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ALostArkCharacter();
	void BeginPlay();

	// Called every frame.
	virtual void Tick(float DeltaSeconds) override;

	/** Returns TopDownCameraComponent subobject **/
	FORCEINLINE class UCameraComponent* GetTopDownCameraComponent() const { return TopDownCameraComponent; }
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

private:
	/** Top down camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* TopDownCameraComponent;

	/** Camera boom positioning the camera above the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	bool bIsAttacking = false;
	bool bInputBuffered = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	UAbilitySystemComponent* AbilitySystemComponent;

	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayAbility> DefaultAttackAbility;

	UPROPERTY()
	UAttributeSet* AttributeSet;

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float ZoomSpeed = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MinZoom = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MaxZoom = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float DesiredArmLength;

	void ZoomIn();
	void ZoomOut();

	void NormalAttack(); //공격 입력 처리
	void ExecuteAttack(int32 Index); //실제 공격 애님재생(몇번째 평타인지?)
	void OnAttackFinished();


	bool bIsAttackHeld = false;


	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	int32 ComboIndex = 0;
	FTimerHandle ComboTimerHandle;

	float AttackInterval = 0.4f; //평타간격... WeaponSpeed로 넣으면 됨

	//void InitAbilitySystem();
	void GiveDefaultAbilities();
	//void InitializeAttributes();

};

