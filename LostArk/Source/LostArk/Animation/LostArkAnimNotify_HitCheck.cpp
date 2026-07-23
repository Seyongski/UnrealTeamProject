#include "Animation/LostArkAnimNotify_HitCheck.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Actor/LostArkShadowClone.h"

void ULostArkAnimNotify_HitCheck::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::Notify(MeshComp, Animation);

	if (!MeshComp) return;

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	if (ALostArkShadowClone* Clone = Cast<ALostArkShadowClone>(Owner))
	{
		Clone->ApplyCloneDamage(Clone->GetActorLocation(), Clone->GetActorRotation());
		return;
	}

	FGameplayEventData EventData;
	EventData.EventMagnitude = 1.f;
	EventData.Instigator = Owner;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		Owner,
		FGameplayTag::RequestGameplayTag(FName("Gameplay.Event.HitCheck"), false),
		EventData
	);
}

FString ULostArkAnimNotify_HitCheck::GetNotifyName_Implementation() const
{
	return TEXT("HitCheck");
}




