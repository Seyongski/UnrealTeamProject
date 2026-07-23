// Fill out your copyright notice in the Description page of Project Settings.


#include "Boss/Damage/BossAoe_Rect.h"
#include "DrawDebugHelpers.h"
#include "NiagaraComponent.h"
#include "Net/UnrealNetwork.h"

void ABossAoe_Rect::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABossAoe_Rect, HalfLength);
	DOREPLIFETIME(ABossAoe_Rect, HalfWidth);
	DOREPLIFETIME(ABossAoe_Rect, ForwardOffset);
}

bool ABossAoe_Rect::IsInsideShape(const FVector& WorldPoint) const
{
	// 중심에서 대상까지 벡터를 Forward/Right 축에 투영
	FVector ToPoint = WorldPoint - AttackCenter;
	ToPoint.Z = 0.f;

	const float F = FVector::DotProduct(ToPoint, GetShapeForward());
	const float R = FVector::DotProduct(ToPoint, GetShapeRight());

	// ForwardOffset 만큼 전방으로 밀린 박스: F 범위는 [Offset-Half, Offset+Half]
	return F >= (ForwardOffset - HalfLength) && F <= (ForwardOffset + HalfLength)
		&& FMath::Abs(R) <= HalfWidth;
}

void ABossAoe_Rect::BuildTelegraph()
{
	// 로컬 좌표(X=Forward, Y=Right) 4정점 사각형
	const float FMin = ForwardOffset - HalfLength;
	const float FMax = ForwardOffset + HalfLength;

	TArray<FVector> Vertices = {
		FVector(FMin, -HalfWidth, 0.f),
		FVector(FMax, -HalfWidth, 0.f),
		FVector(FMax,  HalfWidth, 0.f),
		FVector(FMin,  HalfWidth, 0.f)
	};
	TArray<int32> Triangles = { 0, 1, 2,  0, 2, 3 };

	CreateTelegraphMesh(Vertices, Triangles);
}

void ABossAoe_Rect::DebugDrawShape() const
{
	// 판정과 동일: 중심 = AttackCenter + Forward*ForwardOffset, 반extent = (HalfLength, HalfWidth)
	const FVector Center = AttackCenter + GetShapeForward() * ForwardOffset;
	const FQuat Rot = FRotationMatrix::MakeFromXY(GetShapeForward(), GetShapeRight()).ToQuat();
	DrawDebugBox(GetWorld(), Center, FVector(HalfLength, HalfWidth, 20.f), Rot,
		FColor::Green, false, 4.f, 0, 4.f);
}

void ABossAoe_Rect::ConfigureTelegraphEffect(UNiagaraComponent* NiagaraComp) const
{
	if (!NiagaraComp)
	{
		return;
	}
	NiagaraComp->SetFloatParameter(TEXT("HalfLength"), HalfLength);
	NiagaraComp->SetFloatParameter(TEXT("HalfWidth"), HalfWidth);
	NiagaraComp->SetFloatParameter(TEXT("ForwardOffset"), ForwardOffset);
}
