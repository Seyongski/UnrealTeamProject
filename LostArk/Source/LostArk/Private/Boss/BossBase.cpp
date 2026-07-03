// Fill out your copyright notice in the Description page of Project Settings.


#include "Boss/BossBase.h"
#include "Boss/BackHeadDecalComponent.h"
#include "Boss/BossAttributeSet.h"
#include "Boss/Pattern/BossPatternComponent.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"

// Sets default values
ABossBase::ABossBase()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// 백헤드 데칼: 캡슐(루트)에 부착 -> 보스 회전을 따라가며 앞=헤드 / 뒤=백 정렬
	BackHeadDecal = CreateDefaultSubobject<UBackHeadDecalComponent>(TEXT("BackHeadDecal"));
	BackHeadDecal->SetupAttachment(GetCapsuleComponent());

	// 데칼이 보스 몸에 묻지 않도록 메쉬는 데칼을 받지 않게 설정
	if (GetMesh())
	{
		GetMesh()->SetReceivesDecals(false);
	}

	// GAS: ASC + 어트리뷰트 (다수 플레이어 대상 보스이므로 이펙트 복제는 Minimal)
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UBossAttributeSet>(TEXT("AttributeSet"));

	// 패턴 흐름 브레인
	PatternComponent = CreateDefaultSubobject<UBossPatternComponent>(TEXT("PatternComponent"));
}

UAbilitySystemComponent* ABossBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ABossBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버: ActorInfo 초기화 -> 어트리뷰트 세팅 -> 체력 변화 구독
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		InitializeAttributes();

		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBossAttributeSet::GetHealthAttribute())
			.AddUObject(this, &ABossBase::OnHealthChanged);
	}
}

// Called when the game starts or when spawned
void ABossBase::BeginPlay()
{
	Super::BeginPlay();

	UpdateBackHeadDecal();

	if (AbilitySystemComponent)
	{
		if (!HasAuthority())
		{
			// 클라이언트: 태그/큐 표현을 위해 ActorInfo 초기화
			AbilitySystemComponent->InitAbilityActorInfo(this, this);
		}
		else if (PatternComponent)
		{
			// 서버: 컴포넌트들이 BeginPlay(어빌리티 부여)를 마친 뒤 전투 시작
			PatternComponent->StartCombat();
		}
	}
}

void ABossBase::InitializeAttributes()
{
	if (!AttributeSet)
	{
		return;
	}

	AttributeSet->InitMaxHealth(InitialMaxHealth);
	AttributeSet->InitHealth(InitialMaxHealth);
	AttributeSet->InitMaxStaggerGauge(InitialMaxStaggerGauge);
	AttributeSet->InitStaggerGauge(InitialMaxStaggerGauge);
}

void ABossBase::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (!AttributeSet || !PatternComponent)
	{
		return;
	}

	const float Max = AttributeSet->GetMaxHealth();
	const float Percent = (Max > 0.f) ? (Data.NewValue / Max) * 100.f : 0.f;

	// 전환은 예약만 됨(현재 패턴 완주 후 반영)
	PatternComponent->NotifyHealthPercent(Percent);
}

void ABossBase::UpdateBackHeadDecal()
{
	if (!BackHeadDecal || !GetCapsuleComponent())
	{
		return;
	}

	// 데칼을 발밑으로 내리고(캡슐 중심 기준 아래), 반경에 맞춰 크기 갱신
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	BackHeadDecal->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight));
	BackHeadDecal->UpdateRadius(GetCapsuleComponent()->GetScaledCapsuleRadius());
}

// Called every frame
void ABossBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ABossBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

