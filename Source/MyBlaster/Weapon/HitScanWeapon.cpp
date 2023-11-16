// Fill out your copyright notice in the Description page of Project Settings.


#include "HitScanWeapon.h"

#include "Engine/SkeletalMeshSocket.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "MyBlaster/BlasterComponents/LagCompensationComponent.h"
#include "MyBlaster/PlayerController/BlasterPlayerController.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"

void AHitScanWeapon::Fire(const FVector& HitTarget)
{
	Super::Fire(HitTarget);

	// 获取本射线武器的所有者并转换为pawn
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	// 为空指针时世界返回
	if(OwnerPawn == nullptr) return;
	// 指定该Pawn的控制器为伤害来源方的控制器
	AController* InstigatorController = OwnerPawn->GetController();

	// UE_LOG(LogTemp, Warning, TEXT("手枪开火了！"));
	
	// 获取射线武器的网格体，通过网格体获取插槽
	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
	// 插槽获取成功
	if(MuzzleFlashSocket)
	{
		// 获取插槽的转换Tranform。需要传入插槽来自的骨骼网格组件(指的就是武器的网格体)
		FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
		// 枪口插槽的世界转换作为射线射出的起始点
		FVector Start = SocketTransform.GetLocation();
		// 计算落点。（疑惑：含义是什么？为什么是1.25f）
		FVector End = Start + (HitTarget - Start) * 1.25f;

		// 初始化一个碰撞结果用于后续存储
		FHitResult FireHit;
		// 计算弹道散布处理后的开火射击
		WeaponTraceHit(Start, HitTarget, FireHit);

		// Cast转换命中的目标为玩家角色。（因为只有命中了玩家角色才需要进行伤害计算）
		ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(FireHit.GetActor());
				
		/*
		 * 进行伤害计算。在该操作之前，需要满足3个条件。
		 * 1.受击角色BlasterCharacter成功被获取；
		 * 2.获取伤害来源方的控制器成功，在这里才判断是因为A玩家的控制器只存在他的本地和服务器上，其他玩家没有A的控制器，
		 * 如果在前面过早就判断控制器的获取，则后面的粒子效果等逻辑也无法执行；
		 * 3.仅在服务器上才做伤害处理。本射线武器通过射线检测在这里进行伤害处理，
		 * 而实弹武器则是仅在服务器上才生成实弹(然后网络复制到各客户端)，实弹武器的伤害处理在实弹命中目标后才执行。
		 */ 
		if(BlasterCharacter && InstigatorController)
		{
			/*
			 *  是否符合造成伤害的条件(未开启服务器倒带或者本地对伤害来源的玩家有控制权)
			 *  疑惑：bool值的计算不够明晰，应该优化改进，提高代码阅读性
			 */
			bool bCauseAuthDamage = !bUseServerSideRewind || OwnerPawn->IsLocallyControlled();
			// 当前机器是服务器且符合造成伤害的条件时，直接应用伤害传递即可
			if(HasAuthority() && bCauseAuthDamage)
			{
				// 判断命中的骨骼是否是头部(或者是其他游戏业务逻辑上被认为是弱点的部位),命中头部则应用爆头伤害。为了防止手误修改，使用const将其修饰
				const float DamageToCause = FireHit.BoneName.ToString() == FString("head") ? HeadShotDamage : Damage;
				
				// 调用UE原生的伤害处理函数
                UGameplayStatics::ApplyDamage(
                	// 受伤的Actor对象
                	BlasterCharacter,
                	// 伤害量
                	DamageToCause,
                	// 伤害来源方的控制器
                	InstigatorController,
                	// 造成伤害的原因
                	this,
                	// 伤害类型
                	UDamageType::StaticClass()
                );
			}
			// 当前机器为非服务器(即客户端)，且该武器是否开启了应用服务器倒带
			if(!HasAuthority() && bUseServerSideRewind)
			{
				// 获取并转换玩家角色。注意区分BlasterOwnerCharacter和BlasterCharacter，在本函数 BlasterCharacter 是受击角色
				BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(OwnerPawn) : BlasterOwnerCharacter;
				// 获取并转换玩家控制器。注意前面获取的InstigatorController仅是个AController，发送RPC的函数参数要求ABlasterPlayerController类型
				BlasterOwnerController = BlasterOwnerController == nullptr ? Cast<ABlasterPlayerController>(InstigatorController) : BlasterOwnerController;
				// 成功获取并转换玩家角色和控制器，且玩家角色的延迟补偿组件存在且玩家角色有自主代理权(本地控制)
				if(BlasterOwnerCharacter && BlasterOwnerController && BlasterOwnerCharacter->GetLagCompensation() && BlasterOwnerCharacter->IsLocallyControlled())
				{
					// 客户端向服务器发送进行命中判定的RPC请求
					BlasterOwnerCharacter->GetLagCompensation()->ServerScoreRequest(
						// 受击角色
						BlasterCharacter,
						// 枪口位置
						Start,
						// 命中的位置。疑惑：Hitarget是战斗组件的准星瞄准位置，该位置经过散布处理后，才是玩家实际射击的射线，为什么传这个参数给服务器作命中判定？不应该传FireHit.ImpactPoint吗？
						HitTarget,
						/*
						 * 命中的时刻。疑惑：为何还要减去通信单程时长？开火玩家、服务器、受击角色，三者的机器上的时间轴理论上不是一样的吗？
						 * 我的思考：比如受击角色在服务器上的关卡生成的第10秒经过A点，那么受击角色在开火玩家的客户端机器上也是该关卡生成的第10秒经过A点，只不过客户端和服务器的第10秒不一定同步而已。
						 * 这里应该直接传入GetServerTime()获取的时间即可。
						 */ 
						BlasterOwnerController->GetServerTime() - BlasterOwnerController->SingleTripTime
						// P203教程后该RPC不再传递武器
						// this
						);
				}
			}
		}
		// 播放命中的粒子效果
		if(ImpactParticles)
		{
			UGameplayStatics::SpawnEmitterAtLocation(
				// 在哪个世界生成
				GetWorld(),
				// 生成什么粒子效果
				ImpactParticles,
				// 生成粒子的位置(这里使用发生碰撞的位置，注意不要使用End，那是瞄准的位置)
				FireHit.ImpactPoint,
				// 粒子效果的发射方向。使用命中的法线ImpactNormal是一个Vector，取Vector的方向作为发射方向。
				FireHit.ImpactNormal.Rotation()
			);
		}
		// 播放命中的声音
		if(HitSound)
		{
			UGameplayStatics::PlaySoundAtLocation(
				this,
				HitSound,
				FireHit.ImpactPoint
			);
		}
		// 判断开火枪口火焰特效是否已设置
		if(MuzzleFlash)
		{
			UGameplayStatics::SpawnEmitterAtLocation(
				GetWorld(),
				MuzzleFlash,
				SocketTransform
			);
		}
		// 判断开火声音是否已设置
		if(FireSound)
		{
			UGameplayStatics::PlaySoundAtLocation(
				this,
				FireSound,
				GetActorLocation()
			);
		}
		// 判断开火枪口火焰特效是否已设置
		if(MuzzleFlash)
		{
			UGameplayStatics::SpawnEmitterAtLocation(
				GetWorld(),
				MuzzleFlash,
				SocketTransform
			);
		}
		// 判断开火声音是否已设置
		if(FireSound)
		{
			UGameplayStatics::PlaySoundAtLocation(
				this,
				FireSound,
				GetActorLocation()
			);
		}
	}
}

void AHitScanWeapon::WeaponTraceHit(const FVector& TraceStart, const FVector& HitTarget, FHitResult& OutHit)
{
	/*
	 * TraceStart: 弹道起始点，通常等价于枪口位置。
	 * HitTarget: 弹丸实际的落点
	 * OutHit: 用于存储碰撞结果的变量
	 */
	// 获取世界关卡
	UWorld* World = GetWorld();
	// 获取成功
	if(World)
	{
		// 计算子弹落点位置。开启散布处理时，使用散布处理方法，没有则使用原来的计算方式。
		// FVector End = bUseScatter ? TraceEndWithScatter(TraceStart, HitTarget) : TraceStart + (HitTarget - TraceStart) * 1.25f;
		// 计算子弹落点位置。因为在此之前的HitTarget已经是经过随机弹丸散布处理，从而获得的弹丸最终落点。疑惑：为什么*1.25f ？
		FVector End = TraceStart + (HitTarget - TraceStart) * 1.25f;
		// 生成命中射线。OutHit是外部传入的引用参数，在这里是用于存储命中结果。
		World->LineTraceSingleByChannel(
			OutHit,
			TraceStart,
			End,
			ECC_Visibility
		);
		// 弹道粒子效果轨迹默认落点位置
		FVector BeamEnd = End;
		// 命中射线产生命中时，
		if(OutHit.bBlockingHit)
		{
			// 将弹道粒子效果轨迹落点位置该为命中的位置
			BeamEnd = OutHit.ImpactPoint;
		}
		// 否则，将射线的尽头落点作为命中位置
		else
		{
			OutHit.ImpactPoint = End;
		}

		// DrawDebugSphere(GetWorld(), BeamEnd, 16.f, 12, FColor::Red, true);
		// DrawDebugSphere(GetWorld(), FireHit.ImpactPoint, 6.f, 12, FColor::Red);
		
		// 判断弹道粒子效果是否已设置
		if(BeamParticles)
		{
			// 生成粒子效果
			UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(
				// 在哪个世界关卡生成
				World,
				// 要生成的粒子效果
				BeamParticles,
				// 生成的位置
				TraceStart,
				// 生成的朝向
				FRotator::ZeroRotator,
				// 是否自动销毁
				true
			);
			// 成功生成
			if(Beam)
			{
				// 设置参数移动粒子发射器的新的位置，以达到子弹飞行的效果。(Target是该粒子效果资产在UE中的一个参数的名字)
				Beam->SetVectorParameter(FName("Target"), BeamEnd);
			}
		}
	}
}
