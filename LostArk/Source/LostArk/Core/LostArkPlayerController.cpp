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

	// [임시 디버그] Q/G 직접 바인딩 (Enhanced Input IA/매핑 없이). 나중에 이 줄 삭제
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ALostArkPlayerController::DebugForceCounterHit);
		InputComponent->BindKey(EKeys::G, IE_Pressed, this, &ALostArkPlayerController::DebugTryJustGuard);
	}
}

// ===== [임시 디버그] 아래 함수들은 카운터/그로기 확인용. 나중에 전부 삭제 =====
void ALostArkPlayerController::DebugForceCounterHit()
{
	// 입력은 클라이언트에서 들어오므로 서버 권한으로 넘겨서 실제 판정을 돌린다
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow, TEXT("[DEBUG] Q -> 카운터 강제 시도"));
	}
	ServerDebugForceCounterHit();
}

void ALostArkPlayerController::ServerDebugForceCounterHit_Implementation()
{
	APawn* MyPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!MyPawn || !World)
	{
		return;
	}

	// 월드의 첫 번째 보스를 찾아 헤드존 무시로 카운터 적중 강제 (창이 열려 있어야 실제로 먹힘)
	for (TActorIterator<ABossBase> It(World); It; ++It)
	{
		if (UBossCounterComponent* Counter = It->FindComponentByClass<UBossCounterComponent>())
		{
			Counter->NotifyCounterAttackHit(MyPawn, /*bBypassHeadZone=*/true);
		}
		break;	// 보스 1개 가정 (임시)
	}
}

void ALostArkPlayerController::DebugTryJustGuard()
{
	// 실제 판정/가드가능 여부는 서버가 안다 -> 서버로 넘겨 처리 + 메시지도 서버(리슨)에서 표시
	ServerDebugTryJustGuard();
}

void ALostArkPlayerController::ServerDebugTryJustGuard_Implementation()
{
	APawn* MyPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!MyPawn || !World)
	{
		return;
	}

	// 월드의 첫 보스의 저스트가드 컴포넌트로 가드 가능 여부 판단
	for (TActorIterator<ABossBase> It(World); It; ++It)
	{
		UBossJustGuardComponent* JustGuard = It->FindComponentByClass<UBossJustGuardComponent>();
		if (JustGuard && JustGuard->HasGuardReady(MyPawn))
		{
			// 창 열림 + 아직 가드 안 씀 -> 1회 가드 모션 발동 (성공/실패는 장판이 판정 시각에 표시)
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Cyan, TEXT("[DEBUG] G -> 저스트가드 모션 발동"));
			}
			JustGuard->NotifyGuardInput(MyPawn);
		}
		else if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("[DEBUG] G -> 저스트가드 모션 비활성화"));
		}
		break;	// 보스 1개 가정 (임시)
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
