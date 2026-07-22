// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Raid/BossPlayerStatusWidget.h"

void UBossPlayerStatusWidget::InitStatus(UBossChargeGaugeComponent* InChargeComponent)
{
	ChargeComponent = InChargeComponent;
	OnStatusInitialized();
}
