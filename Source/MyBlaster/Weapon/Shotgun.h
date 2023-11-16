// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HitScanWeapon.h"
#include "Shotgun.generated.h"

/**
 * 本类是霰弹枪类
 */
UCLASS()
class MYBLASTER_API AShotgun : public AHitScanWeapon
{
	GENERATED_BODY()
	
public:
	// 父类的Fire不能满足霰弹枪的需求
	// virtual void Fire11111(const FVector& HitTarget) override;
	// 父类的父类的Fire不能满足霰弹枪的需求，需要重新写一个方法作为霰弹枪的开火逻辑
	virtual void FireShotgun(const TArray<FVector_NetQuantize>& HitTargets);
	
	/*
	 * 处理生成霰弹枪一次开火中的所有弹丸的随机散布。
	 * 传入战斗组件的准星瞄准点，随机生成霰弹枪弹丸数量的随机落点，将它们加入到数组HitTargets中。
	 */ 
	void ShotgunTraceEndWithScatter(const FVector& HitTarget, TArray<FVector_NetQuantize>& HitTargets);
	
private:
	// 霰弹枪每一枪的弹丸数量
	UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
	uint32 NumberOfPellets = 12;
};
