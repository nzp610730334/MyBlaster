// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectileRocket.h"

#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "RocketMovementComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundCue.h"
#include "Components/BoxComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"


AProjectileRocket::AProjectileRocket()
{
	// 初始化火箭弹的静态网格体组件
	ProjectMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rocket Mesh"));
	// 附着到根组件
	ProjectMesh->SetupAttachment(RootComponent);
	// 该网格体暂时不需要碰撞，因此关闭
	ProjectMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 创建火箭弹体的移动组件
	RocketMovementComponent = CreateDefaultSubobject<URocketMovementComponent>(TEXT("RocketMovementComponent"));
	// 设置朝向跟随速度的方向
	RocketMovementComponent->bRotationFollowsVelocity = true;
	// 确保网络复制的开启
	RocketMovementComponent->SetIsReplicated(true);
}

void AProjectileRocket::Destroyed()
{
	/*
	 * 在子类中的本函数实际是处理销毁带来的相关逻辑，实际的销毁在别的地方已经完成。
	 * 重写父类实体子弹的销毁只是为了不把父类的爆炸视效和声效再执行一遍，因为本子类弹体爆炸和声效在Hit时候已经执行了。
	 *
	 * 备注：
	 * 火箭弹爆炸造成伤害，过程是：火箭弹发生撞击-》火箭弹播放爆炸视效声效-》范围伤害应用-》销毁弹体
	 * 普通子弹撞击造成伤害，过程是：子弹发生撞击-》伤害应用-》销毁弹体后的方法Destroyed中播放子弹的撞击(命中)的视效声效
	 */ 
}

#if WITH_EDITOR
void AProjectileRocket::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	/*
	 * 当本类的属性被改变时，会触发FPropertyChangeEvent事件
	 */
	// 因为是重写父类的PostEditChangeProperty，父类的该方法有必要的代码需要先执行
	Super::PostEditChangeProperty(Event);
	// 使用三元运算符检查并获取事件中被修改的属性的名字。因为Event.Property是一个指针，不检查就直接使用可能会导致崩溃。指向空指针时，使用NAME_None赋值
	FName PropertyName = Event.Property != nullptr ? Event.Property->GetFName() : NAME_None;
	// 当被修改的属性的名字符合本类的InitialSpeed属性的名字时，手动将被修改的变量的值更新到需要与之变动一致的属性上
	if(PropertyName == GET_MEMBER_NAME_CHECKED(AProjectileRocket, InitialSpeed))
	{
		// 将被修改后的属性的值赋值到运动组件的变量上。先确定运动组件一定存在
		if(RocketMovementComponent)
		{
			// 修改移动组件的初始速度(用于调试)
			RocketMovementComponent->InitialSpeed = InitialSpeed;
			// 修改移动组件的最大速度(用于调试)
			RocketMovementComponent->MaxSpeed = InitialSpeed;
		}
	}
}
#endif

void AProjectileRocket::BeginPlay()
{
	Super::BeginPlay();

	// 在客户端上绑定这些委托，因为Super::BeginPlay()中已经在服务器上绑定了这些委托
	if(!HasAuthority())
	{
		// 绑定委托，将函数的执行交给CollisionBox触发？
		CollisionBox->OnComponentHit.AddDynamic(this, &AProjectileRocket::OnHit);
	}
	// 生成弹体的尾焰视觉效果
	SpawnTrailSystem();
	
	// 判断声效资产和衰减配置是否已添加
	if(ProjectileLoop && LoopingSoundAttenuation)
	{
		// 生成可附加其他Actor上面的声音并存储其内存地址，用于后续销毁
		ProjectileLoopComponent = UGameplayStatics::SpawnSoundAttached(
			// 声音资产
			ProjectileLoop,
			// 附着到哪个组件
			GetRootComponent(),
			// 附着点名称，没有则传空的FName
			FName(),
			// 生成位置
			GetActorLocation(),
			// 保持世界位置
			EAttachLocation::KeepWorldPosition,
			// 当附着的对象销毁时声音是否销毁
			false,
			// 音量扩大倍数
			1.f,
			// 音高倍数
			1.f,
			// 开始时间
			0.f,
			// 衰减配置
			LoopingSoundAttenuation,
			// 并发声音，没有则传对应类型的空指针。这里为了占位从而能传入后续的参数
			(USoundConcurrency*)nullptr,
			// 是否自动销毁
			false
		);
	}
}

void AProjectileRocket::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                              FVector NormalImpilse, const FHitResult& Hit)
{
	// 当命中目标是自身时，直接返回不作处理。(防止弹体本体直接命中玩家自身而触发爆炸。注意弹体命中地面造成范围伤害仍对玩家自身生效)
	if(OtherActor == GetOwner())
	{
		// UE_LOG(LogTemp, Warning, TEXT("火箭弹命中了自己！"));
		return;
	}
	// 造成范围伤害
	ExplodeDamage();
	
	// 父类的方法，即Super::OnHit()调用后，弹头已经销毁，因此伤害传递等逻辑需要在此之前完成。（P121, 父类的撞击函数代码逻辑是撞击就销毁了，这里先不调用）
	// Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpilse, Hit);

	// 创建定时器定时执行销毁弹体的逻辑
	StartDestroyTimer();
	
	/*
	 * 本项目中火箭弹与步枪子弹的不同之处在于：火箭弹的爆炸效果和声音在撞击时(本函数)执行，步枪子弹的命中效果和声音在撞击逻辑中的销毁逻辑执行后执行。
	 */
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

	/*
	 * 火箭弹撞击事件后，通过计时器延迟销毁前仍然存在，因此需要取消其显示、网格体碰撞、以及销毁生成的火焰烟雾轨迹的粒子效果
	 */
	if(ProjectMesh)
	{
		// 设置网格体不可见
		ProjectMesh->SetVisibility(false);
	}
	if(CollisionBox){
		// 取消其碰撞盒子的碰撞
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	// 确保火焰烟雾轨迹组件存在且已经实例化了对象
	if(TrailSystemComponent && TrailSystemComponent->GetSystemInstance())
	{
		// 销毁
		TrailSystemComponent->GetSystemInstance()->Deactivate();
	}
	// 判断火箭弹飞行声音是否存在且正在播放。火箭弹命中撞击后需要停止火箭弹飞行中循环播放的声音
	if(ProjectileLoopComponent && ProjectileLoopComponent->IsPlaying())
	{
		// 停止正在播放的声音
		ProjectileLoopComponent->Stop();
	}
}


