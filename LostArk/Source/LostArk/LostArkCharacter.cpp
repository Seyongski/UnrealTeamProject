<<<<<<< HEAD
=======
// Copyright Epic Games, Inc. All Rights Reserved.

>>>>>>> parent of 2bc260a (몬스터 / 캐릭터(리퍼))
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
<<<<<<< HEAD
#include "AbilitySystemComponent.h"
#include "LostArkAttributeSet.h"
#include "LostArkCharacterComboAttackAbility.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"

static const float DefaultCapsuleRadius = 42.f;
static const float DefaultCapsuleHalfHeight = 96.f;
static const float CharacterDefaultRotationRateYaw = 640.f;
static const float DefaultCameraBoomLength = 800.f;
static const float DefaultCameraBoomPitch = -60.f;
static const float DefaultBaseRunSpeed = 600.f;

ALostArkCharacter::ALostArkCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(DefaultCapsuleRadius, DefaultCapsuleHalfHeight);

=======

ALostArkCharacter::ALostArkCharacter()
{
	// Set size for player capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate character to camera direction
>>>>>>> parent of 2bc260a (몬스터 / 캐릭터(리퍼))
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

<<<<<<< HEAD
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, CharacterDefaultRotationRateYaw, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;
	GetCharacterMovement()->bAllowPhysicsRotationDuringAnimRootMotion = true;

	BaseRunSpeed = DefaultBaseRunSpeed;
	GetCharacterMovement()->MaxWalkSpeed = BaseRunSpeed;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->TargetArmLength = DefaultCameraBoomLength;
	CameraBoom->SetRelativeRotation(FRotator(DefaultCameraBoomPitch, 0.f, 0.f));
	CameraBoom->bDoCollisionTest = false;

	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AttributeSet = CreateDefaultSubobject<ULostArkAttributeSet>(TEXT("AttributeSet"));

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(GetMesh(), TEXT("b_wp_1"));

	bIsLeftFootForward = true;
	bIsDead = false;

	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

UAbilitySystemComponent* ALostArkCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ALostArkCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		if (HasAuthority())
		{
			if (ComboAttackAbilityClass && !ComboAttackAbilityHandle.IsValid())
			{
				ComboAttackAbilityHandle = AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(ComboAttackAbilityClass, 1, 0, this));
			}

			for (const FLostArkSkillInputBind& Bind : SkillInputBinds)
			{
				if (Bind.AbilityClass)
				{
					AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(Bind.AbilityClass, 1, static_cast<int32>(Bind.InputID), this));
				}
			}
		}
	}
}

void ALostArkCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		for (const FLostArkSkillInputBind& Bind : SkillInputBinds)
		{
			if (Bind.InputAction)
			{
				EnhancedInputComponent->BindAction(Bind.InputAction, ETriggerEvent::Started, this, &ALostArkCharacter::OnSkillInputPressed, Bind.InputID);
				EnhancedInputComponent->BindAction(Bind.InputAction, ETriggerEvent::Completed, this, &ALostArkCharacter::OnSkillInputReleased, Bind.InputID);
			}
		}
	}
}

void ALostArkCharacter::RequestComboAttackInput()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromHandle(ComboAttackAbilityHandle);
	if (Spec && Spec->IsActive())
	{
		ULostArkCharacterComboAttackAbility* ComboAbility = Cast<ULostArkCharacterComboAttackAbility>(Spec->GetPrimaryInstance());
		if (ComboAbility)
		{
			ComboAbility->RequestComboInput();
		}
	}
}

void ALostArkCharacter::Die()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
		AbilitySystemComponent->CancelAllAbilities();
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->StopMovementImmediately();
}

void ALostArkCharacter::SetCombatState(FGameplayTag NewStateTag)
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (CurrentStateTag.IsValid())
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(CurrentStateTag);
	}

	CurrentStateTag = NewStateTag;

	if (CurrentStateTag.IsValid())
	{
		AbilitySystemComponent->AddLooseGameplayTag(CurrentStateTag);
	}
}

void ALostArkCharacter::OnSkillInputPressed(ELostArkAbilityInputID InputID)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityLocalInputPressed(static_cast<int32>(InputID));
	}
}

void ALostArkCharacter::OnSkillInputReleased(ELostArkAbilityInputID InputID)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityLocalInputReleased(static_cast<int32>(InputID));
	}
}


=======
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
}

void ALostArkCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
}
>>>>>>> parent of 2bc260a (몬스터 / 캐릭터(리퍼))
