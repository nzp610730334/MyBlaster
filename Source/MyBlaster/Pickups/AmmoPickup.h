// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Pickup.h"
#include "MyBlaster/Weapon/WeaponTypes.h"
#include "AmmoPickup.generated.h"

/**
 * 本类是可拾取道具-弹药
 */
UCLASS()
class MYBLASTER_API AAmmoPickup : public APickup
{
	GENERATED_BODY()

protected:
	/*
	 * 父类APick的碰撞重叠函数为空，因此这里要重写，但父类的碰撞函数已经使用了UFUNCTION处理，这里就不用了。
	 * 疑惑：为什么这里重写不需要使用override
	 */ 
	virtual void OnSphereOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
		);

private:
	// 本道具的弹药数量
	UPROPERTY(EditAnywhere)
	int32 AmmoAmount = 30;
	// 本道具所属弹药类型
	UPROPERTY(EditAnywhere)
	EWeaponType WeaponType;
};
