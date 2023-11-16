// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Pickup.h"
#include "ShieldPickup.generated.h"

/**
 * 
 */
UCLASS()
class MYBLASTER_API AShieldPickup : public APickup
{
	GENERATED_BODY()

protected:
	// 父类Pickup的方法不能满足要求，需要重写
	virtual void OnSphereOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
	
private:
	// 本道具补充的护盾值
	UPROPERTY(EditAnywhere)
	float ShieldReplenishAmount = 100.f;

	// 护盾补充buff的时长
	UPROPERTY(EditAnywhere)
	float ShieldReplenishTime = 5.f;

};
