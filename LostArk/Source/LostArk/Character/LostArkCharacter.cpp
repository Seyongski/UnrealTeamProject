#include "Character/LostArkCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/Material.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "Character/LostArkAttributeSet.h"
#include "Abilities/LostArkCharacterComboAttackAbility.h"
#include "Abilities/LostArkJustGuardAbility.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "UI/LostArkDamageTextActor.h"
#include "Combat/LostArkObjectPoolSubsystem.h"
#include "Abilities/LostArkSkill_Targeting.h"
#include "Core/LostArkPlayerController.h"
#include "Net/UnrealNetwork.h"

static const float DefaultCapsuleRadius = 42.f;
static const float DefaultCapsuleHalfHeight = 96.f;
static const float CharacterDefaultRotationRateYaw = 640.f;
static const float DefaultCameraBoomLength = 800.f;
static const float DefaultCameraBoomPitch = -60.f;
static const float DefaultBaseRunSpeed = 600.f;

ALostArkCharacter::ALostArkCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(DefaultCapsuleRadius, DefaultCapsuleHalfHeight);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

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
	bIsWeaponEquipped = false;

	static ConstructorHelpers::FClassFinder<ALostArkDamageTextActor> DamageTextClassFinder(TEXT("/Game/Reaper/UI/BP_DamageTextActor"));
	if (DamageTextClassFinder.Succeeded())
	{
		DamageTextClass = DamageTextClassFinder.Class;
	}

	JustGuardAbilityClass = ULostArkJustGuardAbility::StaticClass();

	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

UAbilitySystemComponent* ALostArkCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ALostArkCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALostArkCharacter, CurrentStateTag);
	DOREPLIFETIME(ALostArkCharacter, bIsDead);
	DOREPLIFETIME(ALostArkCharacter, bIsWeaponEquipped);
	DOREPLIFETIME(ALostArkCharacter, bIsLeftFootForward);
}

void ALostArkCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (WeaponMesh && GetMesh())
	{
		WeaponMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponUnequippedSocketName);
	}
}

void ALostArkCharacter::SetWeaponEquipped(bool bIsEquipped)
{
	bIsWeaponEquipped = bIsEquipped;
	
	// 서버에서 소켓 변경 + 클라이언트는 OnRep_IsWeaponEquipped에서 처리
	if (WeaponMesh && GetMesh())
	{
		FName TargetSocket = bIsEquipped ? WeaponEquippedSocketName : WeaponUnequippedSocketName;
		WeaponMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TargetSocket);
	}
}

void ALostArkCharacter::OnRep_IsWeaponEquipped()
{
	// 클라이언트에서 서버로부터 복제된 값이 도착했을 때 소켓 위치 동기화
	if (WeaponMesh && GetMesh())
	{
		FName TargetSocket = bIsWeaponEquipped ? WeaponEquippedSocketName : WeaponUnequippedSocketName;
		WeaponMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TargetSocket);
	}
}

void ALostArkCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		if (HasAuthority())
		{
			if (AttributeSet)
			{
				AttributeSet->InitMaxHealth(InitialMaxHealth);
				AttributeSet->InitHealth(InitialMaxHealth);
				AttributeSet->InitMaxMana(InitialMaxMana);
				AttributeSet->InitMana(InitialMaxMana);
			}

			if (ComboAttackAbilityClass && !ComboAttackAbilityHandle.IsValid())
			{
				ComboAttackAbilityHandle = AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(ComboAttackAbilityClass, 1, 0, this));
			}

			if (JustGuardAbilityClass)
			{
				AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(JustGuardAbilityClass, 1, static_cast<int32>(ELostArkAbilityInputID::JustGuard), this));
			}

			for (const FLostArkSkillInputBind& Bind : SkillInputBinds)
			{
				if (Bind.AbilityClass)
				{
					UE_LOG(LogTemp, Warning, TEXT("[Character] GiveAbility Called for %s"), *Bind.AbilityClass->GetName());
					//AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(Bind.AbilityClass, 1, static_cast<int32>(Bind.InputID), this));

					FGameplayAbilitySpecHandle Handle = AbilitySystemComponent->GiveAbility(
						FGameplayAbilitySpec(Bind.AbilityClass, 1, static_cast<int32>(Bind.InputID), this)
					);

					if (FGameplayAbilitySpec* GrantedSpec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle))
					{
						if (ULostArkSkill_Targeting* TargetingAbility = Cast<ULostArkSkill_Targeting>(GrantedSpec->GetPrimaryInstance()))
						{
							TargetingAbility->SkillInputAction = Bind.InputAction;
						}
						else
						{
							UObject* PrimaryInstance = GrantedSpec->GetPrimaryInstance();
						}
					}
				}
			}
		}

		if (AbilitySystemComponent)
		{
			AbilitySystemComponent->RegisterGameplayTagEvent(
				FGameplayTag::RequestGameplayTag(FName("State.Attacking"), false),
				EGameplayTagEventType::NewOrRemoved
			).AddUObject(this, &ALostArkCharacter::OnAttackingTagChanged);

			AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(FGameplayTag::RequestGameplayTag(FName("Event.Player.CounterSuccess"), false)).AddUObject(this, &ALostArkCharacter::HandleCounterSuccessEvent);
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

	if (ALostArkPlayerController* PC = Cast<ALostArkPlayerController>(GetController()))
	{
		PC->InitializeHUDForCharacter(this);
	}
}

void ALostArkCharacter::ShowDamageText(float DamageAmount)
{
	// AttributeSet에서 직접 Client_ShowDamageText RPC를 호출하지만,
	// 다른 경로를 통해 호출될 경우를 대비한 fallback
	float RandomX = FMath::RandRange(-50.f, 50.f);
	float RandomY = FMath::RandRange(-50.f, 50.f);
	float RandomZ = FMath::RandRange(50.f, 150.f);
	FVector SpawnLoc = GetActorLocation() + FVector(RandomX, RandomY, RandomZ);
	Client_ShowDamageText(DamageAmount, SpawnLoc);
}

void ALostArkCharacter::Client_ShowDamageText_Implementation(float DamageAmount, FVector SpawnLocation)
{
	// 클라이언트 로컈에서만 실행 → 본인 화면에만 표시
	if (!DamageTextClass)
	{
		return;
	}

	if (ULostArkObjectPoolSubsystem* Pool = GetWorld()->GetSubsystem<ULostArkObjectPoolSubsystem>())
	{
		if (AActor* SpawnedText = Pool->AcquireActor(DamageTextClass, SpawnLocation, FRotator::ZeroRotator))
		{
			if (ALostArkDamageTextActor* TextActor = Cast<ALostArkDamageTextActor>(SpawnedText))
			{
				TextActor->SetupDamageText(DamageAmount);
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
		AbilitySystemComponent->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Dead"), false));
		AbilitySystemComponent->CancelAllAbilities();
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->StopMovementImmediately();
}

void ALostArkCharacter::Revive()
{
	// Die() 의 역연산. 체력 회복/태그 정리는 부활 시스템(UBossReviveComponent)이 담당
	if (!bIsDead)
	{
		return;
	}

	bIsDead = false;

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
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

void ALostArkCharacter::OnAttackingTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (NewCount > 0)
	{
		// ?袁る떮(?⑤벀爰? 筌욊쑴????筌앸맩???關媛?
		GetWorldTimerManager().ClearTimer(SheathWeaponTimerHandle);
		SetWeaponEquipped(true);
	}
	else
	{
		// ?袁る떮 ?ル굝利???????????뽰삂
		if (SheathWeaponTimeout > 0.f)
		{
			GetWorldTimerManager().SetTimer(SheathWeaponTimerHandle, this, &ALostArkCharacter::PlaySheathWeaponMontage, SheathWeaponTimeout, false);
		}
		else
		{
			PlaySheathWeaponMontage();
		}
	}
}

void ALostArkCharacter::PlaySheathWeaponMontage()
{
	// 서버에서 호출 → Multicast로 모든 클라이언트에 몽타주 재생 신호 전달
	Multicast_PlaySheathWeaponMontage();
}

void ALostArkCharacter::Multicast_PlaySheathWeaponMontage_Implementation()
{
	if (SheathWeaponMontage)
	{
		PlayAnimMontage(SheathWeaponMontage);
	}
	else
	{
		// 몽타주 없으면 즉시 무기 수납 상태로 전환
		SetWeaponEquipped(false);
	}
}

void ALostArkCharacter::HandleCounterSuccessEvent(const FGameplayEventData* Payload)
{
	// 蹂댁뒪 痢≪뿉??移댁슫???곸쨷???깃났?덈떎???대깽?몃? 蹂대궡二쇰㈃ UI ?곗텧 諛쒕룞
	OnCounterSuccess_UI();
}




