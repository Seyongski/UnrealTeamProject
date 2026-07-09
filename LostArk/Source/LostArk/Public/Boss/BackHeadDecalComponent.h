// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/DecalComponent.h"
#include "BackHeadDecalComponent.generated.h"

class UMaterialInstanceDynamic;

/**
 * 보스 발밑에 백/헤드 어택 방향을 표시하는 지면 데칼 컴포넌트.
 *
 * - 캡슐 반경에 맞춰 크기가 자동 스케일된다(UpdateRadius).
 * - 뎁스 테스트를 켠 기본 데칼이라, 보스 메쉬가 앞을 가리는 부분은 자연스럽게 가려진다
 *   (지면에 먼저 깔리고 불투명 메쉬가 위에 그려지는 효과).
 * - 소유 액터에 부착만 하면 보스 회전을 따라가며 앞=헤드 / 뒤=백 방향이 정렬된다.
 */
UCLASS(ClassGroup = (Boss), meta = (BlueprintSpawnableComponent))
class LOSTARK_API UBackHeadDecalComponent : public UDecalComponent
{
	GENERATED_BODY()

public:
	UBackHeadDecalComponent();

	/** 캡슐 반경(cm)에 맞춰 데칼 풋프린트와 머티리얼 반경 파라미터를 갱신 */
	UFUNCTION(BlueprintCallable, Category = "BackHead")
	void UpdateRadius(float CapsuleRadius);

protected:
	virtual void BeginPlay() override;
	virtual void OnRegister() override;

	// 데칼 머티리얼은 부모 UDecalComponent의 "Decal Material" 슬롯을 그대로 사용한다.

	/** 지면 투영 방향(Pitch). 위로 쏘면(바닥에 안 찍히면) 부호를 반대로 바꿔볼 것 (-90 <-> +90) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BackHead")
	float ProjectionPitch = -90.f;

	/** 반경 대비 여백(cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BackHead")
	float RadiusPadding = 50.f;

	/** 지면 투영 깊이(cm). 클수록 보스 몸에 묻을 수 있으니 얇게 유지 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BackHead")
	float ProjectionDepth = 128.f;

	/** 머티리얼의 반경 스칼라 파라미터 이름 (절차적 머티리얼용, 없으면 무시됨) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BackHead")
	FName RadiusParamName = TEXT("Radius");

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DecalMID;
};
