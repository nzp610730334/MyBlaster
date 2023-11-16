// Fill out your copyright notice in the Description page of Project Settings.


#include "HitedDamageNumber.h"

#include "Components/TextBlock.h"

void UHitedDamageNumber::SetDamageText(float DamageValue)
{
	FString DamageString = FString::Printf(TEXT("%d"), FMath::CeilToInt(DamageValue));
	DamageText->SetText(FText().FromString(DamageString));
	// UE_LOG(LogTemp, Warning, TEXT("命中伤害数字设置完毕！数值是： %d"), FMath::CeilToInt(DamageValue));
}
