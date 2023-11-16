// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Projectile.generated.h"

UCLASS()
class MYBLASTER_API AProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AProjectile();
	virtual void Tick(float DeltaTime) override;

	// 重写父类的Destroyed()函数
	virtual void Destroyed() override;

	/*
	 * Used with server-side rewind
	 * 用于服务器倒带的部分
	 */
	// 是否开启服务器倒带
	bool bUseServerSideRewind = false;
	// 投射物起始点。FVector_NetQuantize经过了优化，比FVector小，便于网络传输。
	FVector_NetQuantize TraceStart;
	// 投射物的速度。FVector_NetQuantize100保留两位小数，但仍比FVector小。
	FVector_NetQuantize100 InitialVelocity;
	
	// 投射物的初始速度。用于初始化本类(投射物)的运动组件的初始速度和最大速度。
	UPROPERTY(EditAnywhere)
	float InitialSpeed = 15000.f;
	
	// 子弹(投射物)的伤害。需要暴露给蓝图，在蓝图中可设置子弹伤害
	UPROPERTY(EditAnywhere)		// 子弹伤害现已改为使用武器伤害覆盖，但手榴弹、火箭弹的伤害仍旧使用投射物自己的伤害属性作为爆炸伤害
	float Damage = 20.f;

	// 子弹(投射物)的爆头伤害。子弹伤害现已改为使用武器伤害覆盖，火箭和手榴弹则没有爆头伤害。
	UPROPERTY(EditAnywhere)
	float HeadShotDamage = 40.f;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 碰撞
	UFUNCTION()
	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpilse, const FHitResult& Hit);
	
	// 弹体造成的范围伤害(如果该弹体没有范围伤害则无需调用)
	void ExplodeDamage();
	
	// 创建计时器定时执行销毁弹体的操作
	void StartDestroyTimer();
	
	// 用于计时销毁弹体的计时器，到点时需要执行的回调函数
	void DestroyTimerFinished();

	/*
	* OnHit1用于学习课程时探究绑定委托时，通过不传参数，看编译器报错，从报错信息中查看传参的函数的参数的个数和类型的要求。
	* 已探明的个数和类型的要求如下
	* UPrimitiveComponent*, AActor*, UPrimitiveComponent*, UE::Math::TVector<double>, const FHitResult&
	* 因此OnHit函数需要有对应的形参，OnHit1不符合要求。
	*/
	UFUNCTION()
	virtual void OnHit1();

	// 粒子系统，子弹撞击相关
	UPROPERTY(EditAnywhere)
	UParticleSystem* ImpactParticles;

	// 声音，子弹撞击相关
	UPROPERTY(EditAnywhere)
	class USoundCue* ImpactSound;
	// 碰撞盒子
	UPROPERTY(EditAnywhere)
	class UBoxComponent* CollisionBox;

	// 子弹的焰尾效果(比如弹体是火箭弹时会用到，因为火箭弹需要有焰尾)
	UPROPERTY(EditAnywhere)
	class UNiagaraSystem* TrailSyStem;
	// 用于存储生成的Niagara视觉效果
	UPROPERTY()
	class UNiagaraComponent* TrailSystemComponent;

	// 生成尾焰的方法
	void SpawnTrailSystem();
	
	// 运动组件，它会被自动网络复制
	UPROPERTY(VisibleAnywhere)
	class UProjectileMovementComponent* ProjectileMovementComponent;
	
	// 弹体的静态网格体组件(原本只用于火箭弹的弹体)，在需要使用的子类中才创建使用。
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* ProjectMesh;

	// 范围伤害内圈大小
	UPROPERTY(EditAnywhere)
	float DamageInnerRadius = 200.f;
	// 范围伤害外圈圈大小
	UPROPERTY(EditAnywhere)
	float DamageOuterRadius = 500.f;

private:
	// 粒子系统，子弹轨迹相关
	UPROPERTY(EditAnywhere)
	class UParticleSystem* Tracer;
	
	// 粒子系统组件
	class UParticleSystemComponent* TracerComponent;
	
	// 计时器，处理火箭弹烟雾轨迹消散(弹体销毁)。榴弹的销毁。
	FTimerHandle DestroyTimer;

	// 销毁延迟。(用于火箭弹的烟雾轨迹消散时长以及网格体的销毁延迟。也用于榴弹的销毁延迟)
	UPROPERTY(EditAnywhere)
	float DestroyTime = 3.f;

public:
	FORCEINLINE UBoxComponent* GetCollisionBox() const { return CollisionBox; };
};
