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

	AbilitySystemComponent = 
		CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AttributeSet = CreateDefaultSubobject<ULostArkAttributeSet>(TEXT("AttributeSet"));
}

void ALostArkCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	if(AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("ASC OK"));
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		GiveDefaultAbilities();
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("ASC is NULL!"));
	}

	if (AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("ASC Initialized"));

		// 테스트용: Ability 개수 확인
		UE_LOG(LogTemp, Warning, TEXT("Granted Abilities: %d"),
			AbilitySystemComponent->GetActivatableAbilities().Num());
	}
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
	UE_LOG(LogTemp, Warning, TEXT("NormalAttack"));
	bAttackBuffered = true;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC) return;

	const FGameplayTag AttackTag =
		FGameplayTag::RequestGameplayTag("State.Attacking");

	UE_LOG(LogTemp, Warning, TEXT("HasAttackTag = %d"),
		ASC->HasMatchingGameplayTag(AttackTag));
	
	
	if (!ASC->HasMatchingGameplayTag(
		FGameplayTag::RequestGameplayTag("State.Attacking")))
	{
		ASC->TryActivateAbilityByClass(DefaultAttackAbility);
	}
}

void ALostArkCharacter::SetAttackHeld(bool bHeld)
{
	bIsAttackHeld = bHeld;
}

bool ALostArkCharacter::IsAttackHeld() const
{
	return bIsAttackHeld;
}

UAbilitySystemComponent* ALostArkCharacter::GetAbilitySystemComponent() const
{
	UE_LOG(LogTemp, Warning, TEXT("GetAbilitySystemComponent CALLED"));

	return AbilitySystemComponent;
}

void ALostArkCharacter::GiveDefaultAbilities()
{
	if (!HasAuthority()) {
		return;
	}

	if (DefaultAttackAbility) {
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(DefaultAttackAbility, 1, 0));
	}

}
