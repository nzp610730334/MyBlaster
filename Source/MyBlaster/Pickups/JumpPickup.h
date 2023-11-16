// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Pickup.h"
#include "JumpPickup.generated.h"

/**
 * 本类作为可拾取道具，可提升玩家角色的跳跃能力
 */
UCLASS()
class MYBLASTER_API AJumpPickup : public APickup
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
	// 提升玩家角色的Z轴的跳跃速度的大小
	UPROPERTY(EditAnywhere)
	float JumpZVelocityBuff = 4000.f;

	// buff的持续时长
	UPROPERTY(EditAnywhere)
	float JumpBuffTime = 30.f;
};
