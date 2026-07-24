#include "Core/LostArkPlayerController.h"
#include "Core/LostArkGameInstance.h"
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
// [임시 디버그] 카운터 강제용 include (나중에 삭제)
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Boss/BossBase.h"
#include "Boss/Combat/BossCounterComponent.h"
#include "Boss/Combat/BossJustGuardComponent.h"
#include "Boss/Gimmick/BossTerrainGimmickComponent.h"
#include "Boss/Raid/BossRaidGameState.h"
#include "Boss/UI/BossHUDWidget.h"

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

	if (IsLocalController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}

		if (HUDWidgetClass)
		{
			HUDWidget = CreateWidget<ULostArkHUDWidget>(this, HUDWidgetClass);
			if (HUDWidget)
			{
				HUDWidget->AddToViewport();
				HUDWidget->BindAttributeDelegates();
			}
		}

		// 보스 레벨에서만 보스 체력 HUD 생성 (GameState 로 판별. 내부에서 복제 대기 재시도)
		TryCreateBossHUD();
	}
}

void ALostArkPlayerController::TryCreateBossHUD()
{
	// 로컬 화면 위젯이므로 로컬 컨트롤러만. 클래스 미지정/이미 생성됐으면 스킵.
	if (!IsLocalController() || !BossHUDWidgetClass || BossHUDWidget)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 보스 레벨 판별: 이 GameState 는 보스 레이드 레벨에서만 쓴다 (튜토/카오스던전은 다른 GameState).
	AGameStateBase* GameState = World->GetGameState();
	if (!GameState)
	{
		// GameState 아직 미복제 -> 잠깐 후 재시도 (아직 보스 레벨인지 알 수 없음)
		if (BossHUDRetryCount++ < 40) // 0.25s * 40 = 10s 상한
		{
			World->GetTimerManager().SetTimer(BossHUDRetryTimer, this, &ALostArkPlayerController::TryCreateBossHUD, 0.25f, false);
		}
		return;
	}
	if (!GameState->IsA<ABossRaidGameState>())
	{
		// 보스 레벨 아님 -> HUD 안 띄움 (재시도도 중단)
		return;
	}

	// 보스 찾기 (레벨 배치/스폰 후 복제까지 지연될 수 있음)
	ABossBase* Boss = nullptr;
	for (TActorIterator<ABossBase> It(World); It; ++It)
	{
		Boss = *It;
		break;
	}
	if (!Boss)
	{
		if (BossHUDRetryCount++ < 40)
		{
			World->GetTimerManager().SetTimer(BossHUDRetryTimer, this, &ALostArkPlayerController::TryCreateBossHUD, 0.25f, false);
		}
		return;
	}

	// 생성 + 배치 + 보스 체력 바인딩
	BossHUDWidget = CreateWidget<UBossHUDWidget>(this, BossHUDWidgetClass);
	if (BossHUDWidget)
	{
		BossHUDWidget->AddToViewport();
		BossHUDWidget->InitForBoss(Boss);
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

	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::G, IE_Pressed, this, &ALostArkPlayerController::TryJustGuardInput);
	}
}

void ALostArkPlayerController::TryJustGuardInput()
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return;

	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(ControlledPawn))
	{
		if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
		{
			const bool bHasGuardReady = ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Player.GuardReady"), false));
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(1004, 2.5f, bHasGuardReady ? FColor::Yellow : FColor::Silver,
					FString::Printf(TEXT("[G키 입력] 저스트가드 시도 (GuardReady 상태: %s)"), bHasGuardReady ? TEXT("ON") : TEXT("OFF")));
			}

			// 보스로부터 State.Player.GuardReady 태그를 받았을 때 G키 입력 시 저스트가드 어빌리티 시전
			ASC->AbilityLocalInputPressed(static_cast<int32>(ELostArkAbilityInputID::JustGuard));
			ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Ability.Skill.JustGuard"), false)));
		}
	}
}
// ==============================================================================

void ALostArkPlayerController::OnInputStarted()
{
	APawn* ActivePawn = GetPawn();
	if (ActivePawn)
	{
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(ActivePawn))
		{
			if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
			{
				if (ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking"), false)))
				{
					return;
				}
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
			if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
			{
				if (ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking"), false)))
				{
					return;
				}
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
			if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
			{
				if (ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking"), false)))
				{
					FollowTime = 0.f;
					return;
				}
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
	if (!InCharacter || !IsLocalController()) return;

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

void ALostArkPlayerController::ClientShowStageClearUI_Implementation()
{
	if (HUDWidget)
	{
		HUDWidget->OnShowStageClearUI();
	}

	if (StageClearWidgetClass && !StageClearWidget)
	{
		StageClearWidget = CreateWidget<UUserWidget>(this, StageClearWidgetClass);
		if (StageClearWidget)
		{
			StageClearWidget->AddToViewport();
		}
	}
}

void ALostArkPlayerController::ClientShowLoadingScreen_Implementation()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (ULostArkGameInstance* LostArkGI = Cast<ULostArkGameInstance>(GameInstance))
		{
			LostArkGI->ShowLoadingScreen();
		}
	}
}
