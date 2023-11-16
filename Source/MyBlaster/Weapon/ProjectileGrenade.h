// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Projectile.h"
#include "ProjectileGrenade.generated.h"

/**
 * 
 */
UCLASS()
class MYBLASTER_API AProjectileGrenade : public AProjectile
{
	GENERATED_BODY()
public:
	AProjectileGrenade();
	// 重写父类的方法
	virtual void BeginPlay() override;

	// 父类的Destroy无法满足本子类的需求
	virtual void Destroyed() override;
	
	// 弹体发生碰撞，反弹时执行的方法
	UFUNCTION()
	void OnBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

private:
	// 反弹时要播放的声音
	UPROPERTY(EditAnywhere)
	USoundCue* BounceCue;
};
