#include "LostArk/Actor/LostArkTargetActor_GroundSelect.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/PlayerController.h"
#include "Components/DecalComponent.h"
#include "Kismet/GameplayStatics.h"
#include "InputAction.h"          // 엔진이 제공하는 헤더 그대로 가져다 씀
#include "EnhancedInputComponent.h"

ALostArkTargetActor_GroundSelect::ALostArkTargetActor_GroundSelect()
{
	UE_LOG(LogTemp, Warning, TEXT("[TargetActor] Constructor Called!"));
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	DecalComponent = CreateDefaultSubobject<UDecalComponent>(TEXT("DecalComponent"));
	DecalComponent->SetupAttachment(RootComponent);
	DecalComponent->DecalSize = FVector(1000.f, 200.f, 200.f);
	DecalComponent->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	DecalComponent->SetVisibility(false);
}

void ALostArkTargetActor_GroundSelect::StartTargeting(UGameplayAbility* Ability)
{
	Super::StartTargeting(Ability);
	UE_LOG(LogTemp, Warning, TEXT("[TargetActor] StartTargeting Called!"));

	DecalComponent->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));

	DecalComponent->DecalSize = FVector(1000.f, TargetRadius, TargetRadius);
	DecalComponent->SetVisibility(true);
	SetActorTickEnabled(true);

	bCanConfirm = false;
	ActivatedTime = 0.f;
	bPrevLMB = true;
	bPrevRMB = false;

	if (SkillInputAction)
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		APawn* ControlledPawn = PC ? PC->GetPawn() : nullptr;

		UEnhancedInputComponent* EIC = nullptr;
		if (ControlledPawn)
		{
			EIC = Cast<UEnhancedInputComponent>(ControlledPawn->InputComponent);
		}
		if (!EIC && PC)
		{
			EIC = Cast<UEnhancedInputComponent>(PC->InputComponent);
		}

		if (EIC)
		{
			SkillActionBindingHandle = &EIC->BindAction(
				SkillInputAction,
				ETriggerEvent::Started,
				this,
				&ALostArkTargetActor_GroundSelect::OnSkillInputPressed
			);
			UE_LOG(LogTemp, Warning, TEXT("[TargetActor] Skill Bind Success"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[TargetActor] EnhancedInputComponent Cast Failed!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[TargetActor] SkillInputAction is NULL!"));
	}
}

void ALostArkTargetActor_GroundSelect::OnSkillInputPressed()
{
	UE_LOG(LogTemp, Warning, TEXT("[TargetActor] Skill Key Pressed - Confirm Attempt"));
	if (!bCanConfirm) return; // 발동 직후 0.15초 딜레이 그대로 적용됨
	ConfirmTargetingAndContinue();
}

void ALostArkTargetActor_GroundSelect::ConfirmTargetingAndContinue()
{
	UE_LOG(LogTemp, Warning, TEXT("[TargetActor] ConfirmTargetingAndContinue Called!"));
	FVector TargetLocation;
	if (GetMouseCursorLocation(TargetLocation))
	{
		OnTargetSelected.Broadcast(TargetLocation);
	}
	else
	{
		CancelTargeting();
	}
}

void ALostArkTargetActor_GroundSelect::CancelTargeting()
{
	OnTargetCancelled.Broadcast();
	Destroy();
}

void ALostArkTargetActor_GroundSelect::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FVector TargetLocation;
	if (GetMouseCursorLocation(TargetLocation))
	{
		SetActorLocation(TargetLocation);
	}

	ActivatedTime += DeltaSeconds;
	if (!bCanConfirm && ActivatedTime >= 0.15f)
	{
		bCanConfirm = true;
	}
	if (!bCanConfirm) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	bool bCurLMB = PC->IsInputKeyDown(EKeys::LeftMouseButton);
	bool bCurRMB = PC->IsInputKeyDown(EKeys::RightMouseButton);

	if (bCurLMB && !bPrevLMB)
	{
		bPrevLMB = bCurLMB;
		ConfirmTargetingAndContinue();
		return;
	}

	if (bCurRMB && !bPrevRMB)
	{
		bPrevRMB = bCurRMB;
		CancelTargeting();
		return;
	}

	bPrevLMB = bCurLMB;
	bPrevRMB = bCurRMB;
}

void ALostArkTargetActor_GroundSelect::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

bool ALostArkTargetActor_GroundSelect::GetMouseCursorLocation(FVector& OutLocation)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		FHitResult HitResult;
		if (PC->GetHitResultUnderCursor(ECC_Visibility, false, HitResult))
		{
			OutLocation = HitResult.ImpactPoint;
			return true;
		}
	}
	return false;
}




