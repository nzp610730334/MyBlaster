// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon.h"
#include "ProjectileWeapon.generated.h"

/**
 * 
 */
UCLASS()
class MYBLASTER_API AProjectileWeapon : public AWeapon
{
	GENERATED_BODY()

public:
	// override声明时对父类的Fire(const FVector& HitTarget)的覆写
	virtual void Fire(const FVector& HitTarget) override;
	
private:
	// 该武器开火所产生的投射物
	UPROPERTY(EditAnywhere)
	TSubclassOf<class AProjectile> ProjectileClass;

	// 该武器开火所产生的投射物，该投射物仅在本地机器上产生，并不进行网络复制更新
	UPROPERTY(EditAnywhere)
	TSubclassOf<class AProjectile> ServerSideRewindProjectileClass;
};
