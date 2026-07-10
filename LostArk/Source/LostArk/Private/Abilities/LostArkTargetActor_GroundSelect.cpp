#include "Abilities/LostArkTargetActor_GroundSelect.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/PlayerController.h"
#include "Components/DecalComponent.h"
#include "Kismet/GameplayStatics.h"

ALostArkTargetActor_GroundSelect::ALostArkTargetActor_GroundSelect()
{
	UE_LOG(LogTemp, Warning, TEXT("[TargetActor] Constructor Called!"));
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// 스폰 시 지형이나 캐릭터 충돌로 스폰이 실패하는 것을 방지
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	DecalComponent = CreateDefaultSubobject<UDecalComponent>(TEXT("DecalComponent"));
	DecalComponent->SetupAttachment(RootComponent);
	// Decal Size: X(Depth), Y(Width), Z(Height). 원형 데칼 기준 X가 바닥을 향하는 축
	DecalComponent->DecalSize = FVector(1000.f, 200.f, 200.f); // 깊이(Depth)를 1000으로 늘림
	DecalComponent->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f)); // 바닥을 향하도록 회전
	DecalComponent->SetVisibility(false);
}

void ALostArkTargetActor_GroundSelect::StartTargeting(UGameplayAbility* Ability)
{
	Super::StartTargeting(Ability);
	UE_LOG(LogTemp, Warning, TEXT("[TargetActor] StartTargeting Called!"));

	// 블루프린트에서 회전값이 오버라이드되어 찌그러지는 문제를 방지하기 위해 
	// 런타임에 바닥 수직 방향(-90, 0, 0)으로 회전을 강제 리셋합니다.
	DecalComponent->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));

	DecalComponent->DecalSize = FVector(1000.f, TargetRadius, TargetRadius);
	DecalComponent->SetVisibility(true);
	SetActorTickEnabled(true);

	// 조준 시작 직후 좌클릭으로 즉시 확정되는 것을 예방하는 서모 보호
	bCanConfirm = false;
	ActivatedTime = 0.f;
	bPrevLMB = true;  // 시작 시 눌린 상태로 간주하여 즉시 확정 방지
	bPrevRMB = false;
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

	// 마우스 위치로 이동
	FVector TargetLocation;
	if (GetMouseCursorLocation(TargetLocation))
	{
		SetActorLocation(TargetLocation);
	}

	// 스킬 키를 누른 직후 0.15초간 입력 감지 유예 (이동 클릭과의 충돌 방지)
	ActivatedTime += DeltaSeconds;
	if (!bCanConfirm && ActivatedTime >= 0.15f)
	{
		bCanConfirm = true;
	}
	if (!bCanConfirm) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	// 이전 프레임 상태와 비교하여 "방금 눌린" 순간만 감지 (엣지 트리거)
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
