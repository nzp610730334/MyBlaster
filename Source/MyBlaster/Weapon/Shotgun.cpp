// Fill out your copyright notice in the Description page of Project Settings.


#include "Shotgun.h"


#include "Engine/SkeletalMeshSocket.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "MyBlaster/BlasterComponents/LagCompensationComponent.h"
#include "MyBlaster/PlayerController/BlasterPlayerController.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"

void AShotgun::FireShotgun(const TArray<FVector_NetQuantize>& HitTargets)
{
	/*
	 * 父类AHitScanWeapon的Fire方法不能满足要求，因此本来无需执行。
	 * 但父类AHitScanWeapon的父类AWeapon的Fire方法里面的相关逻辑如开火声音等是本类需要的，因此㤇执行AWeapon的Fire方法
	 * AWeapon::Fire()实际上没有使用到形参的HitTarget，因此传入一个空的FVector即可。
	 */
	AWeapon::Fire(FVector());

	// 获取本射线武器的所有者并转换为pawn
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	// 为空指针时世界返回
	if(OwnerPawn == nullptr) return;
	// 指定该Pawn的控制器为伤害来源方的控制器
	AController* InstigatorController = OwnerPawn->GetController();
	
	// 获取射线武器的网格体，通过网格体获取插槽
	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
	// 插槽获取成功
	if(MuzzleFlashSocket)
	{
		// 获取插槽的转换Tranform。需要传入插槽来自的骨骼网格组件(指的就是武器的网格体)
		const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
		// 枪口插槽的世界转换作为射线射出的起始点
		const FVector Start = SocketTransform.GetLocation();

		/*
		 * 一次霰弹可命中多个角色。因此需要构建映射存储每个角色的分别受击次数。
		 * 疑惑：HitMap记录的是命中的所有敌人，分别命中几次，用于累计伤害，但伤害的计算实际在服务器上，客户端本地机器是否可以使用if省掉这些步骤的开销
		 */
		// 不同角色被命中的次数记录
		TMap<ABlasterCharacter*, uint32> HitMap;
		// 不同角色被爆头命中次数记录
		TMap<ABlasterCharacter*, uint32> HeadShotHitMap;
		// 遍历本次开火的所有弹丸的随机散布的命中位点
		for(FVector_NetQuantize HitTarget : HitTargets)
		{
			// 初始化一个命中结果用于后续存储
			FHitResult FireHit;
			// 执行本次命中射击的射线方法
			WeaponTraceHit(Start, HitTarget, FireHit);
			
			/*
			 * Cast转换命中的目标为玩家角色。（因为只有命中了玩家角色才需要进行伤害计算）
			 * 这里在for循环中使用cast，是因为一次开火的多个弹丸可能命中多个角色，所以每个弹丸都独立cast，这是必要的开销，
			 * 后续的命中声效和视效也是每个弹丸单独处理的，因为一次开火可能命中多种Actor(敌人、地面、墙壁等)
			 */ 
			ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(FireHit.GetActor());
			/*
			 *  判断该次弹丸是否命中，并累计一次开火射击的弹丸命中数量，后面伤害应用时根据命中弹丸数量觉得伤害。
			 *  弹丸是否命中的三个判断条件与手枪一致。
			 * 1.受击角色BlasterCharacter成功被获取；
			 * 2.获取伤害来源方的控制器成功，在这里才判断是因为A玩家的控制器只存在他的本地和服务器上，其他玩家没有A的控制器，
			 * 如果在前面过早就判断控制器的获取，则后面的粒子效果等逻辑也无法执行；
			 * 3.仅在服务器上才做伤害处理。本射线武器通过射线检测在这里进行伤害处理，
			 * 而实弹武器则是仅在服务器上才生成实弹(然后网络复制到各客户端)，实弹武器的伤害处理在实弹命中目标后才执行。
			 */
			if(BlasterCharacter)
			{
				// 判断是否命中的骨骼是否是头部骨骼
				const bool bHeadShot = FireHit.BoneName.ToString() == FString("head");
				if(bHeadShot)
				{
					// 该角色不是第一次被本次射击的弹丸爆头，则增加其受击次数
					if(HeadShotHitMap.Contains(BlasterCharacter)) HeadShotHitMap[BlasterCharacter]++;
					// 否则将该角色添加进被爆头的映射中，并设置被爆头次数为1
					else HeadShotHitMap.Emplace(BlasterCharacter, 1);
				}
				else
				{
					// 该角色不是第一次被本次射击的弹丸命中，则增加其受击次数
                    if(HitMap.Contains(BlasterCharacter)) HitMap[BlasterCharacter]++;
                    // 否则将该角色添加进受击映射中，并设置命中次数为1
                    else HitMap.Emplace(BlasterCharacter, 1);
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
					// 世界上下文
					this,
					// 播放的声音
					HitSound,
					// 播放地点
					FireHit.ImpactPoint,
					// 音量倍数
					.5f,
					// 随机的音高乘数
					FMath::FRandRange(-.5f, .5f)
				);
			}
		}
		
		/*
		 * 开始伤害处理。遍历存储每个角色的分别受击次数的映射，累计伤害，分别作伤害应用
		 */
		// 初始化新的数组用于存储所有受击角色
		TArray<ABlasterCharacter*> HitCharacters;
		// 初始化新的映射，存储累计每个受击角色累计的受击伤害映射表(普通伤害+爆头伤害)
		TMap<ABlasterCharacter*, float> DamageMap;
		
		/*
		 * 先将存储所有受击角色。如果开火玩家在服务器上游玩，则顺便直接应用伤害计算即可。
		 * 疑惑：为什么是!bUseServerSideRewind
		 * 这样直接应用伤害，那么在服务器上游玩的玩家就没有开火爆头的单独计算了。
		 * 存储所有受击角色的行为不管是服务器玩家还是普通客户端玩家的机器，执行该行为都是需要的。是不是应该单独一个循环？虽然会多一个循环，但循环内只有一步，跟原来比但性能开销是差不多的。
		 */
		// 首先遍历并计算每个受击角色的普通伤害。 auto 自动适应的数据类型。从映射HitMap中遍历并取出每一个元素，一个元素是一个键值对，将其赋值给Hitpair。
		for(auto Hitpair : HitMap)
		{
			/*
			 * 这里有伤害应用的三个判断条件，因为其中HasAuthority的存在，所以实际本for循环只在服务器上才有意义，在所有客户端上相当于是一个空循环。
			 * Hitpair是玩家的指针和受击次数的键值对。Hitpair.Key则是玩家的指针。Hitpair.Value则是受击次数
			 */ 
			if(Hitpair.Key)
			{
				// 累计伤害映射表是否已有该角色，则累计伤害。本次累计伤害 = 普通受击次数 * 普通伤害
				if(DamageMap.Contains(Hitpair.Key)) DamageMap[Hitpair.Key] += Hitpair.Value * Damage;
				// 还没有该角色，则添加新的映射
				else DamageMap.Emplace(Hitpair.Key, Hitpair.Value * Damage);
				// 将受击角色加入存储受击角色的数组中，使用AddUnique添加了重复的角色
                HitCharacters.AddUnique(Hitpair.Key);
			}
		}
		// 接着遍历并计算每个受击角色的爆头伤害。遍历爆头映射表，处理过程与上面基本一致
		for(auto HeadShotHitPair : HeadShotHitMap)
		{
			// 累计伤害映射表是否已有该角色，则累计伤害。本次累计伤害 = 爆头受击次数 * 爆头伤害
			if(DamageMap.Contains(HeadShotHitPair.Key)) DamageMap[HeadShotHitPair.Key] += HeadShotHitPair.Value * HeadShotDamage;
			// 还没有该角色，则添加新的映射
			else DamageMap.Emplace(HeadShotHitPair.Key, HeadShotHitPair.Value * HeadShotDamage);
			// 将受击角色加入存储受击角色的数组中，使用AddUnique添加了重复的角色
			HitCharacters.AddUnique(HeadShotHitPair.Key);
		}

		// 遍历已经计算好的累计伤害映射表，对每个角色进行应用伤害
		for(auto DamagePair : DamageMap)
		{
			/*
             *  是否符合造成伤害的条件(未开启服务器倒带或者本地对伤害来源的玩家有控制权)
             *  疑惑：bool值的计算不够明晰，应该优化改进，提高代码阅读性
             */
            bool bCauseAuthDamage = !bUseServerSideRewind || OwnerPawn->IsLocallyControlled();
            // 当前机器是服务器且符合造成伤害的条件时，直接应用伤害传递即可
            if(HasAuthority() && bCauseAuthDamage)
            {
            	UGameplayStatics::ApplyDamage(
            		// 受击的玩家
            		DamagePair.Key,
            		// 总伤害
            		DamagePair.Value,
            		// 伤害来源方的玩家的控制器
            		InstigatorController,
            		// 造成该伤害的原因
            		this,
            		// 伤害类型
            		UDamageType::StaticClass()
            	);
            }
		}
		
		/*
		 * 处理非服务器上的玩家霰弹枪开火且武器开启了延迟补偿的情况，则需要向服务器发送霰弹枪开火RPC请求，进行命中判定和应用伤害。
		 */
		if(!HasAuthority() && bUseServerSideRewind)
		{
			// 获取并转换开火玩家的玩家角色
            BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : BlasterOwnerCharacter;
            // 获取并转换开火玩家的玩家控制器
            BlasterOwnerController = BlasterOwnerController == nullptr ? Cast<ABlasterPlayerController>(BlasterOwnerCharacter->Controller) : BlasterOwnerController;
            // 成功获取玩家角色以及玩家控制器，且玩家角色的延迟补偿组件不为空，且开火玩家有自主代理权(本地控制权)
            if(BlasterOwnerController && BlasterOwnerCharacter && BlasterOwnerCharacter->GetLagCompensation() && BlasterOwnerCharacter->IsLocallyControlled())
            {
            	// 调用开火玩家的演出补偿组件的霰弹枪开火PRC请求，请求服务器进行命中判定以及应用伤害
                BlasterOwnerCharacter->GetLagCompensation()->ShotgunServerScoreRequest(
                    HitCharacters,
                    Start,
                    HitTargets,
                    BlasterOwnerController->GetServerTime() - BlasterOwnerController->SingleTripTime
                    );
            }
		}
	}
}

void AShotgun::ShotgunTraceEndWithScatter(const FVector& HitTarget, TArray<FVector_NetQuantize>& HitTargets)
{
	/*
	 * 在枪口前方的一定距离生成一个球体，随机获取该球体内的一个点，从枪口到该点的方向即为本次单个弹丸的射击方向。
	 * 一个变量，如果在它的生命周期内它的值不会被修改，那么尽量使用const将其修饰
	 */
	// 获取射线武器的网格体，通过网格体获取插槽
	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
	// 插槽获取失败(为nullptr)，直接返回
	if(MuzzleFlashSocket == nullptr) return;
	// 获取插槽的转换Tranform。需要传入插槽来自的骨骼网格组件(指的就是武器的网格体)
	const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
	// 枪口插槽的世界转换作为射线射出的起始点
	const FVector TraceStart = SocketTransform.GetLocation();
	
	// 计算枪口到瞄准目标的单位方向
	const FVector ToTargetNormalized = (HitTarget - TraceStart).GetSafeNormal();
	// 计算世界关卡的散布球体的球心位置 = 枪口起始点 + 单位方向 * 预设置的球体半径
	const FVector SphereCenter = TraceStart + ToTargetNormalized * DistanceToSphere;
	
	// 根据武器的弹丸数量循环随机生成散布落点
	for(uint32 i = 0; i < NumberOfPellets; i++)
	{
		// 散布球体内随机一个点位置 = 随机方向的单位向量 * 随机半径。			备注： 注意区分FRandRange和RandRange
		const FVector RandVec = UKismetMathLibrary::RandomUnitVector() * FMath::FRandRange(0.f, SphereRadius);
		// 期望的世界关卡中的散布球体的随机点 = 世界关卡中的散布球体的球心 + 散布球体内的随机点
		const FVector EndLoc = SphereCenter + RandVec;
		// 枪口到散布球体的随机落点的向量 = 随机落点位置 - 枪口位置
		FVector ToEndLoc = EndLoc - TraceStart;
		// 射击的子弹的最终落点 = 枪口位置 + 枪口到散布球体的随机落点的向量 * 射线检测的长度 / 枪口到散布球体的随机落点的向量的大小			疑惑：为什么不直接在前面就将ToEndLoc标准化为单位向量
		ToEndLoc = TraceStart + ToEndLoc * TRACE_LENGTH / ToEndLoc.Size();
		
		// 将本次弹丸随机生成的散布落点加入数组HitTargets中
		HitTargets.Add(ToEndLoc);
	}
}
