// Fill out your copyright notice in the Description page of Project Settings.


#include "CombatComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MyBlaster/Character/BlasterCharacter.h"
// #include "MyBlaster/HUD/BlasterHUD.h"
#include "Components/BoxComponent.h"
#include "MyBlaster/BlasterCamera/BlasterMatineeCameraShake.h"
#include "MyBlaster/PlayerController/BlasterPlayerController.h"
#include "MyBlaster/Weapon/Projectile.h"
#include "MyBlaster/Weapon/ProjectileGrenade.h"
#include "MyBlaster/Weapon/Shotgun.h"
#include "MyBlaster/Weapon/Weapon.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundCue.h"


// Sets default values for this component's properties
UCombatComponent::UCombatComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...

	BaseWalkSpeed = 600.f;
	AimWalkSpeed = 415.f;
	// UE_LOG(LogTemp,Log,TEXT("UCombatComponent构造完毕！！！"));
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 需要网络复制的属性不仅要在声明时处理，还要在此处注册。
	// 无条件复制
	DOREPLIFETIME(UCombatComponent, EquippedWeapon);
	DOREPLIFETIME(UCombatComponent, SecondaryWeapon);
	DOREPLIFETIME(UCombatComponent, bAiming);
	// 有条件复制，条件COND_OwnerOnly表示本地机器对该类有控制权时才复制
	DOREPLIFETIME_CONDITION(UCombatComponent, CarriedAmmo, COND_OwnerOnly);
	DOREPLIFETIME(UCombatComponent, CombatState);
	// 原教程使用的是无条件复制。思考后角色应该使用有条件复制DOREPLIFETIME_CONDITION，节省服务器带宽。因为其他玩家客户端无需关心别的玩家的手榴弹。
	DOREPLIFETIME(UCombatComponent, Grenades);
	// DOREPLIFETIME_CONDITION(UCombatComponent, Grenades, COND_OwnerOnly);

	DOREPLIFETIME(UCombatComponent, bHoldingTheFlag);
	
}

void UCombatComponent::ShotgunShellReload()
{
	// 弹药更新的行为只在服务器上进行。客户端的弹药数量是通过网络复制更新的，
	if(Character && Character->HasAuthority())
	{
		UpdateShotgunAmmoValues();
	}
}

void UCombatComponent::PickupAmmo(EWeaponType WeaponType, int32 AmmoAmount)
{
	// 判断所拾取的弹药的武器类型是否存在
	if(CarriedAmmoMap.Contains(WeaponType))
	{
		// 不对玩家的后备弹药数量作限制
		// CarriedAmmoMap[WeaponType] += AmmoAmount;
		// 限制玩家的后备弹药数量
		CarriedAmmoMap[WeaponType] = FMath::Clamp(CarriedAmmoMap[WeaponType] + AmmoAmount, 0, MaxCarriedAmmo);
		// 更新玩家角色的后备弹药
		UpdateCarriedAmmo();
	}
	/*
	 * 优化：玩家装备了武器，弹匣打空，捡到对应武器类型的弹药时，自动装填
	 */
	if(EquippedWeapon && EquippedWeapon->IsEmpty() && EquippedWeapon->GetWeaponType() == WeaponType)
	{
		Reload();
	}
}

// Called when the game starts
void UCombatComponent::BeginPlay()
{
	// 这是本类的对象初始化后要进行的工作
	Super::BeginPlay();

	// ...
	if(Character)
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;

		// 判断角色摄像机是否存在
		if(Character->GetFollowCamera())
		{
			// 存储默认摄像机视野
			DefaultFOV = Character->GetFollowCamera()->FieldOfView;
			// 设置为当前摄像机视野
			CurrentFOV = DefaultFOV;
		}

		// 设置玩家的后备弹药，注意该行为只在服务器上执行
		// 先判断对该组件的角色对象有无权限(有则说明当前机器是服务器)
		if(Character->HasAuthority())
		{
			// 初始化各种武器的后备弹药数量
			InitializeCarriedAmmo();
		}
		
	}
}

// Called every frame
void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
	
	// 只在当前机器调试处理，无需复制（本角色对象是受控于当前本地机器时才执行下面的逻辑，因此本地机器的摸你代理不会执行，节省了性能）
	if(Character && Character->IsLocallyControlled())
	{
		// 绘制子弹飞行轨迹，需要的参数
		FHitResult HitResult;
		// 传入HitResult接收射线碰撞检测结果
		TraceUnderCrosshairs(HitResult);
		// 从结果中取出碰撞点
		HitTarget = HitResult.ImpactPoint;

		// 处理每帧准星的扩散和偏移
		SetHUDCrosshairs(DeltaTime);
		// 开火相机镜头抖动。已转移到localfire()中实现
		// if(bFireButtonPressed && !CanFire() && BlasterMatineeCameraShake)
		// {
		// 	Controller->ClientStartCameraShake(BlasterMatineeCameraShake);
		// 处理每帧鼠标镜头的后坐力上移(注意区分它和相机镜头抖动)
		// 	Controller->AddPitchInput(FMath::FInterpTo(
		// 		EquippedWeapon->OnFireShake.ShakeStart,
		// 		EquippedWeapon->OnFireShake.ShakeEnd,
		// 		DeltaTime,
		// 		EquippedWeapon->OnFireShake.ShakeInterpSpeed
		// 		));
		// }

		// 右键瞄准移动时相机抖动
		MoveCameraShake();
		
		// 开镜
		InterpFOV(DeltaTime);
	}
}

void UCombatComponent::SetHUDCrosshairs(float DeltaTime)
{
	// if(Character == nullptr && Character->GetController() == nullptr) return;
	if(Character == nullptr && Character->Controller == nullptr) return;
	// Cast消耗资源非常大，而本函数又将每帧都被调用，因此这里使用三元运算符，如果不为null，则赋值原来的值，减少资源消耗。
	// 需要注意：使用这种三元运算符的方式有弊端，即便Character->Controller发生变化了，Controller只要不为空指针，就不会发生Cast行为，Controller就不会更新。
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	if(Controller)
	{
		// Cast消耗资源非常大，而本函数又将每帧都被调用，因此这里使用三元运算符，如果不为null，则赋值原来的值，减少资源消耗。
		HUD = HUD == nullptr ? Cast<ABlasterHUD>(Controller->GetHUD()) : HUD;
		// HUD转换成功
		if(HUD)
		{
			// 装备武器时
			if(EquippedWeapon)
			{
				// 分别将武器的准星赋值给已初始化的准星结构体
				HUDPackage.CrosshairsCenter = EquippedWeapon->CrosshairsCenter;
				HUDPackage.CrosshairsLeft = EquippedWeapon->CrosshairsLeft;
				HUDPackage.CrosshairsRight = EquippedWeapon->CrosshairsRight;
				HUDPackage.CrosshairsTop = EquippedWeapon->CrosshairsTop;
				HUDPackage.CrosshairsBottom = EquippedWeapon->CrosshairsBottom;
			}
			else
			{
				// 未装备武器时，都设置为空指针
				HUDPackage.CrosshairsCenter = nullptr;
				HUDPackage.CrosshairsLeft = nullptr;
				HUDPackage.CrosshairsRight = nullptr;
				HUDPackage.CrosshairsTop = nullptr;
				HUDPackage.CrosshairsBottom = nullptr;
			}

			/*
			 * 计算角色移速对准星偏移的影响.
			 * 映射角色移速范围到0和1之间。如[0,600] -> [0, 1]
			 * 传入角色当前移速获取映射后的值。
			 */
			// 角色移速范围
			FVector2d WalkSpeedRange(0.f, Character->GetCharacterMovement()->MaxWalkSpeed);
			// 映射目标范围
			FVector2d VelocityMultiplierRange(0.f, 1.f);
			// 角色当前移速
			FVector Velocity = Character->GetVelocity();
			// 在此只关注水平面的速度，因此将Z轴设置为0
			Velocity.Z = 0.f;
			// 映射并接收结果,注意传入的速度是大小而非矢量
			CrosshairVelocityFactor = FMath::GetMappedRangeValueClamped(WalkSpeedRange, VelocityMultiplierRange, Velocity.Size());

			/*
			 * 计算角色跳跃或落下时对准星偏移的影响
			 */
			if(Character->GetCharacterMovement()->IsFalling())
			{
				// 传入当前CrosshairInAirFactor根据事件DeltaTime以速度2.25f插值到目标2.25f
				CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 2.25f, DeltaTime, 2.25f);
			}
			else
			{
				// 传入当前CrosshairInAirFactor根据事件DeltaTime以速度30.f插值到目标0.f
				CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 0.f, DeltaTime, 30.f);
			}

			// 计算角色瞄准时对准星偏移的影响.(角色瞄准时，准星缩小)
			if(bAiming)
			{
				// 这里提供的CrosshairAimFactor是正值，值越大，准星偏移越小。
				// 插值处理
				CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.58f, DeltaTime, 30.f);
			}
			else
			{
				// 非瞄准状态下回复偏移
				CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.f, DeltaTime, 30.f);
			}

			// 注意本函数在TickComponent中，因此本函数也是每帧调用。
			// 插值处理开火偏移归零.(游戏内表现：每次开火后，因开火导致的准星偏移减少。)
			CrosshairShootingFactor = FMath::FInterpTo(CrosshairShootingFactor, 0.f, DeltaTime, 1.f);
			
			// 计算总和，赋值给结构体HUDPackage。当前硬编码偏移基础值为0.5f
			HUDPackage.CrosshairSpread = 0.5f +
				CrosshairVelocityFactor +
					CrosshairInAirFactor -
						CrosshairAimFactor +
							CrosshairShootingFactor;
			// 将处理好的准星结构体传给给HUD
			HUD->SetHUDPackage(HUDPackage);
		}
	}
}

void UCombatComponent::FireCameraShake()
{
	// 本地没有控制权时，直接返回即可
	if(Controller == nullptr) return;
	// 开火相机镜头抖动，本地自主代理才需要镜头抖动，无需关注模拟代理的镜头抖动
	bool bShouldShake = Character->IsLocallyControlled() && EquippedWeapon->BlasterMatineeCameraShake && EquippedWeapon->bFireCameraShake;
	// if(bFireButtonPressed && !CanFire() && BlasterMatineeCameraShake)
	if(bShouldShake)
	{
		// 使用当前武器的镜头抖动相关参数
		// Controller->ClientStartCameraShake(EquippedWeapon->BlasterMatineeCameraShake);
		// 控制器偏移。视业务需求开启
		if(EquippedWeapon->bFireAddPitchInput)
		{
			// Controller->AddPitchInput(1.f);
			// Controller->AddPitchInput(0.f);
			Controller->AddPitchInput(FMath::FInterpTo(
				EquippedWeapon->OnFireShake.ShakeStart,
				EquippedWeapon->OnFireShake.ShakeEnd,
				1,
				EquippedWeapon->OnFireShake.ShakeInterpSpeed)
				);
		}
	}
}

void UCombatComponent::InterpFOV(float DeltaTime)
{
	// 校验装备的武器是否存在
	if(EquippedWeapon == nullptr) return;
	// 判断当前是否有开镜瞄准
	if(bAiming)
	{
		// 从当前视野插值到开镜视野
		CurrentFOV = FMath::FInterpTo(CurrentFOV, EquippedWeapon->GetZoomedFOV(), DeltaTime, EquippedWeapon->GetZoomInterpSpeed());
	}
	else
	{
		// 从当前视野插值到默认视野(非开镜状态)
		// CurrentFOV = FMath::FInterpTo(CurrentFOV, DefaultFOV, DeltaTime, EquippedWeapon->GetZoomInterpSpeed());
		// 使用战斗系统默认的收镜速度进行插值
		CurrentFOV = FMath::FInterpTo(CurrentFOV, DefaultFOV, DeltaTime, ZoomInterpSpeed);
	}
	// 判断玩家角色和玩家摄像机都非空
	if(Character && Character->GetFollowCamera())
	{
		// 设置玩家角色摄像机视野
		Character->GetFollowCamera()->SetFieldOfView(CurrentFOV);
	}
}

void UCombatComponent::SetAiming(bool bIsAiming)
{
	// 判断校验(视频教程中是且 && 而非或 ||，仔细考虑后我觉得应该改为或，任一条件不满足即返回)
	if(Character == nullptr || EquippedWeapon == nullptr) return;
	
	// 本地客户端调用SetAiming，这里马上设置bAiming，随后导致动画姿势改变
	bAiming = bIsAiming;
	// 非服务器的机器需要调用服务器RPC，让服务器知道本地客户端的玩家角色进行了瞄准行为
	if(!Character->HasAuthority())
	{
		SererSetAmiming(bIsAiming);
	}
	
	//
	if(Character)
	{
		// 瞄准中，设置瞄准速度，否则设置基本速度。
		// 注意：本函数SetAiming是在玩家本地机器上执行，所以速度修改只有本地优先生效，响应玩家输入，提升输入体验，但服务器上的玩家速度没有改变，
		// 因此还需要在SererSetAmiming中通知服务器更新玩家速度。
		Character->GetCharacterMovement()->MaxWalkSpeed = bIsAiming ? AimWalkSpeed : BaseWalkSpeed;
		// 备注：通常而言，网络复制从服务器到客户端，而客户端变更的属性是不会网络更新到服务端的，这不安全。一般使用RPC和服务端主动通信。
	}
	/*
	 * 1.角色需要在本地有控制权，因为本地机器上的其他玩家开镜不需要播放开镜UI，因为与本地机器的玩家无关。
	 * 2.当前装备的武器类型为狙击枪，因为目前只有狙击枪有开镜UI
	 */ 
	if(Character->IsLocallyControlled() && EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SniperRifle)
	{
		// 执行角色的狙击枪瞄准镜UI的播放方法
		Character->ShowSniperScopeWidget(bIsAiming);
	}
	/*
	 * 同步玩家鼠标右键按下和释放状态与角色是否瞄准状态。
	 * 注意，仅本地控制的玩家需要次操作，因为本玩家的客户端不可能为其他玩家进行客户端预测，
	 * 客户端预测就是客户端发起某种行为，在服务器回复客户端之前，客户端预测服务器会回复自己，因此自己先行一步，落实该种行为。
	 * 而本地玩家不可能为其他玩家的客户端发起某种行为，因此本玩家的客户端不需要为其他玩家处理客户端预测。
	 */ 
	if(Character->IsLocallyControlled()) bAimButtomPressed = bIsAiming;
}

// 服务器接收到SererSetAmiming后，会调用本函数
void UCombatComponent::SererSetAmiming_Implementation(bool bIsAiming)
{
	// 因为bAiming被设置了可网络复制，因此bAiming改变后，服务器会通知所有客户端改变它们机器上的与本角色有关的bAiming的值，
	// 从而使得本角色在所有客户端上都做出一样的姿势。
	// 最开始的调用SetAiming()的客户端也会收到复制通知，但因为已经在SetAiming()中先改变了bAiming,影响不大。
	// 一般而言，本地客户端执行一个角色动作，本地A优先响应做出动作，通知服务端本地做出的动作，服务端通知所有客户端（包括A）关于A做出了动作的通知，这种流程有延迟是正常的。
	bAiming = bIsAiming;
	if(Character)
	{
		// 本函数SererSetAmiming_Implementation只在在服务器执行。此处修改速度，网络复制到所有客户端。
		Character->GetCharacterMovement()->MaxWalkSpeed = bIsAiming ? AimWalkSpeed : BaseWalkSpeed;
	}
	// GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Blue, TEXT("收到RPC的SetAiming请求!"));
}

void UCombatComponent::OnRep_EquipWeapon()
{
	if(EquippedWeapon && Character)
	{
		/*
		 * 服务器两个RPC的依次调用，无法确保会按照期望的顺序到达客户端，因此这里需要额外处理武器附着到角色受伤的行为
		 */
		// 更新武器状态为已装备
		EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
		// 将装备的武器附着到玩家右手上
		AttachActorToRightHand(EquippedWeapon);
		// 设置角色方向移动不根据角色朝向
		Character->GetCharacterMovement()->bOrientRotationToMovement = false;
		// 设置角色朝向使用控制器Yaw方向
		Character->bUseControllerRotationYaw = true;
		// 播放装备武器的声音
        PlayEquipWeaponSound(EquippedWeapon);
		// 禁用该武器的自定义深度(模型轮廓高亮)
		EquippedWeapon->EnableCustomDepth(false);
		// 将该装备的武器的弹药信息刷新到弹药HUD上
		EquippedWeapon->SetHUDAmmo();
	}
}

void UCombatComponent::OnRep_SecondaryWeapon()
{
	// 确定玩家角色和副武器都存在
	if(SecondaryWeapon && Character)
	{
		// 将副武器的状态设置为已装备
		SecondaryWeapon->SetWeaponState(EWeaponState::EWS_EquippedSecondary);
		// 将副武器附着到玩家背包插槽
		AttachActorToBackpack(SecondaryWeapon);
		// 播放该副武器被装备时的声音,原教程播放的是EquipWeapon，手上的武器的声音，而非拾取到的被放到背包后面的SecondaryWeapon
		PlayEquipWeaponSound(SecondaryWeapon);
	}
}

void UCombatComponent::EquipWeapon(AWeapon* WeaponToEquip)
{
	// 为了安全，先校验是否存在
	if(Character == nullptr || WeaponToEquip == nullptr) return;

	// 战斗状态不符合条件时，直接返回
	if(CombatState != ECombatState::ECS_Unoccupied) return;

	// 拾取的是旗子时
	if(WeaponToEquip->GetWeaponType() == EWeaponType::EWT_Flag)
	{
		/*
		 * 根据业务需求，手持旗子时需要下蹲。注意：本函数在服务器上执行，服务器的玩家触发本函数，服务器设置下蹲的同时速度也被设置为下蹲的速度，
		 * 而客户端上的玩家角色却没有设置Crouch下蹲，因此需要在网络复制的bHoldingTheFlag的回调函数中设置下蹲
		 */ 
		Character->Crouch();
		// 手持旗子tag设置为true
		bHoldingTheFlag = true;

		// 注意这几步的顺序，其他武器可能需要先SetOwner，因为SetWeaponState里面访问了武器的新的所有者。
		// 设置旗子的状态为已装备
		WeaponToEquip->SetWeaponState(EWeaponState::EWS_Equipped);
		// 将滋滋附着到左手上
		AttachFlagToLefthand(WeaponToEquip);
		// 设置旗子所有者为本对象的所属角色
		WeaponToEquip->SetOwner(Character);
		TheFlag = WeaponToEquip;
		
		/*
		 * 注意：本函数只会被作为服务器的机器调用，因此A玩家拾取旗子后发生的下面的朝向变化，在玩家角色类的RotateInPlace中进行了处理，而RotateInPlace又在Tick中执行，因此这里注释掉即可。
		 */
		// 角色移动改为朝角色面向移动
		// Character->GetCharacterMovement()->bOrientRotationToMovement = true;
		// 关闭角色随控制器朝向转向
		// Character->bUseControllerRotationYaw = false;
	}
	else
	{
		// 当前只有主武器没有副武器时，将拾取的武器作为副武器装备
        if(EquippedWeapon != nullptr && SecondaryWeapon == nullptr)
        {
        	EquipSecondaryWeapon(WeaponToEquip);
        }
        // 以上以外的情况，将主武器丢弃，然后将拾取的武器作为主武器装备到手上。注意：正常情况下，不会出现EquippedWeapon为nullptr而SecondaryWeapon不为nullptr的情况
        else
        {
        	EquipPrimaryWeapon(WeaponToEquip);
        }
        
        // 注意：本函数只会被作为服务器的机器调用，因此A玩家拾取武器需要发生下面的朝向变化，只有服务器和其他玩家客户端更改了，A自己没有更新，A的更新在OnRep_EquipWeapon中
        // 设置角色方向移动不根据角色朝向
        Character->GetCharacterMovement()->bOrientRotationToMovement = false;
        // 设置角色朝向使用控制器Yaw方向
        Character->bUseControllerRotationYaw = true;
	}
}

void UCombatComponent::SwapWeapons()
{
	/*
	 * 交换主副武器
	 */
	// 角色未处于待机状态，不能切换武器，直接return
	if(CombatState != ECombatState::ECS_Unoccupied) return;

	// 播放切换武器动画
	Character->PlaySwapMontage();
	// 设置tag，玩家角色处于本地切换武器中（切换动作没有完成），该值会在动画即将结束的通知触发的函数中修改为true
	Character->bFinishedSwapping = false;
	// 将战斗状态设置为切换武器中
	CombatState = ECombatState::ECS_SwappingWeapons;
	
	// 创建临时变量，用于交换主副武器中的缓存行为。先将当前手上的武器(主武器)缓存
	AWeapon* TempWeapon = EquippedWeapon;
	// 将副武器(后背背包的武器)赋值给主武器
	EquippedWeapon = SecondaryWeapon;
	// 将临时缓存的武器赋值给副武器(后背背包的武器)
	SecondaryWeapon = TempWeapon;
	/*
	 * 注意：以上EquippedWeapon和SecondaryWeapon的改变会被网络复制，触发客户端上两者对应的OnRep函数。
	 * 疑惑：为什么交换主副武器中的行为不在PlaySwapMontage中动画通知触发的 FinishSwapAttachWeapons 中执行
	 */
	// 先禁用绘制武器轮廓的自定义深度。后续武器切换完毕后再打开
	SecondaryWeapon->EnableCustomDepth(false);
}

void UCombatComponent::FinishSwap()
{
	// 将战斗状态冲重新设置为待机状态。只需要在服务器上执行即可，CombatState会被网路复制更新到所有机器上
	if(Character && Character->HasAuthority())
	{
		CombatState = ECombatState::ECS_Unoccupied;
	}
	/*
	 * 切换武器的动画本地播放即将完毕，将tag设置回false.
	 */ 
	if(Character) Character->bFinishedSwapping = true;
	// 如果存在次要武器，则打开之前关闭的次要武器的轮廓的绘制(自定义深度)
	if(SecondaryWeapon)
	{
		SecondaryWeapon->EnableCustomDepth(true);
	}
}

void UCombatComponent::FinishSwapAttachWeapons()
{
	/*
	 * 新的主武器的逻辑处理
	 */
	// 变更新的主武器的状态为被装备
	EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
	// 将新的主武器附着到玩家角色右手
	AttachActorToRightHand(EquippedWeapon);
	// 将新的主武器的弹药信息刷新到弹药HUD上。注意，这只是在服务器上的刷新，这为在服务器上玩的玩家做的工作，客户端的玩家的弹药HUD刷新在OnRep_EquipWeapon中
	EquippedWeapon->SetHUDAmmo();
	// 根据新的主武器的武器类型检索角色对应的后备弹药数量，并将其刷新到HUD上
	UpdateCarriedAmmo();

	// 播放装备新的主武器的声音
	PlayEquipWeaponSound(EquippedWeapon);
	
	/*
	 * 新的副武器的逻辑处理
	 */
	// 变更新的主武器的状态为被放置到后背背包
	SecondaryWeapon->SetWeaponState(EWeaponState::EWS_EquippedSecondary);
	// 将新的主武器附着到玩家角色后背背包
	AttachActorToBackpack(SecondaryWeapon);
}

void UCombatComponent::EquipPrimaryWeapon(AWeapon* WeaponToEquip)
{
	//确保拾取的武器不为空，否则直接返回
	if(WeaponToEquip == nullptr) return;
	
	// 手上已经有武器了，则丢弃，拾取地上的
	DropEquippedWeapon();

	// 将要装备的武器赋值给玩家角色上的武器变量
	EquippedWeapon = WeaponToEquip;
	if(!Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("调用EquipPrimaryWeapon时，战斗组件没有Character！！！"));
	}
	// 设置武器所有者。该步骤需要在读取武器的Owner的步骤之前执行。比如后面的SetWeaponState和SetHUDAmmo都涉及到武器的Owner
	EquippedWeapon->SetOwner(Character);
	// 更新武器状态为已装备
	EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);

	// 将武器附加到玩家右手上
	AttachActorToRightHand(EquippedWeapon);
	
	// 更新玩家的弹匣子弹数量UI
	EquippedWeapon->SetHUDAmmo();

	// 根据当前手上的武器类型检索角色对应的后备弹药数量，并将其刷新到HUD上
	UpdateCarriedAmmo();

	// 播放装备武器的声音
	PlayEquipWeaponSound(WeaponToEquip);

	// 装备武器后，如果弹匣为空，则装填
	ReloadEmptyWeapon();

	// 禁用该武器的自定义深度(模型轮廓不高亮)。疑惑：该逻辑行为已经转移到OnWeaponState函数中，这里是否还有必要执行？
	EquippedWeapon->EnableCustomDepth(false);
}

void UCombatComponent::EquipSecondaryWeapon(AWeapon* WeaponToEquip)
{
	// 确保拾取到的武器不为空
	if(WeaponToEquip == nullptr) return;
	// 将拾取的武器设置为副武器
	SecondaryWeapon = WeaponToEquip;
	// 将拾取的武器(副武器)状态设置为已装备,这将处理碰撞和模型高亮等逻辑
	SecondaryWeapon->SetWeaponState(EWeaponState::EWS_EquippedSecondary);
	// 将副武器附着到玩家角色的背包
	AttachActorToBackpack(WeaponToEquip);
	// 播放该武器被装备的声音
	PlayEquipWeaponSound(SecondaryWeapon);
	
	// 原教程为 EquipWeapon == nullptr  意义不明
	if(Character == nullptr) return;
	// if(EquipWeapon == nullptr) return;
	// 将该武器的所有者设置为玩家角色
	SecondaryWeapon->SetOwner(Character);
}

void UCombatComponent::OnRep_Aiming()
{
	// 仅有对该玩家有控制权的机器才执行
	if(Character && Character->IsLocallyControlled())
	{
		// 使用本地机器表示玩家是否有按住鼠标右键的变量覆盖被服务器网络更新之后的变量
		bAiming = bAimButtomPressed;
	}
}

void UCombatComponent::DropEquippedWeapon()
{
	// 确保手上的武器存在
	if(EquippedWeapon)
	{
		// 丢弃武器
		EquippedWeapon->Dropped();
	}
}

void UCombatComponent::AttachActorToRightHand(AActor* ActorToAttach)
{
	// 确定玩家角色、玩家角色网格体、要附着的Actor的存在，否则直接返回
	if(Character == nullptr || Character->GetMesh() == nullptr || ActorToAttach == nullptr) return;
	
	// 取出玩家角色的用于持有武器的插槽
	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(FName("RightHandSocket"));
	UE_LOG(LogTemp, Warning, TEXT("手部插槽转换成功"));
	if(HandSocket)
	{
		// 传入Actor和骨架网格体，附着Actor到Socket
		HandSocket->AttachActor(ActorToAttach, Character->GetMesh());
		UE_LOG(LogTemp, Warning, TEXT("武器附着成功"));
	}
}

void UCombatComponent::AttachActorToLeftHand(AActor* ActorToAttach)
{
	/*
	 * 疑惑：原教程此处传入的是ActorToAttach，武器类型获取又是EquippedWeapon，二者逻辑上是一个东西，
	 * 但代码上是否应该使用ActorToAttach通过Cast转换成EquippedWeapon再获取武器类型？
	 */
	// 确定玩家角色、玩家角色网格体、要附着的Actor的存在，否则直接返回
	if(Character == nullptr || Character->GetMesh() == nullptr || ActorToAttach == nullptr || EquippedWeapon == nullptr) return;
	// 判断当前握持的武器类型
	bool bUsePistolSocket = EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Pistol ||
		EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SubmachineGun;
	// 根据武器类型确定要附着的角色手部插槽(步枪等长枪左手握持位置和手枪冲锋枪等短枪的位置不一样)
	FName SocketName = bUsePistolSocket ? FName("PistolSocket") : FName("LeftHandSocket");
	
	// 取出玩家角色的用于持有武器的插槽
	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(SocketName);
	if(HandSocket)
	{
		// 传入Actor和骨架网格体，附着Actor到Socket
		HandSocket->AttachActor(ActorToAttach, Character->GetMesh());
	}
}

void UCombatComponent::AttachFlagToLefthand(AActor* Flag)
{
	// 确定玩家角色、玩家角色网格体、要附着的Actor的存在，否则直接返回
	if(Character == nullptr || Character->GetMesh() == nullptr || Flag == nullptr) return;
	
	// 取出玩家角色的用于持有武器的插槽
	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(FName("FlagSocket"));
	if(HandSocket)
	{
		// 传入Actor和骨架网格体，附着Actor到Socket
		HandSocket->AttachActor(Flag, Character->GetMesh());
	}
}

void UCombatComponent::AttachActorToBackpack(AActor* ActorToAttach)
{
	// 确定玩家角色、玩家角色的网格体、要附着的武器都存在，否则直接return
	if(Character == nullptr || Character->GetMesh() == nullptr || ActorToAttach == nullptr) return;
	// 获取玩家角色的背包插槽，因为获取后不会再被修改，此处使用const修饰
	const USkeletalMeshSocket* BackpackSocket = Character->GetMesh()->GetSocketByName(FName("BackpackSocket"));
	// 获取成功
	if(BackpackSocket != nullptr)
	{
		// 将武器附着到玩家角色网格体的背包插槽
		BackpackSocket->AttachActor(ActorToAttach, Character->GetMesh());
		UE_LOG(LogTemp, Warning, TEXT("----------------顺利找到插槽BackpackSocket ！！！"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("找不到插槽BackpackSocket ！！！"));
	}
}

void UCombatComponent::UpdateCarriedAmmo()
{
	// 确定手上武器的存在，否则直接返回。
	if(EquippedWeapon == nullptr) return;
	// 判断映射表里面是否有对应的武器类型
	if(CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
	{
		// 根据map映射，获取当前装备的武器类型对应的后备弹药数量
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}
	else
	{
		CarriedAmmo = 0;
	}
	
	// 刷新玩家后备弹药数量UI，注意此处的SetHUD行为只发生在服务器上，还需要在只在客户端执行的回调函数OnRep_CarriedAmmo中添加SetHUD逻辑
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	if(Controller)
	{
		// 注意，传的参数CarriedAmmo是玩家身上携带的后备弹药
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
}

void UCombatComponent::PlayEquipWeaponSound(AWeapon* WeaponToEquip)
{
	// 确定玩家角色、装备的武器、武器上的设置的拾取声音都存在
	if(Character && WeaponToEquip && WeaponToEquip->EquipSound)
	{
		// 播放装备武器的声音
		UGameplayStatics::PlaySoundAtLocation(
			this,
			WeaponToEquip->EquipSound,
			WeaponToEquip->GetActorLocation()
		);
	}
}

void UCombatComponent::ReloadEmptyWeapon()
{
	// 当前手上的武器，如果弹匣为空，则装填
	if(EquippedWeapon->IsEmpty())
	{
		Reload();
	}
}

void UCombatComponent::Reload()
{
	/*
	 * 判断后备弹药数是否允许换弹。像此类需要向服务器发送RPC的行为都需要仔细考虑，避免发送无意义的RPC占用带宽
	 * 还需要判断战斗状态处于待机中才能执行，避免客户端发送RPC请求换弹后，还一直按R键发送RPC。
	 * 弹匣满时也不能换弹。
	 * 角色如果已经在本地换弹的过程中，则也不能进行换弹
	 */ 
	if(CarriedAmmo && CombatState == ECombatState::ECS_Unoccupied && EquippedWeapon && !EquippedWeapon->IsFull() && !bLocallyReloading)
	{
		ServerReload();
		// 本地机器先一步执行装弹处理
		HandleReload();
		// 设置角色本地进入换弹状态
		bLocallyReloading = true;
	}
}

void UCombatComponent::ServerReload_Implementation()
{
	if(Character == nullptr || EquippedWeapon == nullptr) return;
	// 设置战斗组件状态为装弹状态
	CombatState = ECombatState::ECS_Reloading;
	// 调用装弹函数。发起装弹的客户端已经先一步进行装弹处理，因此这里将其排除，值让该客户端在其他客户端中的模拟代理执行
	if(!Character->IsLocallyControlled()) HandleReload();
}

void UCombatComponent::OnRep_CombatState()
{
	/*
	 * 重构：是否应该重构为策略模式
	 */
	switch (CombatState)
	{
		// 当前战斗状态为换弹时，执行换弹行为
		case ECombatState::ECS_Reloading:
			// 发起装弹的客户端已经先一步进行了HandleReload，因此所有客户端的该角色的CombatState被网络更新后，这里只处理其他客户端的该角色的模拟代理即可
			if(Character && !Character->IsLocallyControlled()) HandleReload();
		break;
		// 当前战斗状态为待机时
		case ECombatState::ECS_Unoccupied:
			/*
			 * 玩家换弹前按下开火键直到换弹结束也未松开，则换弹完毕后需要接着开火
			 */
			if(bFireButtonPressed)
			{
				UE_LOG(LogTemp, Warning, TEXT("OnRep_CombatState执行触发的Fire！！"));
				Fire();
			}
		break;
		// 当前战斗状态为投掷手榴弹时
		case ECombatState::ECS_ThrowingGrenade:
			// 播放投掷手榴弹的动画，注意，发起投掷的玩家已经在本地执行过了，因此这里非本地控制才执行。(发起投掷的玩家在其他玩家机器中的模拟代理才播放动画)
			if(Character && !Character->IsLocallyControlled())
			{
				Character->PlayThrowGrenadeMontage();
				// 将当前装备的武器附着到左手(服务器上已经执行过本逻辑，会自动网络更新到所有客户端上，因此这里无需再附着)
				// AttachActorToLeftHand(EquippedWeapon);

				// 发起投掷行为的客户端和服务器都先一步显示了手榴弹，这里其他玩家的客户端上的该投掷手也需要显示手榴弹
				ShowAttachedGrenade(true);
			}
		break;
		// 当前状态为切换武器中时
		case ECombatState::ECS_SwappingWeapons:
			// 确保角色存在，且非本地控制，因为本地控制的该玩家角色已经先一步在他的本地播放过切换武器的动画了
			if(Character && !Character->IsLocallyControlled())
			{
				Character->PlaySwapMontage();
			}
		break;
	}
}

void UCombatComponent::OnRep_Grenades()
{
	// 更新玩家的手榴弹剩余数量
	UpdateHUDGrenades();
}

void UCombatComponent::UpdateHUDGrenades()
{
	// 获取玩家角色的控制器并转换
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->GetController()) : Controller;
	// 获取成功
	if(Controller)
	{
		// 调用玩家角色控制器刷新玩家剩余的手榴弹数量
		Controller->SetHUDGrenades(Grenades);
	}
}

void UCombatComponent::OnRep_HoldingTheFlag()
{
	// 角色存在且持有旗子且本地机器有控制权时
	if(bHoldingTheFlag && Character && Character->IsLocallyControlled())
	{
		// 设置下蹲姿势
		Character->Crouch();
	}
}

void UCombatComponent::MoveCameraShake()
{
	if(Controller == nullptr) return;
	// 获取角色移动速度
	FVector CharacterVelocity = Character->GetVelocity();
	CharacterVelocity.Z = 0.f;
	float Speed = CharacterVelocity.Size();
	// 瞄准状态下且移动中且已配置相机抖动参数
	bool bShouldShake = bAiming && BlasterMovingMatineeCameraShake && Speed > 0 ;
	if(bShouldShake)
	{
		Controller->ClientStartCameraShake(BlasterMovingMatineeCameraShake);
	}
}

bool UCombatComponent::ShouldSwapWeapons()
{
	return (EquippedWeapon != nullptr && SecondaryWeapon != nullptr);
}

void UCombatComponent::FinishedReload()
{
	// 玩家角色为空，则直接return
	if(Character == nullptr) return;
	// 将本地换弹状态设置回false
	bLocallyReloading = false;
	// 注意，只在服务器上修改战斗状态，在通过网络更新将战斗状态的属性更新到所有客户端。
	if(Character->HasAuthority())
	{
		// 将战斗状态设置回待机状态
		CombatState = ECombatState::ECS_Unoccupied;
		// 更新弹药
		UpdateAmmoValues();
	}
	/*
	 * 玩家可能在换弹过程中按下了开火键且不松开，那么换弹完毕需要开火。
	 * 疑惑：此处是否真的需要如此处理？因为OnRep_CombatState函数中已经对此种情况进行了处理。测试也没有问题，
	 * 两处都有处理时，触发的是OnRep_CombatState的代码。应该只在OnRep_CombatState处理即可
	 */
	if(bFireButtonPressed)
	{
		UE_LOG(LogTemp, Warning, TEXT("FinishedReload触发的Fire！！！"));
		Fire();
	}
}

void UCombatComponent::HandleReload()
{
	// 判断校验Character的存在
	if(Character)
	{
		// 播放角色的换弹动画。（装弹动画中有动画通知，调用FinishedReload函数，FinishedReload处理了装弹逻辑）
		Character->PlayReloadMontage();
	}
}

int32 UCombatComponent::AmountToReload()
{
	// 武器为空指针时，直接返回
	if(EquippedWeapon == nullptr) return 0;
	// 弹匣子弹空位 = 弹匣子弹最大值 - 弹匣当前子弹数
	int32 RoomInMag = EquippedWeapon->GetMagCapacity() - EquippedWeapon->GetAmmo();
	// 确保携带的弹药类型含有该种武器的弹药
	if(CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
	{
		// 获取该种弹药的携带数量
		int32 AmountCarried = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
		// 当前弹匣还能装填的子弹数量 = min(弹匣子弹空位，该种弹药的携带数量)
		int Least = FMath::Min(RoomInMag, AmountCarried);
		// Clamp确保返回的结果在一定范围内。传入RoomInMag给Clamp，RoomInMag大于Least时，输出Least，小于Least时，输出0。
		return FMath::Clamp(RoomInMag, 0, Least);
	}
	return 0;
}

void UCombatComponent::ThrowGrenade()
{
	/*
	 * 手榴弹数量为0，直接返回。此处的检测发生在客户端，服务器接收RPC后仍需要在该RPC的函数中再检测，
	 * 因为玩家可能修改它客户端的数值，通过了本条件检测，服务器上的数值无法被玩家私自修改，以服务期为准。
	 * Grenades == 0 应该改为使用 Grenades <= 0 ,避免逻辑出BUG，出现负数仍能通过检测。
	 */ 
	if(Grenades == 0) return;
	// 投掷手榴弹需要向服务器发送Replicated可靠RPC请求，为了防止玩家短时间连续多次点击投掷按键，避免客户端连续发送RPC增加服务器负担，需要限制客户端投掷行为，待机状态下才能投掷，不符合条件直接return
	// 新增条件，只有装备了武器的前提下才能投掷手榴弹，没有装备则直接返回
	// if(CombatState != ECombatState::ECS_Unoccupied || EquippedWeapon == nullptr) return;
	if(CombatState != ECombatState::ECS_Unoccupied && EquippedWeapon == nullptr) return;
	/*
	 * 设置战斗状态为投掷手榴弹状态。注意：此处CombatState的修改发生在客户端本地，
	 * CombatState网络复制设置只会从服务器复制到客户端，并不会从客户端复制到服务器，因此还需要在给服务器的RPC中修改CombatState的值。
	 */ 
	CombatState = ECombatState::ECS_ThrowingGrenade;
	// 直接在本地播放投掷动画。不等待服务器的多播，直接响应玩家的操作，可提升玩家体验。
	if(Character)
	{
		Character->PlayThrowGrenadeMontage();
		// 右手投掷手榴弹，则需要将当前装备的武器附着到左手上。后续投掷完毕后再附着回右手
		AttachActorToLeftHand(EquippedWeapon);
		// 显示手上的手榴弹。这里只在本地显示了，服务器上以及其他玩家的客户端也需要显示
		ShowAttachedGrenade(true);
	}
	// 确保当前机器非服务器，然后向服务器发送RPC申请投掷手榴弹
	if(Character && !Character->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("向服务器发送RPC投掷手榴弹！"));
		ServerThrowGrenade();
	}
	// 当该玩家是在服务器上玩，且需要投弹时，则不用再发送RPC了，补充投弹流程剩下的扣除手榴弹数量的逻辑即可。直接使用else关键字结合上一个if处理本代码块可能出现逻辑上的漏洞。
	if(Character && Character->HasAuthority())
	{
		// 扣除本次投掷消耗的手榴弹数量。使用Clamp数学函数将扣除后的数值保持在符合业务逻辑的范围内
		Grenades = FMath::Clamp(Grenades - 1, 0, MaxGrenades);
		/*
		 * 更新玩家角色的手榴弹UI中的剩余数。注意这只是在服务器上的操作，为了给在服务器上玩的玩家刷新他的手榴弹数
		 * 通常投弹的玩家都在客户端上玩，因此还需要在客户端的战斗组件的Grenades的回调函数OnRep_Grenades中执行UpdateHUDGrenades()
		 * 服务器的属性不会被网络更新，因此不会执行回调函数OnRep_Grenades，因此UI的刷新放到这里执行。
		 */ 
		UpdateHUDGrenades();
	}
}

void UCombatComponent::ServerThrowGrenade_Implementation()
{
	/*
	 * 手榴弹数量客户端已做判断，但客户端不可信，玩家可能私自修改，因此在服务器这里仍需要检测。
	 * Grenades == 0 应该改为使用 Grenades <= 0 ,避免逻辑出BUG，出现负数仍能通过检测。
	 */
	if(Grenades == 0) return;
	UE_LOG(LogTemp, Warning, TEXT("-服务器收到客户端RPC请求投掷手榴弹！"));
	// 设置战斗状态。CombatState会被网络复制到所有客户端，其回调函数中执行动画播放等内容。
	CombatState = ECombatState::ECS_ThrowingGrenade;
	// 在服务器上播放投掷动画
	if(Character)
	{
		Character->PlayThrowGrenadeMontage();
		/*
		 * 将当前装备的武器附着到左手，本函数是在服务器上执行的，因此附着行为会复制到所有客户端。
		 * 疑惑：包括发起该RPC的客户端？那是否会出现发起的客户端附着到左手，投掷完毕附着回右手，这时候服务器的网络复制才来，该客户端的武器又被附着到左手了？
		 */ 
		AttachActorToLeftHand(EquippedWeapon);
		// 显示手上的手榴弹。这里只在服务器显示了，其他玩家的客户端也需要显示
		ShowAttachedGrenade(true);
	}
	// 扣除本次投掷消耗的手榴弹数量。使用Clamp数学函数将扣除后的数值保持在符合业务逻辑的范围内
	Grenades = FMath::Clamp(Grenades - 1, 0, MaxGrenades);
	/*
	 * 更新玩家角色的手榴弹UI中的剩余数。注意这只是在服务器上的操作，为了给在服务器上玩的玩家刷新他的手榴弹数
	 * 通常投弹的玩家都在客户端上玩，因此还需要在客户端的战斗组件的Grenades的回调函数OnRep_Grenades中执行UpdateHUDGrenades()
	 */ 
	UpdateHUDGrenades();
}

void UCombatComponent::ShowAttachedGrenade(bool bShowGrenade)
{
	// 先确定玩家角色和手上的手榴弹都存在
	if(Character && Character->GetAttachedGrenade())
	{
		// 隐藏或者显示
		Character->GetAttachedGrenade()->SetVisibility(bShowGrenade);
	}
}

/*
 * 客户端本地机器调用 FireButtonPressed()，条件检测通过，调用ServerFire()发送RPC请求给服务器。
 * 服务器本地接收到客户端的RPC请求后，服务器本地机器调用ServerFire_Implementation().
 * ServerFire_Implementation()调用MulticastFire_Implementation(),在所有机器上执行角色开火抖动动画和武器开火动画声效。
 */
void UCombatComponent::FireButtonPressed(bool bPressed)
{
	// 存储是否开火标记
	bFireButtonPressed = bPressed;
	// 校验是否开火且是否已装备武器
	if(bFireButtonPressed && EquippedWeapon)
	{
		Fire();
	}
}

void UCombatComponent::Fire()
{
	if(CanFire())
	{
		// 进入开火流程，设置为false
		bCanFire = false;
		
		// 初始化命中结果
	    // FHitResult HitResult;
	    // 获取当前准星瞄准的射线检测命中结果(传入的参数是引用，目标位置将被存储在HitResult中)
	    // TraceUnderCrosshairs(HitResult);
		
		// 开火后的准星偏移处理，先确定装备的武器存在
        if(EquippedWeapon)
        {
        	// 设置射击准星偏移。此处使用硬编码，后期应该优化为每种武器有各自的开火准星偏移
        	CrosshairShootingFactor = 0.75f;
        	// 根据武器的弹丸开火类型，执行不同的开火方式
        	switch (EquippedWeapon->FireType)
        	{
        	case EFireType::EFT_Projectile:
        		FireProjectileWeapon();
        		break;
        	case EFireType::EFT_HitScan:
        		FireHitScanWeapon();
        		break;
        	case EFireType::EFT_Shotgun:
        		FireShotgun();
        		break;
        	}
        }
		/*
		 * 因为定时器的调用时要经过第一个时间间隔之后才会调用，表现在全自动开火场景下，玩家按下开火键后要延迟一段时间才开火，影响体验。
		 * 因此在定时器之前，就先调用了一次开火ServerFire函数，然后开始定时器函数StartFireTimer，进入周期定时开火流程。
		 */
        StartFireTimer();
	}
}

void UCombatComponent::FireProjectileWeapon()
{
	// 先确定装备的武器存在
	if(EquippedWeapon && Character)
	{
		// 根据该武器的弹丸散布的设置tag，确定该武器的本次开火射击弹道终点是经过随机散布处理后的落点还是战斗组件ComBat的瞄准点
		HitTarget = EquippedWeapon->bUseScatter ? EquippedWeapon->TraceEndWithScatter(HitTarget) : HitTarget;
		
		/*
		 * 发起开火的玩家先一步在本地机器上播放开火的视效和声效。
		 * 注意，在服务器上游玩的玩家，如果本地执行LocalFire，自己给自己发RPC执行ServerFire，那么他就经历了两次SpendRound扣除弹药的过程，因此服务器上不执行LocalFire或者ShotgunLocalFire
		 */ 
        if(!Character->HasAuthority()) LocalFire(HitTarget);
        // RPC调用服务器的开火函数
        // ServerFire(HitResult.ImpactPoint);	// TraceUnderCrosshairs函数已经在TickComponent中调用并将结果赋值HitTarget
        // UE_LOG(LogTemp, Warning, TEXT("开始调用ServerFire"))
        ServerFire(HitTarget, EquippedWeapon->FireDelay);
	}
}

void UCombatComponent::FireHitScanWeapon()
{
	// 先确定装备的武器存在
	if(EquippedWeapon || Character)
	{
		// 根据该武器的弹丸散布的设置tag，确定该武器的本次开火射击弹道终点是经过随机散布处理后的落点还是战斗组件ComBat的瞄准点
		HitTarget = EquippedWeapon->bUseScatter ? EquippedWeapon->TraceEndWithScatter(HitTarget) : HitTarget;
		
		/*
		* 发起开火的玩家先一步在本地机器上播放开火的视效和声效。
		* 注意，在服务器上游玩的玩家，如果本地执行LocalFire，自己给自己发RPC执行ServerFire，那么他就经历了两次SpendRound扣除弹药的过程，因此服务器上不执行LocalFire或者ShotgunLocalFire
		*/
		if(!Character->HasAuthority()) LocalFire(HitTarget);
		// RPC调用服务器的开火函数
		// ServerFire(HitResult.ImpactPoint);	// TraceUnderCrosshairs函数已经在TickComponent中调用并将结果赋值HitTarget
		// UE_LOG(LogTemp, Warning, TEXT("开始调用ServerFire"))
		ServerFire(HitTarget, EquippedWeapon->FireDelay);
	}
}

void UCombatComponent::FireShotgun()
{
	// 将装备武器cast转换为霰弹枪武器
	AShotgun* Shotgun = Cast<AShotgun>(EquippedWeapon);
	// 转换成功
	if(Shotgun && Character)
	{
		// 初始化数组用于存储本次射击产生的所有随机散布点
		// TArray<FVector_NetQuantize> HitTargets;
		TArray<FVector_NetQuantize> HitTargets;
		// 将本次射击产生的所有随机散布点加入到数组HitTargets中
		Shotgun->ShotgunTraceEndWithScatter(HitTarget, HitTargets);
		/*
		* 发起开火的玩家先一步在本地机器上调用复数射线武器的开火的视效和声效。
		* 注意，在服务器上游玩的玩家，如果本地执行LocalFire，自己给自己发RPC执行ServerFire，那么他就经历了两次SpendRound扣除弹药的过程，因此服务器上不执行LocalFire或者ShotgunLocalFire
		*/
		if(!Character->HasAuthority()) ShotgunLocalFire(HitTargets);
		// 本地机器发送RPC开火请求给服务器
		ServerShotgunFire(HitTargets, EquippedWeapon->FireDelay);
	}
}

void UCombatComponent::LocalFire(const FVector_NetQuantize& TraceHitTarget)
{
	// 未装备武器时，直接返回
	if(EquippedWeapon == nullptr) return;
	
	// 角色不为空且战斗状态处于待机
	if(Character && CombatState == ECombatState::ECS_Unoccupied)
	{
		// 播放角色开火动画
		Character->PlayFireMontage(bAiming);
		// HitTarget只是本地机器的命中目标位置，A玩家机器开火产生新的HitTarget，其他玩家也就是模拟代理的机器上的HitTarget不会被更新
		// 因此这里需要使用服务器接收的A玩家调用RPC时网络传送过来的TraceHitTarget作为模拟代理的机器上的HitTarget
		// EquippedWeapon->Fire(HitTarget);
		// 枪械开火。处理枪械的开火动画(扳机的运动等)、弹壳的生成等
		EquippedWeapon->Fire(TraceHitTarget);

		// 执行开火时相机镜头的抖动
		FireCameraShake();
	}
	
	/*
	 * to do：
	 * 如果当前角色是当前机器的玩家控制，则需要如下处理：
	 * 在此计算该武器的后坐力导致玩家鼠标输入偏移的结构体OffsetCausedByRecoilForce。
	 * 在此处施加给玩家额外的鼠标输入，以体现后坐力。
	 * 备注：需要在该玩家的控制器的tick处对玩家的鼠标输入进行补正(即后坐力导致鼠标镜头偏移后的恢复)
	 */
	// 条件判断，AI角色的Controller属于ABlasterPlayerController，也不属于玩家controller，没有AddPitchInput
	if(Controller != nullptr && Controller->IsLocalController())
	{
		UE_LOG(LogTemp, Warning, TEXT("本地控制，应用后坐力，允许额外的鼠标输入！！！"));
		// 额外的鼠标输入
		Controller->AddPitchInput(-0.1);
	}
	
}

void UCombatComponent::ShotgunLocalFire(const TArray<FVector_NetQuantize>& TraceHitTargets)
{
	// 将当前装备的武器转换为霰弹枪
	AShotgun* Shotgun = Cast<AShotgun>(EquippedWeapon);
	// Shotgun转换失败或者玩家角色为空，则直接return
	if(Shotgun == nullptr || Character == nullptr) return;
	/*
	 * 霰弹枪一发一发装填的过程中，也需要允许开火，因此这里需要为霰弹枪特殊处理。
	 */
	if(CombatState == ECombatState::ECS_Reloading || CombatState == ECombatState::ECS_Unoccupied)
	{
		// 恢复本地装弹tag，否则霰弹枪装填中开火会出现bug。疑惑：为什么不是在CanFire中恢复？在那里处理应该更合理。
		bLocallyReloading = false;
		// 播放角色开火动画
		Character->PlayFireMontage(bAiming);

		// 执行开火时相机镜头的抖动
		FireCameraShake();
		
		// 霰弹枪武器开火
		Shotgun->FireShotgun(TraceHitTargets);
		// 战斗状态归位为待机状态。因为是装填中开火，玩家角色动画可能并没有能播放到发送装填结束动画通知的进度
		CombatState = ECombatState::ECS_Unoccupied;
	}
}

void UCombatComponent::ServerFire_Implementation(const FVector_NetQuantize& TraceHitTarget, float FireDelay)
{
	/*
	 * 放作弊的测试，不符合条件直接返回，则仅在开火玩家的客户端本地看到开火行为，同时会导致后续一些列弹药扣除相关的BUG
	 */
	if(EquippedWeapon && !FMath::IsNearlyEqual(EquippedWeapon->FireDelay, FireDelay, 0.0001f))
	{
		return;
	}
	
	// 服务器给所有客户端发送多播，通知它们执行开火逻辑
	MulticastFire(TraceHitTarget);
}

bool UCombatComponent::ServerFire_Validate(const FVector_NetQuantize& TraceHitTarget, float FireDelay)
{
	/*
	 * 本函数return true，则验证通过，服务器执行RPC的函数。
	 * 本函数return false，则验证失败，玩家会被踢出游戏。
	 */
	if(EquippedWeapon)
	{
		// 判断是否几乎相等。分别传入 服务器本地存储的开火间隔、客户端发送来的RPC里面的开火间隔、允许的误差大小
		bool IsNearEqual = FMath::IsNearlyEqual(EquippedWeapon->FireDelay, FireDelay, 0.0001f);
		// 直接将计算结果作为返回结果
		return IsNearEqual;
	}
	// 没有武器的情况下，即便开火间隔有变化也不会影响游戏，返回true即可
	return true;
}

void UCombatComponent::MulticastFire_Implementation(const FVector_NetQuantize& TraceHitTarget)
{
	/*
	 * 发起开火的玩家已经先一步进行了开火视效和声效的处理，因此这里将其排除，只在还未播放视效声效的机器上执行开火的视效和声效
	 * Character && Character->IsLocallyControlled() && !Character->HasAuthority()  三个条件定位到有控制权但非权威的机器，即发起开火的玩家(自主代理)
	 */ 
	if(Character && Character->IsLocallyControlled() && !Character->HasAuthority()) return;
	// 在本地机器上播放开火的视效和声效
	LocalFire(TraceHitTarget);
}

void UCombatComponent::ServerShotgunFire_Implementation(const TArray<FVector_NetQuantize>& TraceHitTargets, float FireDelay)
{
	// 服务器给所有客户端发送多播，通知它们执行开火逻辑
	MulticastShotgunFire(TraceHitTargets);
}

bool UCombatComponent::ServerShotgunFire_Validate(const TArray<FVector_NetQuantize>& TraceHitTargets, float FireDelay)
{
	if(EquippedWeapon)
	{
		bool IsNearEqual = FMath::IsNearlyEqual(EquippedWeapon->FireDelay, FireDelay, 0.0001f);
		return IsNearEqual;
	}
	return true;
}

void UCombatComponent::MulticastShotgunFire_Implementation(const TArray<FVector_NetQuantize>& TraceHitTargets)
{
	/*
	 * 发起开火的玩家已经先一步进行了开火视效和声效的处理，因此这里将其排除，只在还未播放视效声效的机器上执行开火的视效和声效
	 * Character && Character->IsLocallyControlled() && !Character->HasAuthority()  三个条件定位到有控制权但非权威的机器，即发起开火的玩家(自主代理)
	 */ 
	if(Character && Character->IsLocallyControlled() && !Character->HasAuthority()) return;
	// 在本地机器上播放复数射线武器的开火的视效和声效
	ShotgunLocalFire(TraceHitTargets);
}

bool UCombatComponent::CanFire()
{
	// 判断是否持有武器
	if(EquippedWeapon == nullptr) return false;
	/*
	 * 霰弹枪一发一发装填的过程中，也需要允许开火，因此这里需要为霰弹枪特殊处理。
	 * 条件：武器弹药不为空、允许开火、战斗状态为装填中、武器类型为霰弹枪
	 */
	if(!EquippedWeapon->IsEmpty() && bCanFire && CombatState == ECombatState::ECS_Reloading && EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Shotgun)
	{
		// 因为装填过程中开了火，因此bLocallyReloading重置为false。这是原教程没有的处理。
		bLocallyReloading = false;
		return true;
	}
	// 判断角色是否处于本地换弹状态中。（该判断放在前面霰弹枪装填中开火的判断检查之后，否则处于整天状态直接就return false了，导致霰弹枪不能一发发装填中开火）
	if(bLocallyReloading)
	{
		UE_LOG(LogTemp, Warning, TEXT("bLocallyReloading仍旧为true，无法开火！！！"));
		return false;
	}
	
	// 射击间隔是否已过，且武器弹药校验是否通过，战斗状态是否处于待机
	// return !bCanFire || !EquippedWeapon->IsEmpty();		// 教程原代码，似乎有误。
	return bCanFire && !EquippedWeapon->IsEmpty() && CombatState == ECombatState::ECS_Unoccupied;
}

void UCombatComponent::StartFireTimer()
{
	// 判断武器和角色是否存在
	if(EquippedWeapon == nullptr || Character == nullptr) return;
	UE_LOG(LogTemp, Warning, TEXT("开火定时器开始工作"));
	// 获取character的定时器实例并设置定时器
	Character->GetWorldTimerManager().SetTimer(
		// 如果传入的句柄引用了现有的计时器，则在添加新计时器之前，它将被清除。无论在哪种情况下，都会返回新计时器的新句柄。
		FireTimer,
		// 调用计时器函数的对象。
		this,
		// 计时器触发时要调用的方法。
		&UCombatComponent::FireTimerFinished,
		// 设定和发射之间的时间（以秒为单位）。如果<=0.f，则清除现有计时器。
		EquippedWeapon->FireDelay
	);
}

void UCombatComponent::FireTimerFinished()
{
	if(EquippedWeapon == nullptr) return;
	// 开火流程完成，设置回true。
	// bCanFire在此处才归位，是考虑到即便半自动武器连续多次点击，也依然需要经过开火间隔，才能再次开火。
	// 而开火间隔的实现是通过定时器对本函数的间隔执行实现的。必须经过一定时间(开火间隔)，bCanFire才应该归位。
	bCanFire = true;
	// 按下开火键且武器为全自动类型时
	if(bFireButtonPressed && EquippedWeapon->bAutoMatic)
	{
		Fire();
	}

	// 开火过程中，弹匣子弹耗尽，则直接装填
	ReloadEmptyWeapon();
	UE_LOG(LogTemp, Warning, TEXT("FireTimerFinished调用完毕"));
}

void UCombatComponent::UpdateAmmoValues()
{
	// 判断角色对象和武器对象是否存在
	if(Character == nullptr || EquippedWeapon == nullptr) return;
	// 获取要装填的子弹数量
	int32 ReloadAmount = AmountToReload();
	// 确保后备弹药中有该种武器的弹药
	if(CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
	{
		// 扣除该种弹药的后备数量
		CarriedAmmoMap[EquippedWeapon->GetWeaponType()] -= ReloadAmount;
		/*
		 * 要显示在UI上的该种弹药后备数量。注意CarriedAmmo这种变量是会被网络更新复制，因此应当尽量少改动，计算好最终值后才对其赋值，然后才网络复制更新，减少宽带占用。
		 */
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}
	
	/*
	 * 客户端的CarriedAmmo因为网络更新而调用回调函数OnRep_CarriedAmmo，OnRep_CarriedAmmo中更新了HUD，但服务器的HUD并没有更新，所以此处需要处理。
	 */
	// 获取控制器
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	// 获取成功
	if(Controller)
	{
		// 设置携带的武器弹药数量的HUD
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
	// 装填弹匣的子弹，注意，装填函数要求传入的是负值。
	// 教程P167的07:24，已修改为+法计算
	EquippedWeapon->AddAmmo(ReloadAmount);
}

void UCombatComponent::UpdateShotgunAmmoValues()
{
	// 判断角色和武器对象任一为空则不满足条件，直接返回
	if(Character == nullptr || EquippedWeapon == nullptr) return;
	// 获取要装填的子弹数量(霰弹枪装填数量每次为1)
	// 确保后备弹药中有该种武器的弹药
	if(CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType())){
		// 扣除该种弹药的后备数量（霰弹枪每次装填1，则扣除也是1）
		CarriedAmmoMap[EquippedWeapon->GetWeaponType()] -= 1;
		// 要显示在UI上的后备弹药的数量
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}
	
	/*
	 * 客户端的CarriedAmmo因为网络更新而调用回调函数OnRep_CarriedAmmo，OnRep_CarriedAmmo中更新了HUD，但服务器的HUD并没有更新，所以此处需要处理。
	*/
	// 获取玩家控制器
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->GetController()) : Controller;
	// 获取成功
	if(Controller)
	{
		// 设置携带的武器弹药数量的HUD
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
	// 装填弹匣的子弹，注意，装填函数要求传入的是负值。
	// 教程P167的07:24，已修改为+法计算
	EquippedWeapon->AddAmmo(1);

	// 霰弹枪一发装填完毕，则允许开火
	bCanFire = true;
	
	// 弹匣内的弹药是否已经填满或者后备弹药已为0，则动画跳转到装填完毕的section部分
	if(EquippedWeapon->IsFull() || CarriedAmmo == 0)
	{
		// 动画跳转。注意：本函数仅在服务器上被调用，客户端上没有检查弹药已满并跳转的逻辑，因此需要在武器的弹药网络复制的回调函数OnRep_Ammo中补充类似逻辑。
		JumpToShotgunEnd();
	}
}

void UCombatComponent::JumpToShotgunEnd()
{
	// 获取玩家角色网格体的动画实例
	if(Character)
	{
		UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
		// 获取成功且装填动画存在
		if(AnimInstance && Character->GetReloadMontage())
		{
			// 跳转玩家角色的网格体的动画到霰弹枪装填结束段落
			AnimInstance->Montage_JumpToSection(FName("ShotgunEnd"));
		}
	}
}

void UCombatComponent::ThrowGrenadeFinished()
{
	// 归位战斗状态为待机状态
	CombatState = ECombatState::ECS_Unoccupied;
	// 手榴弹投掷动作完毕，将当前装备的武器重新附着回右手
	if(EquippedWeapon)
	{
		AttachActorToRightHand(EquippedWeapon);
	}
}

void UCombatComponent::LaunchGrenade()
{
	// 将其设置为不可视
	ShowAttachedGrenade(false);
	// 客户端发送RPC到服务器请求投掷手榴弹，注意。只有对该角色有本地控制权的客户端才发送，否则会出现玩家A在自己客户端上触发本函数，所有玩家的客户端中的玩家A角色都发送RPC
	if(Character && Character->IsLocallyControlled())
	{
		// 传入鼠标镜头瞄准的目标，发送RPC请求投掷手榴弹
		ServerLauncherGrenade(HitTarget);
	}
}

void UCombatComponent::ServerLauncherGrenade_Implementation(const FVector_NetQuantize& Target)
{
	// 生成手榴弹的行为只在服务器上发生，且需要确定玩家角色已经存在，玩家的手榴弹已经设置
	if(Character && GrenadeClass && Character->GetAttachedGrenade())
	{
		// 获取玩家手上的手榴弹的世界位置。为了防止手误修改，应声明为const
		const FVector StartingLocation = Character->GetAttachedGrenade()->GetComponentLocation();
		// 使用客户端发送RPC传过来的瞄准目标Target，计算投掷方向
		FVector ToTarget = Target - StartingLocation;
		// 初始化生成参数
		FActorSpawnParameters SpawnParams;
		// 设置要生成的Actor的拥有者，后续伤害计算需要用到
		SpawnParams.Owner = Character;
		// 设置要生成的Actor的煽动者，后续伤害计算需要用到
		SpawnParams.Instigator = Character;
		// 获取世界
		UWorld* World = GetWorld();
		// 获取成功
		if(World)
		{
			// SpawnActor的模板化版本，允许您通过参数在返回类型为该类型的父类时指定旋转和位置
			AProjectile* SpwanedProjectile = World->SpawnActor<AProjectile>(
				// 要生成的类
				GrenadeClass,
				// 生成位置
				StartingLocation,
				// 生成之后的朝向
				ToTarget.Rotation(),
				// 生成参数
				SpawnParams
				);
			if(SpwanedProjectile)
			{
				/*
				 * 处理生成的手雷的碰撞。避免发生碰撞玩家角色的胶囊体导致轨迹异常
				 */
				// 获取碰撞盒子
				UBoxComponent* CollisionBox = SpwanedProjectile->GetCollisionBox();
				// 先设置所有碰撞通道忽略
				CollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
				// 单独设置需要的碰撞通道，响应为阻塞（碰撞）
				CollisionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
				CollisionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);
				CollisionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Block);
				// 启用碰撞检测，类型为QueryAndPhysics
				CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			}
			
		}
	}
}

void UCombatComponent::TraceUnderCrosshairs(FHitResult& TraceHitResult)
{
	/*
	 *  每帧执行的函数中执行本函数，进行射线检测
	 *  或 按下开火按键后，才执行本函数，进行射线检测
	 */
	// 获取窗口中心的十字准星位置
	//
	FVector2d ViewportSize;
	if(GEngine && GEngine->GameViewport)
	{
		// 获取窗口大小并存储在ViewportSize中。
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	// 计算十字准星的位置
	FVector2d CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
	// 实例化新的Fvector后面用于存储十字准星在游戏关卡中的世界位置和方向
	FVector CrosshairWorldPosition;
	FVector CrosshairWorldDirection;

	// 获取窗口的十字准星位置在游戏关卡中对应的世界位置和方向，结果存储在CrosshairWorldPosition和CrosshairWorldDirection中
	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(
		// this，这里也就是COmbatComponent的对象自己，获取this所属的玩家控制器的队列，本地玩家控制器在该队列中的索引通常为0
		UGameplayStatics::GetPlayerController(this, 0),
		CrosshairLocation,
		CrosshairWorldPosition,
		CrosshairWorldDirection
	);
	// 成功获取，生成射线检测
	if(bScreenToWorld)
	{
		// 以十字准星的世界位置作为射线起始点
		FVector Start = CrosshairWorldPosition;

		/*
		 * 当前的Start是世界关卡中的摄像机的位置，摄像机通常位于角色右后方偏上，如果从该点发射检测射线，那么该点到角色枪口垂直相交的点之前物体都会检测。
		 * 这会导致不合理的情况：
		 * 1.窗口准星瞄准角色自己的模型时，射线检测到角色自己(会导致开枪脑后自杀?0.0！)。
		 * 2.射线检测到角色右后方的区域的敌人。
		 * 因此需要修正射线发射起点，将其前移
		 */
		if(Character)
		{
			// 获取摄像机到角色的距离(记住摄像机是在角色右后方),注意获取的是Size长度
			float DistanceToCharacter = (Character->GetActorLocation() - Start).Size();
			// 方向是单位长度，需要乘上一个数。然后根据再偏大一点，比如100，该值自己测试确定适当值。
			Start += CrosshairWorldDirection * (DistanceToCharacter + 100.f);
			// 绘制Debug球体用于查看调试。bPersistentLines为false,因为本函数每帧执行，因此每帧都绘制，绘制的无需是持久维持的球体。
			// DrawDebugSphere(GetWorld(), Start, 12.f, 12, FColor::Red, false);
		}
		
		// 射线终点。上面被赋值的CrosshairWorldDirection实际只是一个单位向量，可以乘上一个数值运算。向量相加，得到期望的终点位置。
		FVector End = Start + CrosshairWorldDirection * TRACE_LENGTH;
		// 生成射线检测
		GetWorld()->LineTraceSingleByChannel(
		TraceHitResult,
		Start,
		End,
		// 碰撞检测通道
		ECollisionChannel::ECC_Visibility
		);

		// 射线碰撞检测结果处理。判断命中的目标有无实现了某个接口
		if(TraceHitResult.GetActor() && TraceHitResult.GetActor()->Implements<UInteractWithCrosshairsInterface>())
		{
			// 设置准星数据包里的颜色为红色
			HUDPackage.CrosshairsColor = FLinearColor::Red;
		}
		else
		{
			// 设置准星数据包里的颜色为白色
			HUDPackage.CrosshairsColor = FLinearColor::White;
		}

		
		// 如果没有碰撞
		if(!TraceHitResult.bBlockingHit)
		{
			// UE_LOG(LogTemp, Warning, TEXT("没有碰撞！"));
			// 直接设置碰撞结果为射线检测终点
			TraceHitResult.ImpactPoint = End;
			// 存储瞄准射线的命中位置。已经在TickComponent中处理，此处注释即可
			// HitTarget = End;
		}
		// 有碰撞，则绘制碰撞调试球
		else
		{
			// UE_LOG(LogTemp, Warning, TEXT("发生碰撞！"));
			// 存储瞄准射线的命中位置
			HitTarget = TraceHitResult.ImpactPoint;
			
			// // 绘制可视化球体用于调试
			// DrawDebugSphere(
			// 	// 在哪个世界绘制
			// 	GetWorld(),
			// 	// 绘制位置
			// 	TraceHitResult.ImpactPoint,
			// 	// 球体半径大小
			// 	12.f,
			// 	// 分几个片区
			// 	12,
			// 	// 颜色
			// 	FColor::Red
			// );
		}
		
	}
}

void UCombatComponent::OnRep_CarriedAmmo()
{
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	if(Controller)
	{
		// 注意，传的参数CarriedAmmo是玩家身上携带的后备弹药
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}

	/*
	 * 霰弹枪一发一发地装填过程中，后备弹药为0时，玩家动画需要跳转到霰弹枪转天结束段落。
	 * 满足的条件：
	 * 1.战斗状态为装填状态。(网络情况无法保证客户端的角色状态和服务器一致，因此需要判断);
	 * 2.战斗组件正常(不为空);
	 * 3.武器类型为霰弹枪;
	 * 4.后备弹药为0;
	 */
	bool bJumpToShotgunEnd = CombatState == ECombatState::ECS_Unoccupied &&
		EquippedWeapon != nullptr &&
			EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Shotgun &&
				CarriedAmmo == 0;
	if(bJumpToShotgunEnd)
	{
		// 玩家动画跳转
		JumpToShotgunEnd();
	}
}

void UCombatComponent::InitializeCarriedAmmo()
{
	/*
	 * 初始化每种武器类型对应的玩家初始携带的弹药数量。实际就是设置一对键值对。
	 */
	// 初始化临时的键值对(EWeaponType::EWT_Assaulrifle, 30)
	// CarriedAmmoMap.Emplace(EWeaponType::EWT_Assaulrifle, 30);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_Assaulrifle, StartingARAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_RocketLauncher, StartingRocketAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_Pistol, StartingPistolAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_SubmachineGun, StartingSubmachineAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_Shotgun, StartingShotgunAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_SniperRifle, StartingSniperAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_GrenadeLauncher, StartingGrenadeLauncherAmmo);
}



