// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/BackHeadDecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

UBackHeadDecalComponent::UBackHeadDecalComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 지면으로 투영되도록 회전. 에디터에서 방향 확인 후,
	// 위로 투영되면 Pitch 부호를 반대로(+90) 바꾸면 된다.
	SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));

	// 초기 크기 (실제 값은 BeginPlay/UpdateRadius에서 캡슐 반경으로 갱신)
	DecalSize = FVector(ProjectionDepth, 100.f, 100.f);
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

	// 회전(Pitch -90) 상태에서 X = 투영 깊이, Y/Z = 지면 풋프린트(반지름 크기)
	DecalSize = FVector(ProjectionDepth, Extent, Extent);
	MarkRenderStateDirty();

	if (DecalMID)
	{
		DecalMID->SetScalarParameterValue(RadiusParamName, Extent);
	}
}
