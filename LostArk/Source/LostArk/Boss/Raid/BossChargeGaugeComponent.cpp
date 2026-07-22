// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Raid/BossChargeGaugeComponent.h"
#include "Boss/Raid/BossPlayerStatusWidget.h"
#include "Boss/Combat/BossCombatStatics.h"
#include "Boss/Damage/BossPatternActorBase.h"
#include "Boss/BossGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"

UBossChargeGaugeComponent::UBossChargeGaugeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);	// 런타임 부착이라도 Gauge 복제가 동작하게
}

void UBossChargeGaugeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UBossChargeGaugeComponent, Gauge);
}

void UBossChargeGaugeComponent::InitChargeGauge(AActor* InBoss,
	TSubclassOf<UGameplayEffect> InRedChargeEffect, TSubclassOf<UGameplayEffect> InBlueChargeEffect)
{
	Boss = InBoss;
	RedChargeEffect = InRedChargeEffect;
	BlueChargeEffect = InBlueChargeEffect;
}

void UBossChargeGaugeComponent::BeginPlay()
{
	Super::BeginPlay();

	// Red/Blue 태그 변화 감시. 복제되는 태그 컨테이너가 서버/클라 양쪽에서 동일하게 이벤트를
	// 쏘므로, 반전(FlipOwnerCharge)이든 GameMode 의 최초 랜덤 부여든 원인과 무관하게 반응한다.
	if (UAbilitySystemComponent* ASC = GetOwnerASC())
	{
		ASC->RegisterGameplayTagEvent(LostArkTags::State_Charge_Red, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UBossChargeGaugeComponent::HandleChargeTagChanged);
		ASC->RegisterGameplayTagEvent(LostArkTags::State_Charge_Blue, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UBossChargeGaugeComponent::HandleChargeTagChanged);

		// 부착 시점에 이미 전하가 부여돼 있으면(조우 시작 순서상 항상 그렇다) 즉시 1회 방송
		// -> BeginPlay 이후 생성되는 위젯도 Construct 에서 IsRedCharge() 로 초기값을 놓치지 않음
		if (ASC->HasMatchingGameplayTag(LostArkTags::State_Charge_Red) ||
			ASC->HasMatchingGameplayTag(LostArkTags::State_Charge_Blue))
		{
			OnChargeSideChanged.Broadcast(IsRedCharge());
		}
	}

	// UI 위젯 생성 (렌더링 머신만). 이 컴포넌트가 모든 플레이어 폰에 붙으므로
	// 캐릭터 BP 종류와 무관하게 전원이 동일한 전하 아이콘/게이지를 받는다.
	SetupStatusWidgets();
}

void UBossChargeGaugeComponent::SetupStatusWidgets()
{
	// 데디케이티드 서버는 그릴 필요 없음. 리슨서버/클라만 생성
	if (!GetWorld() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!OverheadIconComp && OverheadIconWidgetClass)
	{
		OverheadIconComp = CreateStatusWidget(
			OverheadIconWidgetClass, OverheadIconZOffset, OverheadIconDrawSize, TEXT("ChargeOverheadIcon"));
	}
	if (!FootGaugeComp && FootGaugeWidgetClass)
	{
		FootGaugeComp = CreateStatusWidget(
			FootGaugeWidgetClass, FootGaugeZOffset, FootGaugeDrawSize, TEXT("ChargeFootGauge"));
	}
}

UWidgetComponent* UBossChargeGaugeComponent::CreateStatusWidget(TSubclassOf<UUserWidget> WidgetClass,
	float ZOffset, const FVector2D& DrawSize, const TCHAR* CompName)
{
	AActor* Owner = GetOwner();
	USceneComponent* Attach = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Attach)
	{
		return nullptr;
	}

	UWidgetComponent* WC = NewObject<UWidgetComponent>(Owner, UWidgetComponent::StaticClass(), CompName);
	WC->SetWidgetSpace(EWidgetSpace::Screen);	// 화면공간 = 항상 카메라를 향하고 크기 일정 (빌보드 코드 불필요)
	WC->SetWidgetClass(WidgetClass);
	WC->SetDrawSize(DrawSize);
	WC->SetDrawAtDesiredSize(false);
	WC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WC->SetGenerateOverlapEvents(false);
	WC->AttachToComponent(Attach, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	WC->SetRelativeLocation(FVector(0.f, 0.f, ZOffset));
	WC->RegisterComponent();

	// 위젯 인스턴스를 강제 생성한 뒤 이 컴포넌트 포인터를 주입 (WBP 가 자기 폰을 못 찾는 문제 해결)
	WC->InitWidget();
	if (UBossPlayerStatusWidget* StatusWidget = Cast<UBossPlayerStatusWidget>(WC->GetUserWidgetObject()))
	{
		StatusWidget->InitStatus(this);
	}
	return WC;
}

void UBossChargeGaugeComponent::HandleChargeTagChanged(const FGameplayTag /*Tag*/, int32 /*NewCount*/)
{
	OnChargeSideChanged.Broadcast(IsRedCharge());
}

bool UBossChargeGaugeComponent::IsRedCharge() const
{
	const UAbilitySystemComponent* ASC = GetOwnerASC();
	return ASC && ASC->HasMatchingGameplayTag(LostArkTags::State_Charge_Red);
}

UAbilitySystemComponent* UBossChargeGaugeComponent::GetOwnerASC() const
{
	return GetOwner() ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()) : nullptr;
}

void UBossChargeGaugeComponent::AddGauge(float Amount)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	const float Old = Gauge;
	Gauge = FMath::Clamp(Old + Amount, 0.f, MaxGauge);
	if (Gauge == Old)
	{
		return;
	}
	OnChargeGaugeChanged.Broadcast(Gauge, MaxGauge);	// 클라는 OnRep 에서 방송

	// 절반 '상향 돌파' 1회: 전하 반전 (내려갔다 다시 넘으면 또 반전 — 게이지 감소 수단이 생겨도 일관)
	const float Half = MaxGauge * 0.5f;
	if (Old < Half && Gauge >= Half)
	{
		FlipOwnerCharge();
	}

	// 가득 차는 순간: 과충전 폭발. 게이지는 유지(기본) — 이미 가득이면 재발동 없음
	if (Old < MaxGauge && Gauge >= MaxGauge)
	{
		TriggerOvercharge();
		if (bResetGaugeAfterOvercharge)
		{
			Gauge = 0.f;
			OnChargeGaugeChanged.Broadcast(Gauge, MaxGauge);
		}
	}
}

void UBossChargeGaugeComponent::ResetGauge()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || Gauge == 0.f)
	{
		return;
	}
	Gauge = 0.f;
	OnChargeGaugeChanged.Broadcast(Gauge, MaxGauge);
}

void UBossChargeGaugeComponent::FlipOwnerCharge()
{
	// 전하 변환장판(UBossAoeChargeSwapEffect)과 동일한 공용 로직: 복제 루스 태그 반전 (GE 애셋 불필요)
	UBossCombatStatics::FlipChargeLoose(GetOwnerASC(),
		LostArkTags::State_Charge_Red.GetTag(), LostArkTags::State_Charge_Blue.GetTag());
}

void UBossChargeGaugeComponent::TriggerOvercharge()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || !OverchargeAoeClass)
	{
		return;
	}

	// 이 캐릭터 위치에서 스폰. 추적(FollowTarget)/예고/반경은 장판 BP 설정이 담당.
	// 시전자=보스(데미지 귀속), 타겟=과충전된 이 플레이어 (FollowTarget 이 이 폰을 따라감)
	const FTransform SpawnTM(Owner->GetActorRotation(), Owner->GetActorLocation());
	ABossPatternActorBase::SpawnAoeDeferred(World, OverchargeAoeClass, SpawnTM,
		/*SpawnOwner=*/Boss.Get(), /*Caster=*/Boss.Get(), Owner, OverchargeDamageCoefficient);
}

void UBossChargeGaugeComponent::OnRep_Gauge()
{
	OnChargeGaugeChanged.Broadcast(Gauge, MaxGauge);
}
