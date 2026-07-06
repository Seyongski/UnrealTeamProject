// Fill out your copyright notice in the Description page of Project Settings.


#include "Damage/BossAoe_Sector.h"

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
	// 로컬 좌표(X=Forward, Y=Right). 각도 A 방향 = (cos A, sin A)
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
