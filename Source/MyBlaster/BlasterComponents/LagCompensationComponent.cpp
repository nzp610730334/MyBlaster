// Fill out your copyright notice in the Description page of Project Settings.


#include "LagCompensationComponent.h"

#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MyBlaster/MyBlaster.h"
#include "MyBlaster/Character/BlasterCharacter.h"

// Sets default values for this component's properties
ULagCompensationComponent::ULagCompensationComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

// Called when the game starts
void ULagCompensationComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	// 初始化一个帧信息结构体
	FFramePackage Package;
	// 保存此帧中补偿组件需要的帧信息存入Package中
	SaveFramePackage(Package);
	// 绘制调试图形
	ShowFramePackage(Package, FColor::Orange);
}

FFramePackage ULagCompensationComponent::InterpBetweenFrames(const FFramePackage& OlderFrame,
	const FFramePackage& YoungerFrame, float HitTime)
{
	// 计算较新和较旧的历史帧之间的时间跨度
	const float Distance = YoungerFrame.Time - OlderFrame.Time;
	/*
	 * 计算命中时间处于该时间跨度上的位置，作为后续插值程度.（使用Clamp将结果限制在正常范围，即0-1之间）
	 * 时间轴从左至右，olderFrame.time《 HitTime《 NewFrame.time
	 */ 
	const float InterpFraction = FMath::Clamp((HitTime - OlderFrame.Time)/Distance, 0, 1);
	// 取出较新和较旧的历史帧的帧信息，赋值给创建的新的引用，为了防止后续手误将其修改，使用const修饰

	// 初始化新的帧信息结构体作为插值之后的结果
	FFramePackage InterpFramePackage;
	// 该结构体的time即是命中时间
	InterpFramePackage.Time = HitTime;
	// 遍历较新的历史帧的碰撞盒子的映射
	for(auto & YoungerPair : YoungerFrame.HitBoxInfo)
	{
		// 将较新的历史帧里的映射的碰撞盒子的名字作为结果帧信息的碰撞盒子映射的key
		const FName& BoxInfoName = YoungerPair.Key;
		// 取出该碰撞盒子的较旧的历史帧信息赋值新的引用
		const FBoxInformation& OlderBox = OlderFrame.HitBoxInfo[BoxInfoName];
		// 取出该碰撞盒子的较新的历史帧信息赋值新的引用
		const FBoxInformation& YoungerBox = YoungerFrame.HitBoxInfo[BoxInfoName];
		// 初始化新的碰撞盒子的帧信息结构体用于存储插值结果的碰撞盒子
		FBoxInformation InterpBoxInfo;
		// 将插值后的Location赋值给InterpInfo,增量时长设置为1.f即可，此处起主要作用的是InterpFraction，表示插值的程度，增量时长在此处没有太大意义
		InterpBoxInfo.Location = FMath::VInterpTo(OlderBox.Location, YoungerBox.Location, 1.f, InterpFraction);
		// 将插值后的Rotation赋值给InterpInfo
		InterpBoxInfo.Rotation = FMath::RInterpTo(OlderBox.Rotation, YoungerBox.Rotation, 1.f, InterpFraction);
		// 本业务中，玩家角色的碰撞盒子的范围不会随着时间改变，因此直接使用较新的碰撞盒子的范围赋值即可
		InterpBoxInfo.BoxExtent = YoungerBox.BoxExtent;
		// 将该碰撞盒子的帧信息添加进插值结果的帧信息结构体的映射中
		InterpFramePackage.HitBoxInfo.Add(BoxInfoName, InterpBoxInfo);
	}
	// 返回插值结果的帧信息
	return InterpFramePackage;
}

FServerSideRewindResult ULagCompensationComponent::ConfirmHit(const FFramePackage& Package,
	ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation)
{
	// 当受击角色为空时，直接返回空的命中结果结构体
	if(HitCharacter == nullptr) return FServerSideRewindResult();
	// 初始化新的帧信息结构体，用于存储受击角色当前碰撞盒子的帧信息
	FFramePackage CurrentFrame;
	// 缓存受击角色当前碰撞盒子的位置
	CacheBoxPosition(HitCharacter, CurrentFrame);
	// 移动受击角色的碰撞盒子到命中判定的位置
	MoveBoxes(HitCharacter, Package);
	// 临时关闭受击角色的网格体的碰撞,本函数return之前记得重新开启
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::NoCollision);

	/*
	 * 多人在线PVP射击游戏，玩家通常以爆头为主要目标，因此每次命中判定时先判定头部命中情况，如果命中头部，则可以提前完成命中判定的工作。
	 * 没有命中头部时，再循环遍历所有碰撞盒子的部位。
	 * 通常情况下为了节省性能开销，玩家角色没有开启碰撞盒子的碰撞检测。命中判定前，需要开启，判定结束，需要关闭。
	 */
	// 取出受击角色的头部碰撞盒子
	UBoxComponent* HeadBox = HitCharacter->HitCollisionBoxes[FName("head")];
	// 开启碰撞检测（碰撞查询和物理模拟）
	HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// 设置碰撞响应通道为自定义类型ECC_HitBox,响应类型为阻塞
	HeadBox->SetCollisionResponseToChannel(ECC_HitBox, ECR_Block);
	/*
	 * 计算命中判定射线的终点。命中判定射线实际是对弹道的模拟。
	 * 直接使用HitLocation作为命中判定射线的终点，可能导致命中判定射线过短，不足以到达碰撞盒子，因此将其适当延长到1.25倍。
	 * 通常情况，弹道射程远远大于玩家之间的交战距离，也远远大于枪口和受击点的距离，相当于无限，因此延长处理不影响游戏公平。
	 * 命中射线终点位置 = 射线起始点(枪口) + 枪口到命中位点的向量
	 */
	const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;

	// 初始化新的命中结果用于存储本次命中射线的命中结果
	FHitResult ConfirmHitResult;
	// 获取世界关卡
	UWorld* World = GetWorld();
	// 正确获取世界关卡
	if(World)
	{
		// 生成命中判定射线
        World->LineTraceSingleByChannel(
        	// 存储命中结果
        	ConfirmHitResult,
        	// 射线起始点
        	TraceStart,
        	// 射线终点
        	TraceEnd,
        	// 射线检测类型
        	ECC_HitBox
        );
        // 当有命中时
        if(ConfirmHitResult.bBlockingHit)
        {
			// 命中的组件有效时
        	if(ConfirmHitResult.Component.IsValid())
        	{
        		// 转换命中的组件为盒子组件
        		UBoxComponent* Box = Cast<UBoxComponent>(ConfirmHitResult.Component);
        		// 转换成功
        		if(Box)
        		{
        			// 根据命中的组件的数据，在世界关卡场景中绘制相应的调试DebugBox
        			DrawDebugBox(GetWorld(), Box->GetComponentLocation(), Box->GetScaledBoxExtent(), FQuat(Box->GetComponentRotation()), FColor::Red, false, 8.f);
        		}
        	}
        	
        	// 重置受击角色的所有碰撞盒子的位置信息为缓存的位置信息
        	ResetHitBoxes(HitCharacter, CurrentFrame);
        	// 服务器命中判定完成。重新开启受击角色的网格体的碰撞检测
        	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
        	// 返回服务器本次命中确认的结果。在此之前只开启了头部盒子的碰撞检测，因此如果有命中，则命中的只能是头部。
        	return FServerSideRewindResult{true, true};
        }
		// 未命中时，则需要遍历开启其余的碰撞盒子的碰撞检测，并重新生成命中射线进行判定。疑惑：这里没有关闭前面的头部碰撞盒子的碰撞检测，理论上没有影响。
		else
		{
			for(auto & BoxPair : HitCharacter->HitCollisionBoxes)
			{
				// 开启碰撞检测（碰撞查询和物理模拟）
				BoxPair.Value->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				// 设置碰撞响应通道为自定义类型ECC_HitBox,响应类型为阻塞
				BoxPair.Value->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
			}
			// 重新生成命中射线进行判定
			World->LineTraceSingleByChannel(
				// 存储命中结果
				ConfirmHitResult,
				// 射线起始点
				TraceStart,
				// 射线终点
				TraceEnd,
				// 射线检测类型
				ECC_HitBox
			);
			// 有命中时
			if(ConfirmHitResult.bBlockingHit)
			{
				// 命中的组件有效时
				if(ConfirmHitResult.Component.IsValid())
				{
					// 转换命中的组件为盒子组件
					UBoxComponent* Box = Cast<UBoxComponent>(ConfirmHitResult.Component);
					// 转换成功
					if(Box)
					{
						// 根据命中的组件的数据，在世界关卡场景中绘制相应的调试DebugBox
						DrawDebugBox(GetWorld(), Box->GetComponentLocation(), Box->GetScaledBoxExtent(), FQuat(Box->GetComponentRotation()), FColor::Blue, false, 8.f);
					}
				}
				
				// 重置受击角色的所有碰撞盒子的位置信息为缓存的位置信息
				ResetHitBoxes(HitCharacter, CurrentFrame);
				// 服务器命中判定完成。重新开启受击角色的网格体的碰撞检测
				EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
				// 返回服务器本次命中确认的结果。确认命中，但没有命中头部
				return FServerSideRewindResult{true, false};
			}
		}
	}
	/*
	 * 命中判定流程完成，有命中的情况在此之前已经return，因此这里处理未命中的情况.
	 * 疑惑：ResetHitBoxes和EnableCharacterMeshCollision是否可以通过优化将其仅在此处恢复，也仅在这里最后return，
	 * 之前的return改为仅存储FServerSideRewindResult结果，在这里最后才一起return，提高代码阅读性
	 */ 
	// 重置受击角色的所有碰撞盒子的位置信息为缓存的位置信息
	ResetHitBoxes(HitCharacter, CurrentFrame);
	// 服务器命中判定完成。重新开启受击角色的网格体的碰撞检测
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
	// 返回服务器本次命中确认的结果。结果未命中，更没有命中头部。
	return FServerSideRewindResult{false, false};
}

FShotgunServerSideRewindResult ULagCompensationComponent::ShotgunConfirmHit(const TArray<FFramePackage>& FramePackages,
	const FVector_NetQuantize& TraceStart, const TArray<FVector_NetQuantize>& HitLocations)
{
	// 遍历历史帧数组，如果有某一帧信息的角色指向了空指针，则直接返回空的倒带结果
	for(auto & Frame : FramePackages)
	{
		if(Frame.Character == nullptr) return FShotgunServerSideRewindResult();
	}
	// 初始化本次霰弹枪命中判定的倒带结果
	FShotgunServerSideRewindResult ShotgunResult;
	// 创建新数组用于存储每个历史帧里面的角色的当前的帧信息
	TArray<FFramePackage> CurrentFrames;
	// 遍历历史帧数组,缓存当前帧信息、移动碰撞盒子、关闭网格体的碰撞
	for(auto & Frame : FramePackages)
	{
		// 初始化新的帧信息结构体
		FFramePackage CurrentFrame;
		// 缓存帧信息结构体的所属角色。疑惑：这一步为什么不直接放到CacheBoxPosition里面执行
		CurrentFrame.Character = Frame.Character;
		// 存储该历史帧所属的角色的当前帧信息
		CacheBoxPosition(Frame.Character, CurrentFrame);
		// 移动该历史帧所属的角色的碰撞盒子到进行命中判定的位置
		MoveBoxes(Frame.Character, Frame);
		// 关闭该帧信息所属的玩家角色的网格体的碰撞，避免影响对本次命中判定射线的影响(比如阻挡)
		EnableCharacterMeshCollision(Frame.Character, ECollisionEnabled::NoCollision);
		// 将帧信息结构体加入数组中
		CurrentFrames.Add(CurrentFrame);
	}
	// 遍历历史帧数组，开启所有受击角色的头部碰撞盒子的碰撞判定
	for(auto & Frame : FramePackages)
	{
		// 获取该历史帧所属的角色的网格体的头部碰撞盒子
		UBoxComponent* HeadBox = Frame.Character->HitCollisionBoxes[FName("head")];
		// 开启碰撞盒子的碰撞检测
		HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		// 设置碰撞盒子的碰撞检测类型
		HeadBox->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
	}

	// 获取世界关卡
	UWorld* World = GetWorld();
	/*
	 * 遍历所有命中点，发射射线进行头部碰撞盒子的命中判定(因为至今为止仅开启了头部碰撞盒子的碰撞检测)
	 */ 
	for(auto & HitLocation : HitLocations)
	{
		// 初始化射线的命中结果
		FHitResult ConfirmHitResult;
		// 计算射线的落点
		const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
		// 成功获取世界关卡时
		if(World)
		{
			// 发射射线进行命中判定
			World->LineTraceSingleByChannel(
				ConfirmHitResult,
				TraceStart,
				TraceEnd,
				// 检测类型
				ECC_HitBox
				);
			// 当有命中时。（原教程省略了ConfirmHitResult.bBlockingHit的判断）
			if(ConfirmHitResult.bBlockingHit)
			{
				// 命中的组件有效时
				if(ConfirmHitResult.Component.IsValid())
				{
					// 转换命中的组件为盒子组件
					UBoxComponent* Box = Cast<UBoxComponent>(ConfirmHitResult.Component);
					// 转换成功
					if(Box)
					{
						// 根据命中的组件的数据，在世界关卡场景中绘制相应的调试DebugBox
						DrawDebugBox(GetWorld(), Box->GetComponentLocation(), Box->GetScaledBoxExtent(), FQuat(Box->GetComponentRotation()), FColor::Red, false, 8.f);
					}
				}
				
				// 转换命中Actor为玩家角色
				ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(ConfirmHitResult.GetActor());
				// 转换成功时
				if(BlasterCharacter)
				{
					/*
					 * 记录受击角色被命中头部的次数(因为代码到此为止只开启了受击角色的头部碰撞盒子进行检测)
					 */
					// 该受击角色已经命中过头部，则累计次数（自增）
					if(ShotgunResult.HeadShots.Contains(BlasterCharacter))
					{
						ShotgunResult.HeadShots[BlasterCharacter]++;
					}
					// 该受击角色未命中过头部，则设置命中头部次数为1
					else
					{
						ShotgunResult.HeadShots.Emplace(BlasterCharacter, 1);
					}
				}
			}
		}
	}

	// 遍历所有历史帧，开启每一帧信息所属的角色的所有碰撞盒子，并关闭头部碰撞盒子(因为头部已经命中判定过了)
	for(auto & Frame : FramePackages)
	{
		// 遍历该角色的所有碰撞盒子
		for(auto & HitBoxPair : Frame.Character->HitCollisionBoxes)
		{
			// 碰撞盒子不为空
			if(HitBoxPair.Value != nullptr)
            {
            	// 开启该碰撞盒子的碰撞检测
				HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				// 设置该碰撞盒子的碰撞检测类型以及响应行为
				HitBoxPair.Value->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
            }
		}
		// 获取该角色的头部碰撞盒子
		UBoxComponent* HeadBox = Frame.Character->HitCollisionBoxes[FName("head")];
		// 关闭该角色的头部碰撞盒子的碰撞检测
		HeadBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	/*
	 * 遍历所有命中点，发射射线进行除了头部的身体各部位碰撞盒子的命中判定(已经开启了除了头部以外的碰撞盒子的碰撞检测)
	 */
	for(auto & HitLocation : HitLocations)
	{
		// 初始化射线的命中结果
		FHitResult ConfirmHitResult;
		// 计算射线的落点
		const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
		// 成功获取世界关卡时
		if(World)
		{
			// 发射射线进行命中判定
			World->LineTraceSingleByChannel(
				ConfirmHitResult,
				TraceStart,
				TraceEnd,
				// 检测类型
				ECC_HitBox
				);
			// 当有命中时。（原教程省略了ConfirmHitResult.bBlockingHit的判断）
			if(ConfirmHitResult.bBlockingHit)
			{
				// 命中的组件有效时
				if(ConfirmHitResult.Component.IsValid())
				{
					// 转换命中的组件为盒子组件
					UBoxComponent* Box = Cast<UBoxComponent>(ConfirmHitResult.Component);
					// 转换成功
					if(Box)
					{
						// 根据命中的组件的数据，在世界关卡场景中绘制相应的调试DebugBox
						DrawDebugBox(GetWorld(), Box->GetComponentLocation(), Box->GetScaledBoxExtent(), FQuat(Box->GetComponentRotation()), FColor::Blue, false, 8.f);
					}
				}
				
				// 转换命中Actor为玩家角色
				ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(ConfirmHitResult.GetActor());
				// 转换成功时
				if(BlasterCharacter)
				{
					/*
					 * 记录受击角色被命中头部的次数(因为代码到此为止只开启了受击角色的头部碰撞盒子进行检测)
					 */
					// 该受击角色已经命中过头部，则累计次数（自增）
					if(ShotgunResult.BodyShots.Contains(BlasterCharacter))
					{
						ShotgunResult.BodyShots[BlasterCharacter]++;
					}
					// 该受击角色未命中过头部，则设置命中头部次数为1
					else
					{
						ShotgunResult.BodyShots.Emplace(BlasterCharacter, 1);
					}
				}
			}
		}
	}

	/*
	 * 所有要进行命中判定的历史帧的所属角色的受击命中判定(头部和身体各部位)都已完成，
	 * 恢复所有参与命中判定的角色的碰撞盒子的位置以及重新开启他们的网格体的碰撞
	 */
	for(auto & Frame : CurrentFrames)
	{
		// 恢复角色的碰撞盒子的位置
		ResetHitBoxes(Frame.Character, Frame);
		// 重新开启他们的网格体的碰撞
		EnableCharacterMeshCollision(Frame.Character, ECollisionEnabled::QueryAndPhysics);
	}
	// 返回命中判定结果
	return ShotgunResult;
}

FServerSideRewindResult ULagCompensationComponent::ProjectileConfirmHit(const FFramePackage& Package,
	ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart,
	const FVector_NetQuantize100& InitialVelocity, float HitTime)
{
	// 当受击角色为空时，直接返回空的命中结果结构体
	if(HitCharacter == nullptr) return FServerSideRewindResult();
	// 初始化新的帧信息结构体，缓存当前碰撞盒子的帧信息
	FFramePackage CurrentFrame;
	CacheBoxPosition(HitCharacter, CurrentFrame);
	// 移动当前碰撞盒子到命中判定的位置
	MoveBoxes(HitCharacter, Package);
	// 关闭受击角色的网格体的检测碰撞，避免对命中判定产生干扰
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::NoCollision);
	
	// 初始化新的要进行投射物预测射线检测的参数
	FPredictProjectilePathParams PathParams;
	// 是否显示碰撞轨迹
	PathParams.bTraceWithCollision = true;
	// 用于预测模拟的射线的发射时长
	PathParams.MaxSimTime = MaxRecordTime;
	// 模拟的射线的发射速度(矢量)
	PathParams.LaunchVelocity = InitialVelocity;
	// 发射地点
	PathParams.StartLocation = TraceStart;
	// 确定模拟中每个子步骤的大小（截断MaxSimTime）。根据所需的质量和性能，建议在10到30之间。表现为该值越高，弹道射线射线绘制的约密集
	PathParams.SimFrequency = 15.f;
	// 追踪碰撞时使用的投射物半径。如果<=0，则使用线条跟踪。
	PathParams.ProjectileRadius = 5.f;
	// 射线的碰撞检测通道
	PathParams.TraceChannel = ECC_HitBox;
	// 射线忽略哪些Actor。通常忽略开火的玩家，即本组件的所有者。注意：向服务器发起命中判定请求的是开火的玩家的本组件
	PathParams.ActorsToIgnore.Add(GetOwner());
	// Debug图形的绘制显示时长
	PathParams.DrawDebugTime = 5.f;
	// Debug图形的存在形式，一定时间持续存在
	PathParams.DrawDebugType = EDrawDebugTrace::ForDuration;
	// 初始化新的投射物预测射线的命中结果
    FPredictProjectilePathResult PathResult;

	// 获取受击角色的头部碰撞盒子
	UBoxComponent* HeadBox = HitCharacter->HitCollisionBoxes[FName("head")];
	// 获取成功
	if(HeadBox)
	{
		// 开启受击角色的头部碰撞盒子的检测
		HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		// 单独开启对某个检测通道的碰撞检测，检测类型为阻塞
		HeadBox->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
	}

	
	// 发射投射物预测射线进行检测
    UGameplayStatics::PredictProjectilePath(
        this,
        PathParams,
        PathResult
        );

	// 如果有命中时
    if(PathResult.HitResult.bBlockingHit)
    {
        // 命中的组件有效时(注意：DrawDebugBox要绘制的是命中的组件，因此if检测和Cast的处理对象都是组件，而非它所属的Actor)
        if(PathResult.HitResult.Component.IsValid())
        {
        	// 转换命中的组件为Box组件
            UBoxComponent* Box = Cast<UBoxComponent>(PathResult.HitResult.Component);
			// 转换成功
        	if(Box)
            {
                // 根据命中的组件的数据，在世界关卡场景中绘制相应的调试DebugBox
                DrawDebugBox(GetWorld(), Box->GetComponentLocation(), Box->GetScaledBoxExtent(), FQuat(Box->GetComponentRotation()), FColor::Red, false, 8.f);
            }
        }
        
        // 将受击角色的碰撞盒子恢复
        ResetHitBoxes(HitCharacter, CurrentFrame);
        // 重新开启受击角色的网格体的碰撞检测
        EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
        // 返回命中判定的结果。既发生了命中，且该命中也爆头了。
        return FServerSideRewindResult{true, true};
    }
	// 头部命中判定发现没有命中时，接下来对其他部位进行命中判定
	else
	{
		// 循环遍历其余碰撞盒子，开启它们的碰撞检测
        for(auto & BoxPair:Package.Character->HitCollisionBoxes)
        {
        	// 开启它们的碰撞检测
            BoxPair.Value->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        	// 单独开启对某个检测通道的碰撞检测，检测类型为阻塞
        	BoxPair.Value->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
        }
        // 关闭受击角色头部碰撞盒子的碰撞检测。（因为头部碰撞盒子的命中判定已经完成）
        HeadBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        // 再次发射投射物预测射线进行检测
        UGameplayStatics::PredictProjectilePath(this, PathParams, PathResult);
        // 有命中时
        if(PathResult.HitResult.bBlockingHit)
        {
            // 命中的组件有效时(注意：DrawDebugBox要绘制的是命中的组件，因此if检测和Cast的处理对象都是组件，而非它所属的Actor)
            if(PathResult.HitResult.Component.IsValid())
            {
                // 转换命中的组件为Box组件
                UBoxComponent* Box = Cast<UBoxComponent>(PathResult.HitResult.Component);
                // 转换成功
                if(Box)
                {
                    // 根据命中的组件的数据，在世界关卡场景中绘制相应的调试DebugBox
                    DrawDebugBox(GetWorld(), Box->GetComponentLocation(), Box->GetScaledBoxExtent(), FQuat(Box->GetComponentRotation()), FColor::Blue, false, 8.f);
                }
            }
            
            // 将受击角色的碰撞盒子恢复
            ResetHitBoxes(HitCharacter, CurrentFrame);
            // 重新开启受击角色的网格体的碰撞检测
            EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
            // 返回命中判定的结果。发生了命中，但没有爆头
            return FServerSideRewindResult{true, false};
        }
	}
	
	/*
	 * 头部和其他部位的碰撞盒子都已经完成命中判定，如果有命中的情况前面就已经return了，此处开始处理未发生任何命中的情况。
	 */
	// 将受击角色的碰撞盒子恢复
	ResetHitBoxes(HitCharacter, CurrentFrame);
	// 重新开启受击角色的网格体的碰撞检测
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
	// 返回命中判定的结果。未发生任何命中，更没有命中头部。
	return FServerSideRewindResult{false, false};
}

void ULagCompensationComponent::CacheBoxPosition(ABlasterCharacter* HitCharacter, FFramePackage& OutFramePackage)
{
	// 受击角色为空，则直接返回
	if(HitCharacter == nullptr) return;
	// 遍历受击角色的碰撞盒子映射
	for(auto & HitBoxPair : HitCharacter->HitCollisionBoxes)
	{
		// 确保遍历获取的键值对的值不为空
		if(HitBoxPair.Value != nullptr)
		{
			// 初始化单个碰撞盒子的帧信息结构体
            FBoxInformation BoxInfo;
			// 缓存碰撞盒子的位置
            BoxInfo.Location = HitBoxPair.Value->GetComponentLocation();
			// 缓存碰撞盒子的朝向
            BoxInfo.Rotation = HitBoxPair.Value->GetComponentRotation();
			// 缓存碰撞盒子的范围
            BoxInfo.BoxExtent = HitBoxPair.Value->GetScaledBoxExtent();
			// 将该碰撞盒子的名字以及帧信息结构体存入玩家角色的帧信息结构体的映射中
            OutFramePackage.HitBoxInfo.Add(HitBoxPair.Key, BoxInfo);
		}
	}
}

void ULagCompensationComponent::MoveBoxes(ABlasterCharacter* HitCharacter, const FFramePackage& Package)
{
	// 受击角色为空，则直接返回即可
	if(HitCharacter == nullptr) return;

	if(HitCharacter->HitCollisionBoxes.Num() <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("HitCollisionBoxes.Num() <= 0"))
	}
	// 遍历受击角色的所有命中盒子，设置其涉及命中判定的属性
	for(auto & HitBoxPair : HitCharacter->HitCollisionBoxes)
	{
		// 确保遍历到的结果不为空指针
		if(HitBoxPair.Value != nullptr)
		{
			// 设置该碰撞盒子的世界位置为插值结果的帧信息里面的对应碰撞盒子的位置
			HitBoxPair.Value->SetWorldLocation(Package.HitBoxInfo[HitBoxPair.Key].Location);
			// 设置该碰撞盒子的世界朝向为插值结果的帧信息里面的对应碰撞盒子的朝向
			HitBoxPair.Value->SetWorldRotation(Package.HitBoxInfo[HitBoxPair.Key].Rotation);
			// 设置该碰撞盒子的范围大小为插值结果的帧信息里面的对应碰撞盒子的范围大小
			HitBoxPair.Value->SetBoxExtent(Package.HitBoxInfo[HitBoxPair.Key].BoxExtent);
		}
	}
}

void ULagCompensationComponent::ResetHitBoxes(ABlasterCharacter* HitCharacter, const FFramePackage& Package)
{
	// 受击角色为空，则直接返回即可
	if(HitCharacter == nullptr) return;
	// 遍历受击角色的所有命中盒子，设置其涉及命中判定的属性
	for(auto & HitBoxPair : HitCharacter->HitCollisionBoxes)
	{
		// 确保遍历到的结果不为空指针
		if(HitBoxPair.Value != nullptr)
		{
			// 设置该碰撞盒子的世界位置为插值结果的帧信息里面的对应碰撞盒子的位置
			HitBoxPair.Value->SetWorldLocation(Package.HitBoxInfo[HitBoxPair.Key].Location);
			// 设置该碰撞盒子的世界朝向为插值结果的帧信息里面的对应碰撞盒子的朝向
			HitBoxPair.Value->SetWorldRotation(Package.HitBoxInfo[HitBoxPair.Key].Rotation);
			// 设置该碰撞盒子的范围大小为插值结果的帧信息里面的对应碰撞盒子的范围大小
			HitBoxPair.Value->SetBoxExtent(Package.HitBoxInfo[HitBoxPair.Key].BoxExtent);
			// 关闭碰撞盒子的碰撞检测
			HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

void ULagCompensationComponent::EnableCharacterMeshCollision(ABlasterCharacter* HitCharacter,
	ECollisionEnabled::Type CollisionEnabled)
{
	// 确保受击角色以及它的网格体存在
	if(HitCharacter && HitCharacter->GetMesh())
	{
		HitCharacter->GetMesh()->SetCollisionEnabled(CollisionEnabled);
	}
}

void ULagCompensationComponent::ShowFramePackage(const FFramePackage& Package, const FColor& Color)
{
	/*
	 * 组件的初始化和生成早于拥有其的角色，因此能看到玩家角色还未持有开局武器时的状态被绘制到世界场景中，
	 * 因为开局武器需要等角色生成之后才附着上去
	 */
	// 遍历帧信息结构体，根据结构体里面所有的碰撞组件的信息绘制调试box
	for(auto& BoxInfo:Package.HitBoxInfo)
	{
		// 在世界场景中绘制box
		DrawDebugBox(
			// 在哪个世界关卡绘制
			GetWorld(),
			// box的中心
			BoxInfo.Value.Location,
			// 绘制的box的范围
			BoxInfo.Value.BoxExtent,
			// 绘制的box的朝向
			FQuat(BoxInfo.Value.Rotation),
			// 绘制的box的颜色
			Color,
			// 是否持续存在
			false,
			// 持续时长
			4.f
			);
	}
}

FServerSideRewindResult ULagCompensationComponent::ServerSideRewind(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart,
	const FVector_NetQuantize& HitLocation, float HitTime)
{
	// 根据受击角色和受击时间，从该角色的历史帧信息中获取用于服务器倒带进行命中判定的帧信息(判定依据)
	FFramePackage FrameToCheck = GetFrameToCheck(HitCharacter, HitTime);
	
	// 返回命中判定的结果
	return ConfirmHit(FrameToCheck, HitCharacter, TraceStart, HitLocation);
}

FShotgunServerSideRewindResult ULagCompensationComponent::ShotgunServerSideRewind(
	const TArray<ABlasterCharacter*>& HitCharacters, const FVector_NetQuantize& TraceStart,
	const TArray<FVector_NetQuantize>& HitLocations, float HitTime)
{
	// 初始化数组用于存放所有要用于命中判定的帧信息
	TArray<FFramePackage> FrameToChecks;
	// 遍历所有受击角色
	for(ABlasterCharacter* HitCharacter:HitCharacters)
	{
		// 根据受击角色和受击时间，从该角色的历史帧信息中获取用于服务器倒带进行命中判定的帧信息(判定依据)
		FFramePackage FrameToCheck = GetFrameToCheck(HitCharacter, HitTime);
		// 将结果加入数组
		FrameToChecks.Add(FrameToCheck);
	}
	
	// 进行命中判定并将结果返回
	return ShotgunConfirmHit(FrameToChecks, TraceStart, HitLocations);
}

FServerSideRewindResult ULagCompensationComponent::ProjectileServerSideRewind(ABlasterCharacter* HitCharacter,
	const FVector_NetQuantize& TraceStart, const FVector_NetQuantize100& InitialVelocity, float HitTime)
{
	// 根据受击角色和受击时间，从该角色的历史帧信息中获取用于服务器倒带进行命中判定的帧信息(判定依据)
	FFramePackage FrameToCheck = GetFrameToCheck(HitCharacter, HitTime);
	// 返回命中判定的结果
	return ProjectileConfirmHit(FrameToCheck, HitCharacter, TraceStart, InitialVelocity, HitTime);
}

FFramePackage ULagCompensationComponent::GetFrameToCheck(ABlasterCharacter* HitCharacter, float HitTime)
{
	// 判断各种条件是否满足进行服务器倒带的条件
	bool bReturn = HitCharacter == nullptr ||
		HitCharacter->GetLagCompensation() == nullptr ||
			HitCharacter->GetLagCompensation()->FrameHistory.GetHead() == nullptr ||
				HitCharacter->GetLagCompensation()->FrameHistory.GetTail() == nullptr;
	
	// 是否需要直接返回，不进行服务器倒带
	if(bReturn) FFramePackage();

	// 初始化新的帧信息结构体，用于后续的命中校验
	FFramePackage FrameToCheck;
	// 是否需要进行插值处理。绝大部分情况都需要，因此初始化为true
	bool bShouldInterpolate = true;
	
	// 数据读取的代码过长，这里声明新的短名字的引用，便于后续代码编写。该引用不会被修改则使用const修饰，注意引用的类型要与被引用的对象一致
	const TDoubleLinkedList<FFramePackage>& History = HitCharacter->GetLagCompensation()->FrameHistory;
	// 受击角色的最老的历史帧信息的时间
	const float OldestHistoryTime = History.GetTail()->GetValue().Time;
	// 受击角色的最新的历史帧信息的时间
	const float NewestHistoryTime = History.GetHead()->GetValue().Time;

	// 命中时间早于服务器存储的受击角色的最老的历史帧信息，则不进行服务器倒带
	if(OldestHistoryTime > HitTime)
	{
		// too far back - too laggy to do SSR	太过久远太迟无法进行服务器倒带(SSR)，直接返回
		return FFramePackage();
	}
	// 极端情况，命中时间等于服务器存储的受击角色的最老的历史帧信息
	if(OldestHistoryTime == HitTime)
	{
		// 将受击角色的最老的帧信息存储
		FrameToCheck = History.GetTail()->GetValue();
		// 这种命中时间恰好最老的历史帧信息的情况，则无需进行插值，后续直接处理
		bShouldInterpolate = false;
	}
	// 疑惑：为什么是命中时间大于等于受击角色的最新历史帧信息的时间
	if(NewestHistoryTime <= HitTime)
	{
		// 将受击角色的最新的帧信息存储
		FrameToCheck = History.GetHead()->GetValue();
		// 这种命中时间恰好最新的历史帧信息的情况，则无需进行插值，后续直接处理
		bShouldInterpolate = false;
	}

	/*
	 * 使用两个指针表示较新较旧的历史帧的指针，在时间轴上将命中时间的那一帧包夹起来，
	 * 后续在较新和较旧的俩帧之间做插值，处理碰撞盒子的位置，判断命中时间那一帧时的命中位置是否命中了碰撞盒子？
	 * Younger是一种指针，类型是元素为FFramePackage的双向链表TDoubleLinkedList的链表节点指针TDoubleLinkedListNode*
	 */
	// 获取受击角色的历史帧信息的双向链表的头指针作为较新的历史帧的指针
	TDoubleLinkedList<FFramePackage>::TDoubleLinkedListNode* Younger = History.GetHead();
	// 开始时，较新的历史帧的指针与较新的历史帧的指针指向一致
	TDoubleLinkedList<FFramePackage>::TDoubleLinkedListNode* Older = Younger;
	// 当较旧的历史帧的时间大于命中时间时，则一直循环
	while(Older->GetValue().Time > HitTime)
	{
		/*
		 * 指针指向改变时，需要注意指向的内存地址是否为空，为空则说明双向链表已经从头遍历到尾了。break跳出本循环
		 * 经过前面的代码判断，虽然不可能但还是需要再判断，避免访问空指针导致奔溃。
		 */ 
		if(Older->GetNextNode() == nullptr) break;
		// 将Older指向下一个节点
		Older = Older->GetNextNode();
		// 如果较旧的历史帧的时间仍旧大于命中时间，则将Younger指向当前Older指向的内存地址
		if(Older->GetValue().Time > HitTime)
		{
			Younger = Older;
		}
	}
	// 如果较旧的历史帧的时间恰好等于命中时间，可能性极低，但也要处理
	if(Older->GetValue().Time == HitTime)
	{
		// 将该时刻的帧信息存储，用于后续命中判断
		FrameToCheck = Older->GetValue();
		// 那么这种情况下也无需进行插值，后续直接使用该帧信息判断命中与否即可
		bShouldInterpolate = false;
	}
	// 是否需要插值处理
	if(bShouldInterpolate)
	{
		// Interplate between Younger and older;
		// 传入较新和较旧的历史帧，插值处理，获得用于命中校验的帧信息
		FrameToCheck = InterpBetweenFrames(Older->GetValue(), Younger->GetValue(), HitTime);
	}

	// 注意不要忘了补上该帧信息的所属角色，最后return前才补上是因为前面可能会出现整个FrameToCheck被覆盖赋值的情况，最后补上最稳妥
	FrameToCheck.Character = HitCharacter;
	// 返回用于服务器倒带命中判定的依据(受击角色命中时的帧信息)
	return FrameToCheck;
}

// P203 教程后，不再传递参数AWeapon* DamageCauser
void ULagCompensationComponent::ServerScoreRequest_Implementation(ABlasterCharacter* HitCharacter,
                                                                  const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, float HitTime
                                                                  // AWeapon* DamageCauser
                                                                  )
{
	// 进行服务器倒带处理，判定开火玩家是否命中受击玩家,存储倒带处理的结果
	FServerSideRewindResult Confirm = ServerSideRewind(HitCharacter, TraceStart, HitLocation, HitTime);
	// 如果判定命中，且必要的属性变量不为空
	// if(Character && HitCharacter && DamageCauser && Confirm.bHitCofirmed)
	// 原教程代码
	if(Character && HitCharacter && Character->GetEquippedWeapon() && Confirm.bHitCofirmed)
	{
		// 判断是否应用爆头伤害
		// float DamageToCasue = Confirm.bHeadShot ? DamageCauser->GetHeadShotDamage() : DamageCauser->GetDamage();
		/*
		 * 原教程的代码。疑惑：玩家发射后立马切换武器，切换武器的RPC在命中判定的RPC前到达，那么命中判定时伤害就读取了切换后的武器的伤害，这是否恰当？
		 */
		float DamageToCause = Confirm.bHeadShot ? Character->GetEquippedWeapon()->GetHeadShotDamage() : Character->GetEquippedWeapon()->GetDamage();
		/*
		 *  应用伤害传递.
		 *  伤害值这类重要的游戏数据，应该谨慎考虑是否由客户端传递，因为客户端可能作弊不可信，且伤害值可以从RPC传递的武器中获取，
		 *  有必要的话，将其武器伤害与服务器本地的武器伤害对比校验是否正常即可。没有必要花费额外的带宽传递伤害值。
		 */
		UGameplayStatics::ApplyDamage(
			// 被伤害的角色
			HitCharacter,
			// 伤害值
			DamageToCause,
			// 伤害来源(本玩家的控制器)
			Character->Controller,
			// 受伤原因
			// DamageCauser,
			Character->GetEquippedWeapon(),
			// 伤害类型
			UDamageType::StaticClass()
			);
	}
}

void ULagCompensationComponent::ShotgunServerScoreRequest_Implementation(
	const TArray<ABlasterCharacter*>& HitCharacters, const FVector_NetQuantize& TraceStart,
	const TArray<FVector_NetQuantize>& HitLocations, float HitTime)
{
	FShotgunServerSideRewindResult Confirm = ShotgunServerSideRewind(HitCharacters, TraceStart, HitLocations, HitTime);
	/*
	 * 遍历所有受击角色，计算每个角色所受伤害总量并应用伤害。
	 * 疑惑：一般的思路上，为什么不从Confirm中获取所有受击角色，再循环遍历HitCharacters嵌套循环从Confirm中获取的所有受击角色？
	 * 直接遍历HitCharacters不会因为有的受击角色突然消失(掉线),导致无法计算伤害吗？代码里使用过了if以及Contains防止访问空指针。
	 */ 
	for(auto & HitCharacter : HitCharacters)
	{
		// 条件不满足时，跳到下一轮循环
		if(HitCharacter == nullptr || Character->GetEquippedWeapon() == nullptr) return;
		// 初始化该受击角色的总受击伤害
		float TotalDamage = 0.f;
		// 判断映射表里是否有该角色头部受击记录，否则直接使用键key访问，访问了空指针可能会导致奔溃
		if(Confirm.HeadShots.Contains(HitCharacter))
		{
			// 该受击角色的头部受击伤害 = 头部受击次数 * 武器的爆头伤害
			float HeadDamage = Confirm.HeadShots[HitCharacter] * Character->GetEquippedWeapon()->GetHeadShotDamage();
			// 累计到总伤害
			TotalDamage += HeadDamage;
		}
		// 判断映射表里是否有该角色身体受击记录，否则直接使用键key访问，访问了空指针可能会导致奔溃
		if(Confirm.BodyShots.Contains(HitCharacter))
		{
			// 该受击角色的身体受击伤害 = 身体受击次数 * 武器伤害
			float BodyDamage = Confirm.BodyShots[HitCharacter] * Character->GetEquippedWeapon()->GetDamage();
			// 累计到总伤害
			TotalDamage += BodyDamage;
		}
		// 应用伤害
		UGameplayStatics::ApplyDamage(
			// 受击角色
			HitCharacter,
			// 该受击角色所受总伤害
			TotalDamage,
			// 伤害来源：开火的玩家的控制器
			Character->Controller,
			// 造成该伤害的原因：开火玩家的武器
			Character->GetEquippedWeapon(),
			// 伤害类型
			UDamageType::StaticClass()
			);
	}
}

void ULagCompensationComponent::ProjectileServerScoreRequest_Implementation(ABlasterCharacter* HitCharacter,
	const FVector_NetQuantize& TraceStart, const FVector_NetQuantize100& InitialVelocity, float HitTime)
{
	// 服务器收到客户端的RPC请求后，进行服务器倒带处理，赋值缓存命中判定的结果
	FServerSideRewindResult Confirm = ProjectileServerSideRewind(HitCharacter, TraceStart, InitialVelocity, HitTime);
	// 命中判定结果获取成功,受击角色存在且命中判定结果为命中
	if(Character && HitCharacter && Confirm.bHitCofirmed && Character->GetEquippedWeapon())
	{
		/*
		 * 原教程的代码。疑惑：玩家发射后立马切换武器，切换武器的RPC在命中判定的RPC前到达，那么命中判定时伤害就读取了切换后的武器的伤害，这是否恰当？
		 */
		float DamageToCause = Confirm.bHeadShot ? Character->GetEquippedWeapon()->GetHeadShotDamage() : Character->GetEquippedWeapon()->GetDamage();
		
		// 应用伤害
		UGameplayStatics::ApplyDamage(
			HitCharacter,
			DamageToCause,
			Character->Controller,
			Character->GetEquippedWeapon(),
			UDamageType::StaticClass()
			);
	}
}

// Called every frame
void ULagCompensationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
	
	// 进行玩家角色的历史帧的缓存。
	SaveFramePakcage();
}

void ULagCompensationComponent::SaveFramePakcage()
{
	/*
	 * 存储玩家角色最近一段时间的帧信息
	 */
	// 服务器倒带处理命中判定仅发生在服务器上，因此历史帧的缓存处理也仅需要在服务器上进行
	if(Character == nullptr || !Character->HasAuthority()) return;

	// 先判断双向链表的力是否已存有帧信息，是则直接开始存储帧信息
	if(FrameHistory.Num() <= 1)
	{
		// 初始化空的帧信息结构体，用于存储本帧的信息
		FFramePackage ThisFrame;
		// 传入结构体，将帧信息存储进去
		SaveFramePackage(ThisFrame);
		// 将本帧帧信息的结构体添加到双向链表中
		FrameHistory.AddHead(ThisFrame);
	}
	// 双向链表内已有帧信息的情况
	else
	{
		// 计算双向链表里最新的帧信息的存储时间距离最旧的帧信息的存储时间已经过去多久
		float HistoryLength = FrameHistory.GetHead()->GetValue().Time - FrameHistory.GetTail()->GetValue().Time;
		// 当该时长大于需要保存的帧信息的时间跨度时，循环执行
		while(HistoryLength > MaxRecordTime)
		{
			// 移除双向链表的尾节点。这会将双向链表的尾指针改为指向链表原本倒数第二的节点
			FrameHistory.RemoveNode(FrameHistory.GetTail());
			// 重新计算当前最新和最旧的帧信息的时间跨度
			HistoryLength = FrameHistory.GetHead()->GetValue().Time - FrameHistory.GetTail()->GetValue().Time;
		}
		/*
		 * 通过上述代码，将双向链表的长度(帧信息的时间跨度)限制在一定范围内后，将本帧的帧信息加入双向链表中。
		 * 所以实际时间跨度比MaxRecordTime设置的要大一点，但下一帧就再次while循环处理存储的长度，因此影响不大
		 */
		// 初始化新的结构体用于存储本帧帧信息
		FFramePackage ThisFrame;
		// 传入结构体，将帧信息存储进去
		SaveFramePackage(ThisFrame);
		// 将本帧帧信息的结构体添加到双向链表中
		FrameHistory.AddHead(ThisFrame);

		// Debug绘制本帧碰撞盒子的信息（仅用于调试）
		// ShowFramePackage(ThisFrame, FColor::Red);
	}
}

void ULagCompensationComponent::SaveFramePackage(FFramePackage& Package)
{
	// 转换玩家角色
	Character = Character == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : Character;
	// 转换成功
	if(Character)
	{
		// 记录本次存储时的时间。本函数将仅运行在服务器上，因此存储的是服务器的权威时间(游戏诞生以来的时长)
		Package.Time = GetWorld()->GetTimeSeconds();
		// 记录本次命中的角色
		Package.Character = Character;
		// 获取玩家角色的组件，并遍历。auto是自适应类型，将根据获取的数据类型(FName:box组件的键值对)初始化自己的数据类型
		for(auto& BoxPair : Character->HitCollisionBoxes)
		{
			// 初始化新的用于存储玩家角色帧信息单个碰撞盒子的结构体
			FBoxInformation BoxInformation;
			/*
			 * BoxPair是FName:box组件的键值对，BoxPair.Value则是对应的组件对象。
			 */
			// 取出玩家角色的组件的位置存储
			BoxInformation.Location = BoxPair.Value->GetComponentLocation();
			// 取出玩家角色的组件的朝向存储
			BoxInformation.Rotation = BoxPair.Value->GetComponentRotation();
			// 取出玩家角色的组件的缩放范围存储
			BoxInformation.BoxExtent = BoxPair.Value->GetScaledBoxExtent();
			// 组合FName和单个组件帧信息的结构体的键值对，将它添加到包含所有组件的帧信息的结构体的映射中
			Package.HitBoxInfo.Add(FName(BoxPair.Key), BoxInformation);
		}
	}
}


