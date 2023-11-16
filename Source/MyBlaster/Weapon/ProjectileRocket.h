// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Projectile.h"
#include "ProjectileRocket.generated.h"

/**
 * 
 */
UCLASS()
class MYBLASTER_API AProjectileRocket : public AProjectile
{
	GENERATED_BODY()
public:
	AProjectileRocket();
	// 父类实体子弹的销毁函数不能满足当前子类火箭弹的需要，这里将其重写
	virtual void Destroyed() override;

	/*
	 * 父类的InitialSpeed设置了UPROPERTY(EditAnywhere)，UE编辑器启动后，在蓝图里面修改InitialSpeed的值后点击编译，InitialSpeed改变了，
	 * 但经由InitialSpeed赋值的变量如运动组件里ProjectileMovementComponent->InitialSpeed却没变，因为ProjectileMovementComponent->InitialSpeed的赋值完成是在C++中，
	 * C++项目启动UE编辑器时就已经完成对ProjectileMovementComponent->InitialSpeed的赋值工作，后期在UE编辑器的蓝图上修改父类的InitialSpeed再点蓝图编译也不会改变ProjectileMovementComponent->InitialSpeed的值。
	 * 解决方案：使用#if WITH_EDITOR以及#endif关键字，重写父类的PostEditChangeProperty方法。
	 * 使用#if WITH_EDITOR和#endif修饰的代码，仅在UE编译器中被执行，不会被打包到游戏中。
	 */
#if WITH_EDITOR
	// 属性被更改时该方法被调用
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
	
protected:
	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpilse, const FHitResult& Hit) override;

	virtual void BeginPlay() override;
	
	// 火箭弹在空中时循环播放的声音资产。因为不可能模拟得了火箭弹实时摩擦空气的变化的声音
	UPROPERTY(EditAnywhere)
	class USoundCue* ProjectileLoop;

	// 存储生成的火箭弹在空中的声音
	UPROPERTY()
	class UAudioComponent* ProjectileLoopComponent;

	// Loop循环声音的衰减配置
	UPROPERTY(EditAnywhere)
	USoundAttenuation* LoopingSoundAttenuation;

	// 火箭弹弹体的移动组件
	UPROPERTY(VisibleAnywhere)
	class URocketMovementComponent* RocketMovementComponent;
	
private:
};
