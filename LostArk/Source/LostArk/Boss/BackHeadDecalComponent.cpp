// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/BackHeadDecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

UBackHeadDecalComponent::UBackHeadDecalComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 초기 크기 (실제 값은 BeginPlay/UpdateRadius에서 캡슐 반경으로 갱신)
	DecalSize = FVector(ProjectionDepth, 100.f, 100.f);
}

void UBackHeadDecalComponent::OnRegister()
{
	Super::OnRegister();

	// 지면 투영 방향 적용. ProjectionPitch 를 에디터에서 바꾸면 재등록되며 즉시 반영됨
	SetRelativeRotation(FRotator(ProjectionPitch, 0.f, 0.f));
}

void UBackHeadDecalComponent::BeginPlay()
{
	Super::BeginPlay();

	// 부모의 Decal Material 슬롯에 머티리얼이 지정돼 있으면,
	// 반경 파라미터를 런타임에 바꾸기 위해 MID 생성
	if (GetDecalMaterial())
	{
		DecalMID = CreateDynamicMaterialInstance();
	}
}

void UBackHeadDecalComponent::UpdateRadius(float CapsuleRadius)
{
	const float Extent = CapsuleRadius + RadiusPadding;

	// 회전(Pitch -90) 상태에서 X = 투영 깊이, Y/Z = 지면 풋프린트(반지름 크기).
	// 깊이는 최소 128 보장 — 0 이면 데칼이 아예 안 그려진다(실수로 0 세팅 방어).
	const float Depth = FMath::Max(ProjectionDepth, 128.f);
	DecalSize = FVector(Depth, Extent, Extent);
	MarkRenderStateDirty();

	if (DecalMID)
	{
		DecalMID->SetScalarParameterValue(RadiusParamName, Extent);
	}
}
