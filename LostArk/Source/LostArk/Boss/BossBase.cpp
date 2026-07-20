// Fill out your copyright notice in the Description page of Project Settings.


#include "Boss/BossBase.h"
#include "Boss/BackHeadDecalComponent.h"
#include "Boss/BossAttributeSet.h"
#include "Boss/Pattern/BossPatternComponent.h"
#include "Boss/Targeting/BossTargetingComponent.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"

// Sets default values
ABossBase::ABossBase()
{
	// 기본 틱은 꺼두고(성능), 디버그 표시가 필요할 때만 BeginPlay에서 켠다
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 백헤드 데칼: 캡슐(루트)에 부착 -> 보스 회전을 따라가며 앞=헤드 / 뒤=백 정렬
	BackHeadDecal = CreateDefaultSubobject<UBackHeadDecalComponent>(TEXT("BackHeadDecal"));
	BackHeadDecal->SetupAttachment(GetCapsuleComponent());

	// 데칼이 보스 몸에 묻지 않도록 메쉬는 데칼을 받지 않게 설정
	if (GetMesh())
	{
		GetMesh()->SetReceivesDecals(false);

		// 데디서버에서도 몽타주/노티파이/루트모션이 동작하도록 항상 포즈 틱
		// (기본값 OnlyTickPoseWhenRendered 면 서버에서 안 렌더 -> 노티파이 안 불림)
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	// GAS: ASC + 어트리뷰트 (다수 플레이어 대상 보스이므로 이펙트 복제는 Minimal)
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UBossAttributeSet>(TEXT("AttributeSet"));

	// 패턴 흐름 브레인
	PatternComponent = CreateDefaultSubobject<UBossPatternComponent>(TEXT("PatternComponent"));

	// 타겟 선정 + 추적 회전
	TargetingComponent = CreateDefaultSubobject<UBossTargetingComponent>(TEXT("TargetingComponent"));
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

		// 재빙의 시 어트리뷰트 리셋/중복 바인딩/전투 재시작 방지
		if (!bGASInitialized)
		{
			bGASInitialized = true;

			InitializeAttributes();

			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBossAttributeSet::GetHealthAttribute())
				.AddUObject(this, &ABossBase::OnHealthChanged);

			// ASC 초기화가 끝난 지금 시점에 어빌리티 부여 + 전투 시작
			// (전투 시작은 임시로 여기서. 이후 어그로/트리거 시점으로 옮기면 됨)
			if (PatternComponent)
			{
				PatternComponent->InitializePatterns();
				PatternComponent->StartCombat();
			}
		}
	}
}

// Called when the game starts or when spawned
void ABossBase::BeginPlay()
{
	Super::BeginPlay();

	UpdateBackHeadDecal();

	// 회전 검증 디버그가 켜져 있을 때만 틱 활성화
	SetActorTickEnabled(bDrawFacingDebug);

	// 클라이언트: 태그/큐 표현을 위해 ActorInfo 초기화 (서버는 PossessedBy에서 처리)
	if (AbilitySystemComponent && !HasAuthority())
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void ABossBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bDrawFacingDebug)
	{
		return;
	}

	// 캡슐(=액터, 루트) forward 를 화살표로. 캡슐이 실제로 얼마나 돌았는지 실시간 확인용
	const FVector Origin = GetActorLocation();
	const float Length = (GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 50.f) + 150.f;
	DrawDebugDirectionalArrow(GetWorld(), Origin, Origin + GetActorForwardVector() * Length,
		40.f, FColor::Yellow, false, -1.f, 0, 4.f);
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


