// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossArenaCamera.generated.h"

class UCameraComponent;

/**
 * 보스 레이드 전용 카메라.
 * 기존 템플릿 탑다운 '각도(피치)'는 고정 유지하고, 요(yaw)만 회전시켜
 * 항상 "카메라 - 플레이어 - 보스" 가 화면상 세로로 정렬되게 한다.
 * (플레이어가 보스 오른쪽으로 이동하면 카메라도 궤도로 돌아 보스가 다시 화면 위에 온다)
 *
 * 네트워크:
 *  - 서버(GameMode)가 스폰하고 각 PC의 ViewTarget 으로 지정 (복제로 클라에 존재)
 *  - 위치/회전 계산은 각 클라이언트가 '자기 로컬 플레이어' 기준으로 매 틱 수행
 *    (움직임 복제 안 함 -> 클라마다 자기 시점)
 */
UCLASS()
class LOSTARK_API ABossArenaCamera : public AActor
{
	GENERATED_BODY()

public:
	ABossArenaCamera();

	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 스폰 직후 GameMode 가 호출 (복제됨) */
	void SetBoss(AActor* InBoss) { Boss = InBoss; }

	/** 탑다운 내려보는 각도(도). 음수=아래. 기존 템플릿 카메라 피치에 맞춰 조정 */
	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "-89.0", ClampMax = "0.0"))
	float Pitch = -55.f;

	/** 붐 길이(포커스 지점 -> 카메라 거리, cm). 클수록 멀리서 봄 */
	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "0.0"))
	float BoomLength = 900.f;

	/** 포커스 지점 높이 오프셋(cm). 플레이어를 화면 아래쪽에 두려면 올림 */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float FocusHeightOffset = 100.f;

	/** 포커스를 보스 쪽으로 당기는 비율 0~1 (0=플레이어 중심, 커질수록 보스가 화면에 더 들어옴) */
	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float FocusBiasToBoss = 0.25f;

	/** 위치/회전 보간 속도 (클수록 즉각적) */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float LocationInterpSpeed = 7.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float RotationInterpSpeed = 7.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	/** 기준 보스 (서버가 세팅, 복제) */
	UPROPERTY(Replicated, Transient)
	TObjectPtr<AActor> Boss;

private:
	/** 플레이어가 보스와 겹칠 때 쓸 직전 방위각(yaw, 도) */
	float LastYaw = 0.f;

	/** 첫 유효 틱에 보간 없이 스냅 */
	bool bSnapped = false;
};
