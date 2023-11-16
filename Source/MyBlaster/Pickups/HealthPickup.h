// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Pickup.h"
#include "HealthPickup.generated.h"

/**
 * 本类是血量恢复的道具，拾取后缓慢恢复血量
 */
UCLASS()
class MYBLASTER_API AHealthPickup : public APickup
{
	GENERATED_BODY()

public:
	AHealthPickup();

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
	// 本道具恢复的血量
	UPROPERTY(EditAnywhere)
	float HealAmount = 100.f;

	// 血量恢复时长
	UPROPERTY(EditAnywhere)
	float HealingTime = 5.f;

};
