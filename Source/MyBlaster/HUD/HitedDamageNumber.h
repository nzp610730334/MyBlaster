// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
// #include "Components/TextBlock.h"
#include "HitedDamageNumber.generated.h"

/**
 * 
 */
UCLASS()
class MYBLASTER_API UHitedDamageNumber : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* DamageText;

	// UPROPERTY(EditAnywhere)
	// float DamageValue;
	//
	// UPROPERTY(EditAnywhere)
	// bool bCriticalStrike;

	UPROPERTY(meta = (BindWidgetAnim), Transient)
	class UWidgetAnimation* NormalDamage;

	UPROPERTY(meta = (BindWidgetAnim), Transient)
	class UWidgetAnimation* CriticalStrikeDamage;

	void SetDamageText(float DamageValue);
};
