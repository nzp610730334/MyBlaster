// Fill out your copyright notice in the Description page of Project Settings.


#include "Projectile.h"

#include "NiagaraFunctionLibrary.h"
#include "Components/BoxComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MyBlaster/Character/BlasterCharacter.h"
#include "Sound/SoundCue.h"
#include "MyBlaster/MyBlaster.h"

// Sets default values
AProjectile::AProjectile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// 设置是否能复制到远程机器上,客户端能看到，到服务器才有权限
	// 没有设置bReplicates为true，则所有机器都拥有本对象的权限，且会在所有机器上生成，独立于服务器
	bReplicates = true;

	// 碰撞盒子
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);
	CollisionBox->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	// 启用碰撞
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// 先设置忽略所有碰撞通道
	CollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	// 单独开启碰撞通道ECC_Visibility，设置为封锁(碰撞)
	CollisionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	// 单独开启碰撞通道ECC_WorldStatic，设置为封锁(碰撞)
	CollisionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Block);
	
	// 设置网格体对pawn类型可碰撞。（玩家角色通常为pawn）
	// CollisionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);
	/*
	 * 碰撞通道pawn与玩家角色的胶囊体有关，而不是角色精确的网格体Mesh。比如两腿之间的空档也会被碰撞类型为pawn的子弹命中。
	 * 为了提升游戏性，子弹应该优化为命中角色的网格体而非胶囊体，这样后续可以做部位命中相关处理，如爆头。
	 * 实现过程：
	 * 1.项目设置自定义碰撞通道类型SkeletaMesh，项目头文件MyBlaster.h中定义对应的宏常量；
	 * 2.子弹设置碰撞响应通道为ECC_SkeletaMesh且蓝图中的collision设置对pawn的检测设为ignore不要是block，
	 * 3.玩家角色BlasterCharacter.cpp构造函数㕜设置碰撞类型为ECC_SkeletaMesh。(确保蓝图中也做了相应设置)
	 */
	CollisionBox->SetCollisionResponseToChannel(ECC_SkeletaMesh, ECollisionResponse::ECR_Block);

}

// Called when the game starts or when spawned
void AProjectile::BeginPlay()
{
	Super::BeginPlay();

	// 粒子效果附着
	if(Tracer)
	{
		UGameplayStatics::SpawnEmitterAttached(
			Tracer,
			CollisionBox,
			FName(),
			GetActorLocation(),
			GetActorRotation(),
			EAttachLocation::KeepWorldPosition
		);
	}

	// 仅在服务器上绑定这些委托。比如碰撞OnHit，没有在玩家本地机器上绑定委托。
	if(HasAuthority())
	{
		// 绑定委托，将函数的执行交给CollisionBox触发？
		CollisionBox->OnComponentHit.AddDynamic(this, &AProjectile::OnHit);
	}
}
/*
 * OnHit1用于学习课程时探究绑定委托时，通过不传参数，看编译器报错，从报错信息中查看传参的函数的参数的个数和类型的要求。
 * 已探明的个数和类型的要求如下
 * UPrimitiveComponent*, AActor*, UPrimitiveComponent*, UE::Math::TVector<double>, const FHitResult&
 * 因此OnHit函数需要有对应的形参，OnHit1不符合要求。
 */
void AProjectile::OnHit1()
{
	Destroy();
}

void AProjectile::SpawnTrailSystem()
{
	// 判断视觉效果资产是否已添加
	if(TrailSyStem)
	{
		// 生成Niagara粒子并存储其内存地址
		TrailSystemComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			// 要生成的视觉效果资产
			TrailSyStem,
			// 视觉效果需要附着到哪个组件
			GetRootComponent(),
			// 附着点的名字，没有则传空字符串
			FName(),
			// 生成位置
			GetActorLocation(),
			// 生成的朝向
			GetActorRotation(),
			// 保持世界相对于附着点的位置
			EAttachLocation::KeepWorldPosition,
			// 是否自动销毁(自动销毁则火箭弹撞击爆炸销毁时弹道上的烟雾轨迹也瞬间销毁)，业务要求烟雾轨迹不能瞬间消失，因此不需要自动销毁
			false
		);
	}
}

// UPrimitiveComponent*, AActor*, UPrimitiveComponent*, UE::Math::TVector<double>, const FHitResult&
void AProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                        FVector NormalImpilse, const FHitResult& Hit)
{
	/*
	 *  因为子弹撞击的逻辑函数OnHit只会在服务器(Hasauthoy)上调用，因此这里的粒子效果和声音也只有服务器上出现。
	 *  改进：
	 *  1.改为使用RPC调用，让服务器通知所有客户端播放粒子和声音。这种方式会增加网络带宽的使用。
	 *  2.在原有代码函数的基础上改进。比如Projectile类设置了网络复制为true，则它被删除时，所有客户端上的Projectile都会被更新（删除）
	 *  因此Destory()在被调用去Destroyed本类Projectile的对象时，服务器会通知所有客户端更新，
	 *  因此可以在Projectile类重写父类的Destroyed，在重写的Destroyed中处理粒子和声音的播放。Destroy()被调用时，Actor的Destroyed会被调用。
	 *  本项目使用第2种方式。
	 *  底层源码追踪：
	 *  Destroy();到 Actor.cpp的World->DestroyActor( this, bNetForce, bShouldModifyLevel );
	 *  到 LevelActor.cpp的DEstroyedActor()的ThisActor->Destroyed();
	 */
	// // 播放子弹撞击的粒子效果
	// if(ImpactParticles)
	// {
	// 	// 传参：在哪个世界关卡生成、生成哪个粒子、粒子效果的转换(包含位置、朝向、缩放)
	// 	UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, GetActorTransform());
	// }
	//
	// // 播放子弹撞击的声音
	// if(ImpactSound)
	// {
	// 	// 传参：声音来源、声音资产、播放位置
	// 	UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());
	// }

	/* 2023年5月31日15:47:31 优化
	 * 多人游戏中，应尽量减少RPC的调用。原本逻辑为角色被子弹打中，调用多播RPC通知所有客户端播放受击动画。
	 * Health本来就需要被网络复制更新，因此可以优化为
	 * 在Health被更新后，在OnRep_Health函数中播放角色受击动画即可。所有的客户端的该角色的Health都会被更新，因此所有客户端都会播放该角色受击动画。
	 * 但OnRep_Health不会在服务器上执行，导致服务器上的角色不会播放受击动画。
	 * 函数ReceiveDamage绑定了伤害处理委托，伤害处理因子弹碰撞而执行，子弹碰撞设计为只在服务器上执行。因此可以在ReceiveDamage中播放角色受击动画作为弥补。
	 * 此处不再需要调用多播RPC播放角色受击动画。
	 */
	/*
	 * 子弹命中玩家角色的处理
	 */
	// 转换碰撞到的对象
	// ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);
	// if(BlasterCharacter)
	// {
	// 	// 转换成功，播放受击动画
	// 	BlasterCharacter->MulticastHit();
	// }
	
	// 销毁Actor，即本对象，也就是子弹
	Destroy();
}

void AProjectile::ExplodeDamage()
{
	// 获取伤害来源(开枪的玩家),Instigator的值在spawn本actor的时候被赋值，如果没有则返回nillptr
	APawn* FiringPawn = GetInstigator();
	// 获取成功。注意 伤害计算只在服务器上处理
	if(FiringPawn && HasAuthority())
	{
		// 获取伤害来源的玩家的控制器
		AController* FiringController = FiringPawn->GetController();
		if(FiringController)
		{
			// 造成有衰减的范围伤害
			UGameplayStatics::ApplyRadialDamageWithFalloff(
				// 世界上下文对象
				this,
				// 正常伤害
				Damage,
				// 最小伤害
				10.f,
				// 中心位置
				GetActorLocation(),
				// 范围的内圈大小
				DamageInnerRadius,
				// 范围的外圈大小
				DamageOuterRadius,
				// 伤害衰减的函数
				1.f,
				// 伤害类型
				UDamageType::StaticClass(),
				// 伤害忽略哪些对象(有的游戏会选择自身，免疫自身的爆炸伤害)
				TArray<AActor*>(),
				// 造成该伤害的原因：本Actor
				this,
				// 伤害来源的玩家的控制器
				FiringController
			);
		}
	}
}

// Called every frame
void AProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AProjectile::StartDestroyTimer()
{
	// 设置定时器
	GetWorldTimerManager().SetTimer(
		// 定时器
		DestroyTimer,
		// 调用定时器函数的对象
		this,
		// 要调用的函数
		&AProjectile::DestroyTimerFinished,
		// 多久调用一次
		DestroyTime
	);
}

void AProjectile::DestroyTimerFinished()
{
	/*
	 * 本函数时计时器的回调函数。时间到了就销毁自身，以此连带Niagara的视觉效果火箭弹的烟雾轨迹也一并销毁
	 */
	// 销毁自身。注意区分它和Destroyed(), Destroy()进入销毁流程并销毁，Destroyed()是执行销毁之后的工作。
	Destroy();
}

void AProjectile::Destroyed()
{
	// 记得要调用父类的函数
	Super::Destroyed();

	// 播放子弹撞击的粒子效果
	if(ImpactParticles)
	{
		// 传参：在哪个世界关卡生成、生成哪个粒子、粒子效果的转换(包含位置、朝向、缩放)
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, GetActorTransform());
	}
	
	// 播放子弹撞击的声音
	if(ImpactSound)
	{
		// 传参：声音来源、声音资产、播放位置
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());
	}
}

