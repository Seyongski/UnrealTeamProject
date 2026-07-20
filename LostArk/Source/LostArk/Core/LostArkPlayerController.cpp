#include "Core/LostArkPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Character/LostArkCharacter.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Core/LostArkCombatInterface.h"
#include "Blueprint/UserWidget.h"
#include "UI/LostArkHUDWidget.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

ALostArkPlayerController::ALostArkPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	CachedDestination = FVector::ZeroVector;
	FollowTime = 0.f;
}

void ALostArkPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}

}

void ALostArkPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Started, this, &ALostArkPlayerController::OnInputStarted);
		EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Triggered, this, &ALostArkPlayerController::OnSetDestinationTriggered);
		EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Completed, this, &ALostArkPlayerController::OnSetDestinationReleased);
		EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Canceled, this, &ALostArkPlayerController::OnSetDestinationReleased);

		EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Started, this, &ALostArkPlayerController::OnInputStarted);
		EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Triggered, this, &ALostArkPlayerController::OnTouchTriggered);
		EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Completed, this, &ALostArkPlayerController::OnTouchReleased);
		EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Canceled, this, &ALostArkPlayerController::OnTouchReleased);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input Component!"), *GetNameSafe(this));
	}
}

void ALostArkPlayerController::OnInputStarted()
{
	APawn* ActivePawn = GetPawn();
	if (ActivePawn)
	{
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(ActivePawn))
		{
			if (ASI->GetAbilitySystemComponent()->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking"), false)))
			{
				return;
			}
		}
	}
	StopMovement();
}

void ALostArkPlayerController::OnSetDestinationTriggered()
{
	APawn* ActivePawn = GetPawn();
	if (ActivePawn)
	{
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(ActivePawn))
		{
			if (ASI->GetAbilitySystemComponent()->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking"), false)))
			{
				return;
			}
		}
	}

	FollowTime += GetWorld()->GetDeltaSeconds();
	
	FHitResult Hit;
	bool bHitSuccessful = false;
	if (bIsTouch)
	{
		bHitSuccessful = GetHitResultUnderFinger(ETouchIndex::Touch1, ECollisionChannel::ECC_Visibility, true, Hit);
	}
	else
	{
		bHitSuccessful = GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, Hit);
	}

	if (bHitSuccessful)
	{
		CachedDestination = Hit.Location;
	}
	
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn != nullptr)
	{
		FVector WorldDirection = (CachedDestination - ControlledPawn->GetActorLocation()).GetSafeNormal();
		ControlledPawn->AddMovementInput(WorldDirection, 1.0, false);
	}
}

void ALostArkPlayerController::OnSetDestinationReleased()
{
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn)
	{
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(ControlledPawn))
		{
			if (ASI->GetAbilitySystemComponent()->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking"), false)))
			{
				FollowTime = 0.f;
				return;
			}
		}
	}

	if (FollowTime <= ShortPressThreshold)
	{
		UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, CachedDestination);
		
		if (!FXCursor.IsNull())
		{
			UNiagaraSystem* SpawnedFX = FXCursor.LoadSynchronous();
			if (SpawnedFX)
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, SpawnedFX, CachedDestination, FRotator::ZeroRotator, FVector(1.f, 1.f, 1.f), true, true, ENCPoolMethod::None, true);
			}
		}
	}

	FollowTime = 0.f;
}

void ALostArkPlayerController::OnTouchTriggered()
{
	bIsTouch = true;
	OnSetDestinationTriggered();
}

void ALostArkPlayerController::OnTouchReleased()
{
	bIsTouch = false;
	OnSetDestinationReleased();
}

void ALostArkPlayerController::InitializeHUDForCharacter(class ALostArkCharacter* InCharacter)
{
	if (!InCharacter) return;

	// 기존 HUD가 있으면 제거
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}

	// 캐릭터에 지정된 고유 HUD 클래스가 있다면 우선 사용, 없으면 컨트롤러의 기본 HUD 클래스 사용
	TSubclassOf<class ULostArkHUDWidget> ClassToUse = InCharacter->CharacterHUDClass ? InCharacter->CharacterHUDClass : HUDWidgetClass;

	if (ClassToUse)
	{
		HUDWidget = CreateWidget<ULostArkHUDWidget>(this, ClassToUse);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport();
			HUDWidget->BindAttributeDelegates();
		}
	}
}
