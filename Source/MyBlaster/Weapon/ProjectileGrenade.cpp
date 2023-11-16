// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectileGrenade.h"

#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"

AProjectileGrenade::AProjectileGrenade()
{
	// 创建弹体网格体
	ProjectMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Grenade Mesh"));
	// 附着到根组件
	ProjectMesh->SetupAttachment(RootComponent);
	// 该网格体暂时不需要碰撞，因此将其关闭
	ProjectMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	/*
	 * 当前存在的BUG：手榴弹生成时，有时候可能与手部网格体有碰撞，导致投掷方向不准确。
	 * 解决方案：
	 * 1.手榴弹的蓝图中，碰撞盒子取消对骨架网格体的碰撞响应，这会手榴弹导致无法撞击玩家的骨架万个体。
	 * 2.手榴弹蓝图设置对骨架网格体无碰撞，给玩家角色添加一个小范围的碰撞检测盒子，生成的手榴弹绑定对该检测盒子的overlap覆盖事件，覆盖事件结束才开启骨架网格体的碰撞。
	 */

	// 创建弹体的运动组件
	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(FName("ProjectileMovementComponent"));
	// 设置旋转跟随速度
	ProjectileMovementComponent->bRotationFollowsVelocity = true;
	// 确保网络复制开启
	ProjectileMovementComponent->SetIsReplicated(true);
	// 开启弹体运动组件的碰撞
	ProjectileMovementComponent->bShouldBounce = true;
}

void AProjectileGrenade::BeginPlay()
{
	// Super也就是父类的BeginPlay方法不适用本子类，因而不调用。但Super的父类也就是AActor的BeginPlay方法仍是本子类需要的，因此要调用
	// Super::BeginPlay();
	AActor::BeginPlay();

	// 创建弹体尾焰。(如果没设置则代码没做操作)
	SpawnTrailSystem();
	// 设置弹体自毁定时器
	StartDestroyTimer();

	// 绑定运动组件的反弹委托
	ProjectileMovementComponent->OnProjectileBounce.AddDynamic(this, &AProjectileGrenade::OnBounce);
}

void AProjectileGrenade::OnBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	// 判断反弹声音是否已设置
	if(BounceCue)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			BounceCue,
			GetActorLocation()
		);
	}
}

void AProjectileGrenade::Destroyed()
{
	// 造成范围伤害
	ExplodeDamage();
	// 父类的方法处理了爆炸的视效和声效的播放
	Super::Destroyed();
}

