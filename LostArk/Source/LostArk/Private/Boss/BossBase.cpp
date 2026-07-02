// Fill out your copyright notice in the Description page of Project Settings.


#include "Boss/BossBase.h"
#include "Boss/BackHeadDecalComponent.h"
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
}

// Called when the game starts or when spawned
void ABossBase::BeginPlay()
{
	Super::BeginPlay();

	UpdateBackHeadDecal();
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

