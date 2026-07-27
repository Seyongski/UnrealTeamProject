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
 *
 * 가림 방지 규약 (백헤드는 항상 최상단으로 보여야 함):
 *  - 다른 데칼(장판 예고 데칼 / 플레이어 지면 타겟 데칼)보다 위 => SortOrder 를 크게 준다.
 *    데칼끼리는 SortOrder 가 큰 쪽이 나중에 그려져 이긴다.
 *  - 플레이어가 데칼 위에 서도 몸이 물들면 안 된다 => 캐릭터 메시가 데칼을 안 받게 한다
 *    (ALostArkCharacter 생성자의 SetReceivesDecals(false). 보스 메시도 같은 이유로 꺼져 있다).
 *  - 주의: 디퍼드 데칼은 '불투명 지오메트리'에만 투영된다. 위에 겹치는 장판 예고 메시가
 *    Translucent 머티리얼이면 데칼보다 무조건 나중에 그려져 가려진다 -> 그 경우
 *    예고 머티리얼을 Opaque/Masked 로 바꿔야 이 데칼이 그 위에 얹힌다.
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

	/**
	 * 데칼 정렬 순서. 다른 데칼(장판 예고/지면 타겟팅)보다 확실히 위에 오도록 크게 잡는다.
	 * 생성자에서 부모의 SortOrder 에 반영되며, 값을 바꾸면 재등록 시(OnRegister) 다시 적용된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BackHead")
	int32 DecalSortOrder = 1000;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DecalMID;
};
