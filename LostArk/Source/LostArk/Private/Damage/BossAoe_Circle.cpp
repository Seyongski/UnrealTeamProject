// Fill out your copyright notice in the Description page of Project Settings.


#include "Damage/BossAoe_Circle.h"

bool ABossAoe_Circle::IsInsideShape(const FVector& WorldPoint) const
{
	const float Dist = FVector::Dist2D(WorldPoint, AttackCenter);
	return Dist <= Radius && Dist >= InnerRadius;
}

void ABossAoe_Circle::BuildTelegraph()
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;

	const int32 Segments = 48;

	if (InnerRadius > KINDA_SMALL_NUMBER)
	{
		// 도넛: 안쪽 링 + 바깥 링을 스트립으로 연결
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float Rad = ((float)i / Segments) * 2.f * PI;
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
		// 꽉 찬 원: 중심 팬
		Vertices.Add(FVector::ZeroVector);
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float Rad = ((float)i / Segments) * 2.f * PI;
			Vertices.Add(FVector(FMath::Cos(Rad) * Radius, FMath::Sin(Rad) * Radius, 0.f));
		}
		for (int32 i = 0; i < Segments; ++i)
		{
			Triangles.Add(0); Triangles.Add(i + 1); Triangles.Add(i + 2);
		}
	}

	CreateTelegraphMesh(Vertices, Triangles);
}
