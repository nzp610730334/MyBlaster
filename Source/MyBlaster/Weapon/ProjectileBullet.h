// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Projectile.h"
#include "ProjectileBullet.generated.h"

/**
 * 
 */
UCLASS()
class MYBLASTER_API AProjectileBullet : public AProjectile
{
	GENERATED_BODY()
	
public:
	AProjectileBullet();

	/*
	 * 父类的InitialSpeed设置了UPROPERTY(EditAnywhere)，UE编辑器启动后，在蓝图里面修改InitialSpeed的值后点击编译，InitialSpeed改变了，
	 * 但经由InitialSpeed赋值的变量如运动组件里ProjectileMovementComponent->InitialSpeed却没变，因为ProjectileMovementComponent->InitialSpeed的赋值完成是在C++中，
	 * C++项目启动UE编辑器时就已经完成对ProjectileMovementComponent->InitialSpeed的赋值工作，后期在UE编辑器的蓝图上修改父类的InitialSpeed再点蓝图编译也不会改变ProjectileMovementComponent->InitialSpeed的值。
	 * 解决方案：使用#if WITH_EDITOR以及#endif关键字，重写父类的PostEditChangeProperty方法。
	 * 使用#if WITH_EDITOR和#endif修饰的代码，仅在UE编译器中被执行，不会被打包到游戏中。
	 */
#if WITH_EDITOR
	// 属性被更改时该方法被调用
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& Event) override;
#endif
	
protected:
	// 重写覆盖父类的碰撞方法
	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpilse, const FHitResult& Hit) override;
	// 重写父类的BeginPlay方法
	virtual void BeginPlay() override;
};
