// Fill out your copyright notice in the Description page of Project Settings.


#include "Boss/Damage/BossAoe_Sector.h"
#include "DrawDebugHelpers.h"
#include "NiagaraComponent.h"
#include "Net/UnrealNetwork.h"

void ABossAoe_Sector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABossAoe_Sector, Radius);
	DOREPLIFETIME(ABossAoe_Sector, InnerRadius);
	DOREPLIFETIME(ABossAoe_Sector, StartAngle);
	DOREPLIFETIME(ABossAoe_Sector, EndAngle);
}

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
	// 베이스 공용 아크 헬퍼 사용 (InnerRadius>0 이면 환형 부채꼴)
	CreateArcTelegraphMesh(StartAngle, EndAngle, InnerRadius, Radius);
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

bool ABossAoe_Sector::GetSweepPushDirection(bool bReverse, FVector& OutDir) const
{
	// 훑는 방향 부호: 보통 StartAngle < EndAngle 이라 각도가 커지는 쪽(=우측)이 진행 방향.
	// (설정이 뒤집혀 있어도 부호로 흡수)
	const float Sign = (EndAngle >= StartAngle) ? 1.f : -1.f;

	// 기준 경계선: 정방향이면 끝 각도, 역방향이면 시작 각도.
	// 그 경계선(반지름 방향)에 직교 = ±90도 회전 -> 부채꼴 바깥으로 밀어내는 접선 방향이 된다.
	const float BoundaryDeg = bReverse ? StartAngle : EndAngle;
	const float PushDeg = BoundaryDeg + (bReverse ? -90.f : 90.f) * Sign;

	// 판정/디버그와 동일 규약: 각도 A 방향 = Forward*cos(A) + Right*sin(A) (전방=0°, 우측 +)
	const float Rad = FMath::DegreesToRadians(PushDeg);
	FVector Dir = GetShapeForward() * FMath::Cos(Rad) + GetShapeRight() * FMath::Sin(Rad);
	Dir.Z = 0.f;
	if (!Dir.Normalize())
	{
		return false;	// 축이 퇴화 -> 베이스가 ShapeForward 로 폴백
	}

	OutDir = Dir;
	return true;
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
