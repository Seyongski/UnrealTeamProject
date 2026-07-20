#include "LostArk/UI/LostArkDamageTextActor.h"
#include "Components/WidgetComponent.h"
#include "LostArk/System/LostArkObjectPoolSubsystem.h"

ALostArkDamageTextActor::ALostArkDamageTextActor()
{
	PrimaryActorTick.bCanEverTick = false;

	DamageTextWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("DamageTextWidgetComponent"));
	RootComponent = DamageTextWidgetComponent;

	// ?ㅽ겕由??ㅽ럹?댁뒪(Screen Space) 紐⑤뱶濡??ㅼ젙?섏뿬 UI 罹붾쾭?ㅼ뿉 吏곸젒 ?뚮뜑留곷릺寃???	DamageTextWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	DamageTextWidgetComponent->SetDrawSize(FVector2D(200.f, 50.f));
}

void ALostArkDamageTextActor::BeginPlay()
{
	Super::BeginPlay();
}

void ALostArkDamageTextActor::OnAcquiredFromPool_Implementation()
{
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
}

void ALostArkDamageTextActor::OnReleasedToPool_Implementation()
{
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
}

void ALostArkDamageTextActor::ReturnToPool()
{
	if (UWorld* World = GetWorld())
	{
		if (ULostArkObjectPoolSubsystem* PoolSubsystem = World->GetSubsystem<ULostArkObjectPoolSubsystem>())
		{
			PoolSubsystem->ReleaseActor(this);
		}
	}
}

