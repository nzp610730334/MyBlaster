// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Pickup.h"
#include "SpeedPickup.generated.h"

/**
 * 
 */
UCLASS()
class MYBLASTER_API ASpeedPickup : public APickup
{
	GENERATED_BODY()
protected:
	// 父类Pickup的方法不能满足要求，需要重写
	virtual void OnSphereOverlap(
		// 本对象的检测球体
		UPrimitiveComponent* OverlappedComponent,
		// 与碰撞球体发生碰撞的其他Actor
		AActor* OtherActor,
		// 与碰撞球体发生碰撞的其他Actor的组件
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		// 碰撞结果
		const FHitResult& SweepResult);

private:
	// 本道具提供的提升基础行走速度的增益的数值
	UPROPERTY(EditAnywhere)
	float BaseSpeedBuff = 1600.f;
	// 本道具提供的提升蹲伏行走速度的增益的数值
	UPROPERTY(EditAnywhere)
	float CrouchSpeedBuff = 850.f;
	// 增益持续时长
	UPROPERTY(EditAnywhere)
	float SpeedBuffTime = 30.f;
};
