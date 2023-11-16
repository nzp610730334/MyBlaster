// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon.h"
#include "HitScanWeapon.generated.h"

/**
 * 本类是射线武器类
 */
UCLASS()
class MYBLASTER_API AHitScanWeapon : public AWeapon
{
	GENERATED_BODY()
public:
	// 父类的开火方法不能完全满足本类的需求，需要重写
	virtual void Fire(const FVector& HitTarget) override;

	
protected:
	// 计算本次弹丸的命中处理
	void WeaponTraceHit(const FVector& TraceStart, const FVector& HitTarget, FHitResult& OutHit);
	// 子弹命中的声音
	UPROPERTY(EditAnywhere)
	USoundCue* HitSound;
	// 武器命中时产生的粒子效果
	UPROPERTY(EditAnywhere)
	class UParticleSystem* ImpactParticles;
	
private:
	// 弹道烟雾轨迹粒子效果
	UPROPERTY(EditAnywhere)
	UParticleSystem* BeamParticles;
	// 子弹开火枪口火焰特效
	UPROPERTY(EditAnywhere)
	UParticleSystem* MuzzleFlash;
	// 子弹开火声音
	UPROPERTY(EditAnywhere)
	USoundCue* FireSound;

};
