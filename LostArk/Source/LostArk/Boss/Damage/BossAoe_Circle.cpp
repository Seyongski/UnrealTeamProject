// Fill out your copyright notice in the Description page of Project Settings.


#include "Boss/Damage/BossAoe_Circle.h"
#include "DrawDebugHelpers.h"
#include "NiagaraComponent.h"

bool ABossAoe_Circle::IsInsideShape(const FVector& WorldPoint) const
{
	const float Dist = FVector::Dist2D(WorldPoint, AttackCenter);
	return Dist <= Radius && Dist >= InnerRadius;
}

void ABossAoe_Circle::BuildTelegraph()
{
	// 원 = 0~360° 부채꼴. 베이스 공용 아크 헬퍼 사용 (InnerRadius>0 이면 도넛)
	CreateArcTelegraphMesh(0.f, 360.f, InnerRadius, Radius);
}

void ABossAoe_Circle::DebugDrawShape() const
{
	// 판정과 동일: AttackCenter 중심, 바깥/안쪽 반지름. 바닥 평면(우측/전방 축)에 그림
	DrawDebugCircle(GetWorld(), AttackCenter, Radius, 48, FColor::Green, false, 4.f, 0, 4.f,
		GetShapeRight(), GetShapeForward(), false);
	if (InnerRadius > KINDA_SMALL_NUMBER)
	{
		DrawDebugCircle(GetWorld(), AttackCenter, InnerRadius, 48, FColor::Yellow, false, 4.f, 0, 4.f,
			GetShapeRight(), GetShapeForward(), false);
	}
}

void ABossAoe_Circle::ConfigureTelegraphEffect(UNiagaraComponent* NiagaraComp) const
{
	if (!NiagaraComp)
	{
		return;
	}
	// 나이아가라 시스템에 같은 이름의 User 파라미터(float)를 노출해두면 반영됨
	NiagaraComp->SetFloatParameter(TEXT("Radius"), Radius);
	NiagaraComp->SetFloatParameter(TEXT("InnerRatio"), Radius > KINDA_SMALL_NUMBER ? InnerRadius / Radius : 0.f);
}
