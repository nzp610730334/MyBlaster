// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectileBullet.h"

#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "MyBlaster/BlasterComponents/LagCompensationComponent.h"
#include "MyBlaster/Character/BlasterCharacter.h"
#include "MyBlaster/PlayerController/BlasterPlayerController.h"


AProjectileBullet::AProjectileBullet()
{
	// 实例化运动组件
	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
	// 设置旋转跟随速度
	ProjectileMovementComponent->bRotationFollowsVelocity = true;
	// 确保网络复制开启
	ProjectileMovementComponent->SetIsReplicated(true);
	// 修改移动组件的初始速度(用于调试)
	ProjectileMovementComponent->InitialSpeed = InitialSpeed;
	// 修改移动组件的最大速度(用于调试)
	ProjectileMovementComponent->MaxSpeed = InitialSpeed;
}

#if WITH_EDITOR
void AProjectileBullet::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	/*
	 * 当本类的属性被改变时，会触发FPropertyChangeEvent事件
	 */
	// 因为是重写父类的PostEditChangeProperty，父类的该方法有必要的代码需要先执行
	Super::PostEditChangeProperty(Event);
	// 使用三元运算符检查并获取事件中被修改的属性的名字。因为Event.Property是一个指针，不检查就直接使用可能会导致崩溃。指向空指针时，使用NAME_None赋值
	FName PropertyName = Event.Property != nullptr ? Event.Property->GetFName() : NAME_None;
	// 当被修改的属性的名字符合本类的InitialSpeed属性的名字时，手动将被修改的变量的值更新到需要与之变动一致的属性上
	if(PropertyName == GET_MEMBER_NAME_CHECKED(AProjectileBullet, InitialSpeed))
	{
		// 将被修改后的属性的值赋值到运动组件的变量上。先确定运动组件一定存在
		if(ProjectileMovementComponent)
		{
			// 修改移动组件的初始速度(用于调试)
            ProjectileMovementComponent->InitialSpeed = InitialSpeed;
            // 修改移动组件的最大速度(用于调试)
            ProjectileMovementComponent->MaxSpeed = InitialSpeed;
		}
	}
}
#endif


void AProjectileBullet::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpilse, const FHitResult& Hit)
{
	// 转换本对象的所有者
	ABlasterCharacter* OwnerCharacter = Cast<ABlasterCharacter>(GetOwner());
	// 成功获取所有者
	if(OwnerCharacter)
	{
		// 获取该所有者的控制器
		ABlasterPlayerController* OwnerController = Cast<ABlasterPlayerController>(OwnerCharacter->Controller);
		// 控制器获取成功
		if(OwnerController)
		{
			// 当前机器是服务器，且投射物未开启应用服务器倒带，则说明该投射物弹体是由服务器上的玩家开火发射的，则直接应用伤害即可
			if(OwnerCharacter->HasAuthority() && !bUseServerSideRewind)
			{
				// 判断是否爆头，是否应用爆头伤害
				const float DamageToCause = Hit.BoneName.ToString() == FString("head") ? HeadShotDamage : Damage;
				// 使用UE原生的伤害逻辑
                UGameplayStatics::ApplyDamage(
                	// 受伤的Actor
                	OtherActor,
                	// 伤害量
                	DamageToCause,
                	// 伤害来源（玩家）的控制器
                	OwnerController,
                	// 造成伤害的Actor，本对象，子弹
                	this,
                	// 伤害类型？
                	UDamageType::StaticClass()
                	);
				// 本投射物碰撞伤害计算完毕，执行父类的碰撞函数(里面有销毁本投射物的操作)，然后返回
				Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpilse, Hit);
				return;
			}
			// Cast转换受击角色为玩家角色类型
			ABlasterCharacter* HitCharacter = Cast<ABlasterCharacter>(OtherActor);
			// HitCharacter转换成功，本投射物对象开启了服务器倒带且本地有控制权且玩家角色的延迟补偿组件存在，则说明是本投射物是客户端玩家发射的，则需要发送RPC请求，请求服务器进行命中判定
			if(bUseServerSideRewind && OwnerCharacter->GetLagCompensation() && OwnerCharacter->IsLocallyControlled() && HitCharacter)
			{
				OwnerCharacter->GetLagCompensation()->ProjectileServerScoreRequest(
					HitCharacter,
					TraceStart,
					InitialVelocity,
					OwnerController->GetServerTime() - OwnerController->SingleTripTime
					);
			}
		}
	}
	
	// 因为父类方法中调用了销毁方法，因此父类的OnHit需要在最后才调用。
	Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpilse, Hit);
}

void AProjectileBullet::BeginPlay()
{
	Super::BeginPlay();
	/*
	// 初始化新的抛物体/投射物参数。该投射物将用于生成模拟弹道。
	FPredictProjectilePathParams PathParams;

	// 如果使用碰撞进行跟踪，是否使用跟踪通道TraceChannel。
	PathParams.bTraceWithChannel = true;
	// 是否沿着路径追踪，寻找阻挡碰撞并在第一次命中时停止。
	PathParams.bTraceWithCollision = true;
	// 预测的弹道的Debug绘制时长
	PathParams.DrawDebugTime = 5.f;
	// 调试绘制持续时间的类型(None无、ForOneFrame持续一帧、ForDuration持续一段时长(即Debug绘制时长)、Persistent永久)。
	PathParams.DrawDebugType = EDrawDebugTrace::ForDuration;
	// 启动速度(矢量)。使用Actor的向前单位方向作为矢量方向，单位矢量乘上3500.f作为速度
    PathParams.LaunchVelocity = GetActorForwardVector() * InitialSpeed;
	// 投射物的模拟飞行时长
    PathParams.MaxSimTime = 4.f;
	// 投射物半径
	PathParams.ProjectileRadius = 5.f;
	// 预测模拟频率。数值越大则路径上的模拟投射物越多。
	PathParams.SimFrequency = 0.1f;
	// 投射物的发射起始点
	PathParams.StartLocation = GetActorLocation();
	// 投射物的碰撞检测通道
	PathParams.TraceChannel = ECollisionChannel::ECC_Visibility;
	// 投射物碰撞检测时需要忽略对哪些目标的碰撞。忽略的目标是一个数组。需要忽略的目标都add进去
	PathParams.ActorsToIgnore.Add(this);

	// 可选参数：覆盖重力。（如果为0，则使用WorldGravityZ）。此处直接将运动组件的重力大小赋值给模拟投射物，是为了保持弹体和模拟投射物的飞行轨迹一致。
	PathParams.OverrideGravityZ = ProjectileMovementComponent->GetGravityZ();
	
	// 初始化新的抛物体/投射物碰撞结果
	FPredictProjectilePathResult PathResult;
	// 根据传入的抛物体/投射物参数ProjectileParams，预测弹道，并将碰撞结果存入ProjectileResult
	UGameplayStatics::PredictProjectilePath(this, PathParams, PathResult);
	*/
}
