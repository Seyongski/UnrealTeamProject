#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LostArkShadowClone.generated.h"

UCLASS()
class LOSTARK_API ALostArkShadowClone : public AActor
{
	GENERATED_BODY()
	
public:	
	ALostArkShadowClone();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USkeletalMeshComponent* MeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadow")
	class UMaterialInterface* ShadowMaterial;

	void InitShadow(class USkeletalMeshComponent* SourceMeshComp, class UAnimMontage* MontageToPlay, float Rate = 1.0f);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnMontageEnded(class UAnimMontage* Montage, bool bInterrupted);

	FTimerHandle SelfDestructTimerHandle;

	UFUNCTION()
	void SelfDestruct();
};
