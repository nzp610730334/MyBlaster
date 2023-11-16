// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectileWeapon.h"

#include "Projectile.h"
#include "Engine/SkeletalMeshSocket.h"

void AProjectileWeapon::Fire(const FVector& HitTarget)
{
	Super::Fire(HitTarget);
	
	// Cast转换本武器对象的所有者并存储，用于后续赋值给生成弹体Actor的发起者
	APawn* InstigatorPawn = Cast<APawn>(GetOwner());
	if(InstigatorPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("已经成功Cast转换并获取InstigatorPawn啦！！！"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("转换获取InstigatorPawn失败啦！！！"));
	}
	
	// 获取武器网格体预留的枪口插槽
	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName(FName("MuzzleFlash"));
	// 获取枪口插槽的转换
	FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
	// 根据子弹目标落点和枪口位置计算子弹飞行方向
	FVector ToTarget = HitTarget - SocketTransform.GetLocation();
	FRotator TargetRotation = ToTarget.Rotation();

	// 初始化要生成的Actor参数
	FActorSpawnParameters SpawnParams;
	// 后续伤害计算需要用到所有者作为伤害来源？
	// 设置所有者为武器的所有者
	SpawnParams.Owner = GetOwner();
	// 设置发起者为武器的所有者
	SpawnParams.Instigator = InstigatorPawn;
	// 获取游戏世界关卡
	UWorld* World =  GetWorld();
	
	// 枪口插槽以及世界关卡获取成功
	if(MuzzleFlashSocket && World)
	{
		// 初始化要生成的子弹
		AProjectile* SpawnedProjectile = nullptr;
		/*
		 * 根据本武器类是否有开启应用服务器倒带、以及本类在不同机器上的网络角色地位不同，spawn生成不同的actor。
		 */
		if(bUseServerSideRewind)
		{
			/* 疑惑
			 * 2023年8月2日23:23:05 问题记录
			 * 榴弹短时间内快速连续开火，生成多个爆炸物，爆炸炸死自己后，后续的爆炸继续生效，但GetOwner()即本武器的所有者已经不存在，
			 * 从而导致InstigatorPawn为nullptr，原教程没有在if中对InstigatorPawn进行判断而直接->访问指针指向的内存地址导致报错程序崩溃！
			 * 暂时的解决方案：补充对InstigatorPawn的判断，但这样处理导致角色死亡即本武器的所有者不存在后，开火成功产生的弹丸只有模拟效果。见自定义标记TAG：2023080201
			 */
			// 当前机器是服务器时(本武器的持有者是服务器的版本)
			if(InstigatorPawn && InstigatorPawn->HasAuthority())
			{
				// 本actor受本地机器控制时(即本actor属于在本服务器上游玩的玩家)，则需要spawn的actor是可以被网络复制到所有客户端上的actor
				if(InstigatorPawn->IsLocallyControlled())
				{
					/*
					 * 备注：当前子弹对空发射没有碰撞销毁的话会一直留存在世界场景之中。
					 * 后期优化：在碰撞销毁之外添加子弹到达预定的HitTarget后销毁的逻辑
					 * AProjectile是要生成的actor的类的父类
					 */
                    SpawnedProjectile = World->SpawnActor<AProjectile>(
                    	// 生成的actor的类型
                    	ProjectileClass,
                    	// 生成位置
                    	SocketTransform.GetLocation(),
                    	// 生成的朝向
                    	TargetRotation,
                    	// 相关生成参数
                    	SpawnParams
                    );
					// 当前机器是服务器，生成的子弹命中判定本来就已经在服务器上，因此无需开启服务器倒带
					SpawnedProjectile->bUseServerSideRewind = false;
					// 将武器设置的伤害赋值给弹体，用于后续碰撞命中判定时应用伤害
					SpawnedProjectile->Damage = Damage;
					// 当前的爆头伤害设置，使用武器的爆头伤害将投射物的爆头伤害覆盖。
					SpawnedProjectile->HeadShotDamage = HeadShotDamage;
				}
				/*
				 * 否则，本actor就是其他玩家控制的，仅起模拟的作用，生成一个无需网络复制的actor供在本地机器(服务器)上游玩的玩家观看即可。
				 * 这是由其他玩家发射的仅用于模拟的实体子弹，无需关注其伤害多少，伤害值是该开火玩家机器上生成的网络复制的弹体里面携带的
				 */ 
				else
				{
					SpawnedProjectile = World->SpawnActor<AProjectile>(
						// 该子类的属性中没有启用网络复制
						ServerSideRewindProjectileClass,
						SocketTransform.GetLocation(),
						TargetRotation,
						SpawnParams
					);
					// 生成的actor仅作模拟使用，因此也无需开启服务器倒带
					// SpawnedProjectile->bUseServerSideRewind = false;
					/*
					 * P189为了避免ProjectileBullet的OnHit中对受击角色应用了两次伤害，此处改为true。
					 * 疑惑：此为服务器上没有本地控制权的实体弹丸的处理，不启用服务器倒带逻辑上是正确的，应该为false，
					 * 如果因为弹丸OnHit时命中判定那里的代码导致算了两次伤害，则应该改动的是命中判定的代码，
					 * 比如通过检测弹丸的所有者(枪械)的所有者(玩家角色)是否本地控制来处理是否进行应用伤害
					 */
					// UE_LOG(LogTemp, Warning, TEXT("准备设置SpawnedProjectile的bUseServerSideRewind的值！！！！"));
					SpawnedProjectile->bUseServerSideRewind = true;
					/* 原教程没有的处理
					 * 将武器设置的伤害赋值给弹体。
					 * 备注：普通的弹道武器，客户端开火命中，需要RPC请求服务器进行命中判定，子弹伤害使用RPC传的武器伤害。
					 * 而榴弹火箭炮武器等延时爆炸的弹道武器，爆炸判定在服务器上(投射物销毁时进行爆炸伤害计算)，使用的是投射物生成时的变量伤害，
					 * 因此这里也需要从武器拿伤害赋值给投射物，否则客户端发射的榴弹爆炸时，服务器上客户端发射的榴弹用的是榴弹默认伤害而非武器伤害
					 */ 
					SpawnedProjectile->Damage = Damage;
				}
			}
			// 当前的机器是客户端时
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("不满足条件InstigatorPawn && InstigatorPawn->HasAuthority() "));
				if(InstigatorPawn)
				{
					UE_LOG(LogTemp, Warning, TEXT("InstigatorPawn存在！！！"));
					if(InstigatorPawn->HasAuthority())
					{
						UE_LOG(LogTemp, Warning, TEXT("InstigatorPawn具有网络权限！！！"));
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("InstigatorPawn没有网络权限！！！"));
					}
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("InstigatorPawn不存在！！！"));
				}
				// 本actor收本地机器(客户端)控制，说明是客户端的玩家控制本武器生成弹体
				if(InstigatorPawn && InstigatorPawn->IsLocallyControlled())
				{
					SpawnedProjectile = World->SpawnActor<AProjectile>(
						ProjectileClass,
						SocketTransform.GetLocation(),
						TargetRotation,
						SpawnParams
					);
					// 生成的actor需要开启服务器倒带，在服务器上进行命中判定
					SpawnedProjectile->bUseServerSideRewind = true;
					/*
					 * 该actor会被网络复制，因此需要将服务器命中判定所需的属性存储其中。
					 */
					// 该弹体的生成点
					SpawnedProjectile->TraceStart = SocketTransform.GetLocation();	
					// 该弹体的速度(方向和大小)
					SpawnedProjectile->InitialVelocity = SpawnedProjectile->GetActorForwardVector() * SpawnedProjectile->InitialSpeed;
				}
				// 本Actor不收本地机器(客户端)的控制，说明是其他玩家控制本武器生成弹体
				else
				{
					// 自定义标记TAG：2023080201
					UE_LOG(LogTemp, Warning, TEXT("不满足条件InstigatorPawn && InstigatorPawn->IsLocallyControlled()， 当前生成的弹体只有模拟效果！！！"));
					// 则生成的弹体仅作模拟即可，该弹体本身无需开启网络复制
					SpawnedProjectile = World->SpawnActor<AProjectile>(
						ServerSideRewindProjectileClass,
						SocketTransform.GetLocation(),
						TargetRotation,
						SpawnParams
					);
					// 生成的actor仅作模拟使用，因此也无需开启服务器倒带
					SpawnedProjectile->bUseServerSideRewind = false;
				}
			}
		}
		/*
		 * 本武器类未开启服务器倒带，则不管是在那个机器上生成的actor弹体，发出开火指令后，都需要服务器生成弹体后，通过原本的actor对象的网络复制逻辑，复制到所有机器上
		 */ 
		else
		{
			if(InstigatorPawn->HasAuthority())
			{
				// 则生成的弹体仅作模拟即可，该弹体本身无需开启网络复制
				SpawnedProjectile = World->SpawnActor<AProjectile>(
					ProjectileClass,
					SocketTransform.GetLocation(),
					TargetRotation,
					SpawnParams
				);
				// 生成的actor仅作模拟使用，因此也无需开启服务器倒带
				SpawnedProjectile->bUseServerSideRewind = false;
				// 将武器设置的伤害赋值给弹体，用于后续碰撞命中判定时应用伤害
				SpawnedProjectile->Damage = Damage;
				// 当前的爆头伤害设置，使用武器的爆头伤害将投射物的爆头伤害覆盖。
				SpawnedProjectile->HeadShotDamage = HeadShotDamage;
			}
			UE_LOG(LogTemp, Warning, TEXT("该武器没有开启服务器倒带！"));
		}
	}
}
