// Fill out your copyright notice in the Description page of Project Settings.


#include "Boss/Damage/BossAoe_Sector.h"
#include "DrawDebugHelpers.h"
#include "NiagaraComponent.h"

bool ABossAoe_Sector::IsInsideShape(const FVector& WorldPoint) const
{
	FVector ToPoint = WorldPoint - AttackCenter;
	ToPoint.Z = 0.f;

	// 거리(도넛) 체크
	const float Dist = ToPoint.Size();
	if (Dist > Radius || Dist < InnerRadius)
	{
		return false;
	}

	// 전방 기준 부호각: 오른쪽 +, 왼쪽 -
	const float F = FVector::DotProduct(ToPoint, GetShapeForward());
	const float R = FVector::DotProduct(ToPoint, GetShapeRight());
	float Angle = FMath::RadiansToDegrees(FMath::Atan2(R, F));

	// [StartAngle, StartAngle+360) 로 정규화 후 EndAngle 이하인지
	while (Angle < StartAngle)          Angle += 360.f;
	while (Angle >= StartAngle + 360.f) Angle -= 360.f;

	return Angle <= EndAngle;
}

void ABossAoe_Sector::BuildTelegraph()
{
	// 로컬 좌표(X=Forward, Y=Right). 각도 A 방향 = (cos A, sin A). 메시 자체가 부채꼴.
	TArray<FVector> Vertices;
	TArray<int32> Triangles;

	const int32 Segments = 48;

	if (InnerRadius > KINDA_SMALL_NUMBER)
	{
		// 환형 부채꼴: 안쪽 호 + 바깥 호를 스트립으로 연결
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float A = FMath::Lerp(StartAngle, EndAngle, (float)i / Segments);
			const float Rad = FMath::DegreesToRadians(A);
			const float Cos = FMath::Cos(Rad);
			const float Sin = FMath::Sin(Rad);
			Vertices.Add(FVector(Cos * InnerRadius, Sin * InnerRadius, 0.f));
			Vertices.Add(FVector(Cos * Radius, Sin * Radius, 0.f));
		}
		for (int32 i = 0; i < Segments; ++i)
		{
			const int32 i0 = i * 2, i1 = i * 2 + 1, i2 = i * 2 + 2, i3 = i * 2 + 3;
			Triangles.Add(i0); Triangles.Add(i1); Triangles.Add(i3);
			Triangles.Add(i0); Triangles.Add(i3); Triangles.Add(i2);
		}
	}
	else
	{
		// 꽉 찬 부채꼴: 중심 + 바깥 호 팬
		Vertices.Add(FVector::ZeroVector);
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float A = FMath::Lerp(StartAngle, EndAngle, (float)i / Segments);
			const float Rad = FMath::DegreesToRadians(A);
			Vertices.Add(FVector(FMath::Cos(Rad) * Radius, FMath::Sin(Rad) * Radius, 0.f));
		}
		for (int32 i = 0; i < Segments; ++i)
		{
			Triangles.Add(0); Triangles.Add(i + 1); Triangles.Add(i + 2);
		}
	}

	CreateTelegraphMesh(Vertices, Triangles);
}

void ABossAoe_Sector::DebugDrawShape() const
{
	// 판정과 동일 규약: 각도 A 방향 = Forward*cos(A) + Right*sin(A) (전방=0°, 우측 +)
	const int32 Seg = 24;
	FVector PrevOut = FVector::ZeroVector, PrevIn = FVector::ZeroVector;
	for (int32 i = 0; i <= Seg; ++i)
	{
		const float A = FMath::DegreesToRadians(FMath::Lerp(StartAngle, EndAngle, (float)i / Seg));
		const FVector Dir = GetShapeForward() * FMath::Cos(A) + GetShapeRight() * FMath::Sin(A);
		const FVector Out = AttackCenter + Dir * Radius;
		const FVector In = AttackCenter + Dir * InnerRadius;
		if (i > 0)
		{
			DrawDebugLine(GetWorld(), PrevOut, Out, FColor::Green, false, 4.f, 0, 4.f);	// 바깥 호
			if (InnerRadius > KINDA_SMALL_NUMBER)
			{
				DrawDebugLine(GetWorld(), PrevIn, In, FColor::Green, false, 4.f, 0, 4.f);	// 안쪽 호
			}
		}
		PrevOut = Out;
		PrevIn = In;
	}
	// 양 측면 경계선
	for (float A : { StartAngle, EndAngle })
	{
		const float Rad = FMath::DegreesToRadians(A);
		const FVector Dir = GetShapeForward() * FMath::Cos(Rad) + GetShapeRight() * FMath::Sin(Rad);
		DrawDebugLine(GetWorld(), AttackCenter + Dir * InnerRadius, AttackCenter + Dir * Radius,
			FColor::Green, false, 4.f, 0, 4.f);
	}
}

void ABossAoe_Sector::ConfigureTelegraphEffect(UNiagaraComponent* NiagaraComp) const
{
	if (!NiagaraComp)
	{
		return;
	}
	NiagaraComp->SetFloatParameter(TEXT("Radius"), Radius);
	NiagaraComp->SetFloatParameter(TEXT("InnerRatio"), Radius > KINDA_SMALL_NUMBER ? InnerRadius / Radius : 0.f);
	NiagaraComp->SetFloatParameter(TEXT("StartAngleDeg"), StartAngle);
	NiagaraComp->SetFloatParameter(TEXT("EndAngleDeg"), EndAngle);
}
