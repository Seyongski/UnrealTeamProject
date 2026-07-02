// Copyright Epic Games, Inc. All Rights Reserved.

#include "LostArkCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/Material.h"
#include "Engine/World.h"

ALostArkCharacter::ALostArkCharacter()
{
	// Set size for player capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate character to camera direction
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Rotate character to moving direction
	GetCharacterMovement()->RotationRate = FRotator(0.f, 640.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;

	// Create a camera boom...
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true); // Don't want arm to rotate when character does
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	CameraBoom->bDoCollisionTest = false; // Don't want to pull camera in when it collides with level

	// Create a camera...
	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Activate ticking in order to update the cursor every frame.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	//set default camera distance
	DesiredArmLength = 1400.f;
	CameraBoom->TargetArmLength = 1400.f;
}

void ALostArkCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

	if (!CameraBoom) return;

	CameraBoom->TargetArmLength = FMath::FInterpTo(
		CameraBoom->TargetArmLength,
		DesiredArmLength,
		DeltaSeconds,
		8.f //부드러움 강도 (클수록 빠름)
	);
}

void ALostArkCharacter::ZoomIn()
{
	DesiredArmLength -= ZoomSpeed;
	DesiredArmLength = FMath::Clamp(DesiredArmLength, MinZoom, MaxZoom);
}

void ALostArkCharacter::ZoomOut()
{
	DesiredArmLength += ZoomSpeed;
	DesiredArmLength = FMath::Clamp(DesiredArmLength, MinZoom, MaxZoom);
}

void ALostArkCharacter::NormalAttack()
{
	if (bIsAttacking) {
		bInputBuffered = true;
		return;
	}
	ComboIndex = (ComboIndex % 3) + 1;
	ExecuteAttack(ComboIndex);

}

void ALostArkCharacter::ExecuteAttack(int32 Index)
{
	UE_LOG(LogTemp, Warning, TEXT("Todo:Play Attack Anim!!Count : %d"), Index);

	bIsAttacking = true;

	//공격 간격 타이머(=애니메이션이 재생될 시간)
	GetWorldTimerManager().ClearTimer(ComboTimerHandle);
	GetWorldTimerManager().SetTimer(
		ComboTimerHandle,
		this,
		&ALostArkCharacter::OnAttackFinished,
		AttackInterval,
		false
	);

	switch (Index) {
	
	case 1:
		//PlayAnimMontage(Attack1Montage);
		break;
	case 2:
		//PlayAnimMontage(Attack2Montage);
		break;
	case 3:
		//PlayAnimMontage(Attack3Montage);
		break;

	default:
		break;
	}


}

void ALostArkCharacter::OnAttackFinished()
{
	bIsAttacking = false;

	GetWorldTimerManager().ClearTimer(ComboTimerHandle);
	if (bInputBuffered) {
		bInputBuffered = false;
		NormalAttack();
		return;
	}

	//꾹눌렀을때
	if (bIsAttackHeld) {
		NormalAttack();
	}
}
