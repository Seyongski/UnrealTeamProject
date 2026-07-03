#include "LostArkShadowClone.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "Engine/World.h"

ALostArkShadowClone::ALostArkShadowClone()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;
	
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCollisionProfileName(TEXT("NoCollision"));
	MeshComponent->SetCastShadow(true);

	ShadowMaterial = nullptr;
}

void ALostArkShadowClone::BeginPlay()
{
	Super::BeginPlay();
}

void ALostArkShadowClone::InitShadow(USkeletalMeshComponent* SourceMeshComp, UAnimMontage* MontageToPlay, float Rate)
{
	if (!SourceMeshComp) return;

	// 시전자의 스켈레탈 메쉬 에셋 및 애니메이션 블루프린트 복사 (스켈레톤과 본 매칭)
	MeshComponent->SetSkeletalMesh(SourceMeshComp->GetSkeletalMeshAsset());
	MeshComponent->SetAnimClass(SourceMeshComp->GetAnimClass());
	
	// 에디터에서 설정한 그림자 전용 머티리얼이 있다면 모든 슬롯에 덮어씌움 (디폴트 머티리얼 리셋 방지)
	if (ShadowMaterial)
	{
		const int32 NumMaterials = MeshComponent->GetNumMaterials();
		for (int32 i = 0; i < NumMaterials; ++i)
		{
			MeshComponent->SetMaterial(i, ShadowMaterial);
		}
	}

	UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance();
	if (AnimInstance && MontageToPlay)
	{
		float Length = AnimInstance->Montage_Play(MontageToPlay, Rate);
		if (Length > 0.f)
		{
			// 몽타주가 끝날 때 소멸시키기 위한 델리게이트 바인딩
			FOnMontageEnded EndedDelegate;
			EndedDelegate.BindUObject(this, &ALostArkShadowClone::OnMontageEnded);
			AnimInstance->Montage_SetEndDelegate(EndedDelegate, MontageToPlay);

			// 안전장치 타이머: 애니메이션 타임아웃 이후 자동 파괴
			GetWorldTimerManager().SetTimer(
				SelfDestructTimerHandle,
				this,
				&ALostArkShadowClone::SelfDestruct,
				Length + 0.2f,
				false
			);
		}
		else
		{
			Destroy();
		}
	}
	else
	{
		Destroy();
	}
}

void ALostArkShadowClone::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	SelfDestruct();
}

void ALostArkShadowClone::SelfDestruct()
{
	GetWorldTimerManager().ClearTimer(SelfDestructTimerHandle);
	Destroy();
}
