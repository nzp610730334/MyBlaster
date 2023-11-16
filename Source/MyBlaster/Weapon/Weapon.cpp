// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Kismet/KismetMathLibrary.h"
#include "MyBlaster/BlasterComponents/CombatComponent.h"
#include "MyBlaster/Character/BlasterCharacter.h"
#include "MyBlaster/PlayerController/BlasterPlayerController.h"
#include "Net/UnrealNetwork.h"

// Sets default values
AWeapon::AWeapon()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// 设置是否能复制到远程机器上,客户端能看到，到服务器才有权限
	// 没有设置bReplicates为true，则所有机器都拥有本对象的权限，且会在所有机器上生成，独立于服务器
	bReplicates = true;
	/*
	 * 还需要设置武器的运动组件为网络复制。否则服务器和客户端的武器运动不一致，会出现武器掉落到地面后，服务器和客户端上的武器的角度甚至位置不一样，
	 * 比如掉落后两者相差两三米，拾取判断的碰撞盒子只有1米，导致客户端明明靠近武器却无法拾取，因为以服务器为准的权威版本上，客户端的玩家并没有靠近武器。
	 * 注意区分两者的区别
	 * SetReplicatedMovement();
	 * SetReplicateMovement();
	 */
	SetReplicateMovement(true);
	
	// 设置骨架网格体
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	// 设置附着
	WeaponMesh->SetupAttachment(RootComponent);
	// 设置根组件
	SetRootComponent(WeaponMesh);
	
	// 设置全通道碰撞为阻塞
	WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	// 单独设置pawn碰撞忽略，这是为了让玩家角色能穿过武器
	WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
	// 构造时，先不启用碰撞设置
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	/*  设置网格体自定义深度的值。使得武器网格图边框高亮
	 * Sets the CustomDepth stencil value (0 - 255) and marks the render state dirty.
	 * 设置“自定义深度”模具值（0-255）并将渲染状态标记为“dirty”。
	 */ 
	WeaponMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_PURPLE);
	// 将渲染状态标记为Dirty-将在帧结束时发送到渲染线程。
	WeaponMesh->MarkRenderStateDirty();
	// 启用自定义深度
	EnableCustomDepth(true);

	// 对于多人游戏而言，碰撞检测逻辑处理建议放在服务器上，因此这里构造时关闭相关设置，在服务器上执行相关逻辑时再打开
	// 创建球体
	AreaSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AreaSphere"));
	// 设置附着
	AreaSphere->SetupAttachment(RootComponent);
	// 全通道碰撞检测设置为忽略
	AreaSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	// 先不启用碰撞
	AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 武器拾取图标
	PickupWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PickupWidget"));
	PickupWidget->SetupAttachment(RootComponent);
	
}


void AWeapon::EnableCustomDepth(bool bEnable)
{
	// 判断武器网格体是否已被设置
	if(WeaponMesh)
	{
		// 启用网格体的自定义深度
		WeaponMesh->SetRenderCustomDepth(bEnable);
	}
}

void AWeapon::OnPingTooHigh(bool bPingTooHigh)
{
	// 如果处于高Ping情况下，则不使用服务器倒带
	bUseServerSideRewind = !bPingTooHigh;
	// if(bUseServerSideRewind == true)
	// {
	// 	UE_LOG(LogTemp, Warning, TEXT("开启了bUseServerSideRewind！！！"));
	//
	// }
	// else if(bUseServerSideRewind == false)
	// {
	// 	UE_LOG(LogTemp, Warning, TEXT("关闭了bUseServerSideRewind------"));
	// }
}

// Called when the game starts or when spawned
void AWeapon::BeginPlay()
{
	Super::BeginPlay();

	// 验证本Actor所在的机器是否为ROLE_Authority，一般服务器机器上才会是ROLE_Authority
	// if(GetLocalRole() == ENetRole::ROLE_Authority)		// 本行代码等价于  	if(HasAuthority())
	
	/*
	 * 为了提升高延迟玩家的游戏体验，将武器碰撞重叠事件的委托的绑定改为在所有机器上执行，而非之前仅在服务器上执行，因此不再要求HasAuthority()
	 */ 
	// 验证通过，开启物理碰撞
	AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// 碰撞检测只检测Pawn，玩家角色通常属于Pawn。碰撞检测类型为重叠Overlap
	AreaSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
	// ？多播委托
	AreaSphere->OnComponentBeginOverlap.AddDynamic(this, &AWeapon::OnSphereOverlap);
	AreaSphere->OnComponentEndOverlap.AddDynamic(this, &AWeapon::OnSphereEndOverlap);

	// 确保拾取组件的存在，游戏开始时先设置为不可视
	if(PickupWidget)
	{
		PickupWidget->SetVisibility(false);
	}

	// 模拟客户端作弊，修改了开火间隔
	// if(!HasAuthority())
	// {
	// 	FireDelay = 0.001f;
	// }
}

// Called every frame
void AWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AWeapon::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                              UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// UE_LOG(LogTemp,Log,TEXT("开始碰撞重叠检测！！！"));
	ABlasterCharacter* BlasterActor = Cast<ABlasterCharacter>(OtherActor);
	// if(BlasterActor && PickupWidget)
	if(BlasterActor)
	{
		// 当前武器是旗子时，且武器所属队伍(如果有)与当前重叠的玩家的队伍一致时，直接返回（业务需求：玩家不能拿起(夺取)本队伍的旗子，只能夺取对面的旗子放自家得分区）
		if(WeaponType == EWeaponType::EWT_Flag && BlasterActor->GetTeam() == Team) return;
		// 玩家角色处于举着旗子的状态时，不允许拾取武器，直接返回
		if(BlasterActor->IsHoldingTheFlag()) return;
		// UE_LOG(LogTemp,Log,TEXT("触发了碰撞重叠！！！"));
		// PickupWidget->SetVisibility(true);
		BlasterActor->SetOverlappedWeapon(this);
	}
}

// 角色离开武器球体检测范围
void AWeapon::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ABlasterCharacter* BlasterActor = Cast<ABlasterCharacter>(OtherActor);
	if(BlasterActor)
	{
		// 当前武器是旗子时，且武器所属队伍(如果有)与当前重叠的玩家的队伍一致时，直接返回
		if(WeaponType == EWeaponType::EWT_Flag && BlasterActor->GetTeam() == Team) return;
		// 玩家角色处于举着旗子的状态时，不允许拾取武器，直接返回
		if(BlasterActor->IsHoldingTheFlag()) return;
		// 传入空指针，将拾取显示组件隐藏
		BlasterActor->SetOverlappedWeapon(nullptr);
	}
}

void AWeapon::SetHUDAmmo()
{
	// 获取武器所有者并转换为玩家角色
	BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : BlasterOwnerCharacter;
	// 转换成功
	if(BlasterOwnerCharacter)
	{
		// 获取武器所有者的控制器，并转换成玩家控制器
		BlasterOwnerController = BlasterOwnerController == nullptr ? Cast<ABlasterPlayerController>(BlasterOwnerCharacter->Controller) : BlasterOwnerController;
		// 转换成功
		if(BlasterOwnerController)
		{
			// 设置UI信息
			BlasterOwnerController->SetHUDWeaponAmmo(Ammo);
		}
	}
}

void AWeapon::SpendRound()
{
	/*
	 * Fire开火以及子弹扣除函数(本函数)都是在服务器上进行的行为，客户端的UI弹匣子弹数量的刷新是收到网络复制更新后执行回调函数OnRep完成的。
	 * 而服务器是发出网络复制更新的一方，服务器的机器上不会执行OnRep函数，因此UI的刷新需要在此处处理。
	 */
	// 扣除子弹数量
	// --Ammo;
	// 需要确保扣除子弹数后，剩余数量符合逻辑，即大于等于0小于等于弹匣最大数量
	Ammo = FMath::Clamp(Ammo - 1, 0, MagCapacity);
	SetHUDAmmo();

	/*
	 * 当前机器为服务器，则需要通知所有客户端更新弹药。
	 */ 
	if(HasAuthority())
	{
		ClientUpdateAmmo(Ammo);
	}
	// 当前机器为客户端，则需要累记之前开火RPC的发送次数。这是在开火一次消耗一发弹药的情况下，Sequence增加的值等于一次开火消耗的弹药
	else
	{
		++Sequence;
	}
}

void AWeapon::ClientUpdateAmmo_Implementation(int32 ServerAmmo)
{
	// 本函数只在客户端上执行，非客户端直接return。 疑惑：需要手动写if代码指定吗？Client的函数服务器也会执行吗？
	if(HasAuthority()) return;
	// 本函数是在客户端上执行的。客户端先接受服务器的修正。
	Ammo = ServerAmmo;
	// 扣除一次Sequence的值，表示之前发出的那么多次开火RPC请求，服务器接收并回复了一次。剩余的Sequence表示服务器尚未回复的次数。
	--Sequence;
	// 但客户端还在本地消耗了等值于Sequence的弹药数，修正后的Ammo不符合本地最新情况，因此需要补正。
	Ammo -= Sequence;
	// 刷新弹药HUD
	SetHUDAmmo();
}

void AWeapon::AddAmmo(int32 AmmoToAdd)
{
	/*
	 * 疑惑：为什么要使用Ammo - AmmoToAdd，这就要求传进来的AmmoToAdd是负值。传一个正值使用Ammo + AmmoToAdd不行吗？
	 * 教程P167的07:24，已修改为+法计算
	 */
	// 确保Ammo在符合逻辑的范围内。
	Ammo = FMath::Clamp(Ammo + AmmoToAdd, 0, MagCapacity);
	// 客户端的Weapon的Ammo因为网络更新而调用回调函数OnRep_Ammo，OnRep_Ammo中更新了HUD，但服务器的HUD并没有更新，所以此处需要处理。
	SetHUDAmmo();
	// 通知客户端添加弹药
	ClientAddAmmo(AmmoToAdd);
}

void AWeapon::ClientAddAmmo_Implementation(int32 AmmoToAdd)
{
	// 本函数只在客户端上执行，非客户端直接return。 疑惑：需要手动写if代码指定吗？Client的函数服务器也会执行吗？
	if(HasAuthority()) return;
	// 根据服务器的要求，添加弹药，但确保添加后的Ammo在符合逻辑的范围内。
	Ammo = FMath::Clamp(Ammo + AmmoToAdd, 0, MagCapacity);
	/*
	 * 一般枪械是整个弹匣装填，装填动画的动画通知执行完毕则装填完毕。
	 * 霰弹枪是一发一发装填，RPC也是一个个地发给换弹玩家的客户端。霰弹枪允许装填中开火，打断装填，
	 * 因此霰弹枪每次添加完子弹后(单发子弹装填动画的动画通知执行完毕后)，都需要检查弹匣是否已满，已满则跳转霰弹枪装填完毕的动画。
	 */
	// 获取并转换玩家角色
	BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : BlasterOwnerCharacter;
	// 获取并转换成功
	if(BlasterOwnerCharacter)
	{
		// 战斗组件调用函数将玩家动画实例跳转到霰弹枪装填完毕的阶段
		BlasterOwnerCharacter->GetCombat()->JumpToShotgunEnd();
	}
	// 刷新弹药HUD
	SetHUDAmmo();
}

void AWeapon::OnRep_Ammo()
{
	/*
	 * 本函数作为回调函数，在客户端的Ammo属性被服务器网络复制更新时在客户端本机机器上调用，处理刷新玩家UI信息
	 */
	BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : BlasterOwnerCharacter;
	// 角色获取成功、战斗组件存在且武器弹匣已满
	if(BlasterOwnerCharacter && BlasterOwnerCharacter->GetCombat() && IsFull())
	{
		// 玩家角色动画跳转到装填结束的段落
		BlasterOwnerCharacter->GetCombat()->JumpToShotgunEnd();
	}
	SetHUDAmmo();
}

void AWeapon::OnRep_Owner()
{
	Super::OnRep_Owner();

	// 武器所有者发生网络复制导致的更新有两种情况，1.从有值到空指针(现在是空指针了)；2.从空指针到有值(现在是有值了)。
	if(Owner == nullptr)
	{
		// 现在是空指针了，则需要把原来所有者的玩家角色和控制器重置为空指针；
		BlasterOwnerCharacter = nullptr;
		BlasterOwnerController = nullptr;
	}
	else
	{
		// 获取本对象的所有者并转换为玩家角色类
		BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(Owner) : BlasterOwnerCharacter;
		// 获取成功，且本对象的所有者当前装备的武器就是本对象时，设置弹药HUD
		if(BlasterOwnerCharacter && BlasterOwnerCharacter->GetEquippedWeapon() == this)
		{
			// 设置本对象的弹药HUD
			SetHUDAmmo();
		}
	}
}

void AWeapon::OnRep_MagCapacity()
{
}

void AWeapon::SetWeaponState(EWeaponState State)
{
	/*
	 * 本函数在服务器上执行，涉及武器状态改变后需要执行的一系列行为，
	 * 客户端不执行本函数，因此涉及武器状态改变的一系列行为的执行需要在客户端的武器状态改变的回调函数中执行。
	 */
	// 在本函数内处理武器状态改变的相关逻辑
	WeaponState = State;
	// 根据武器变更的状态执行对应逻辑
	OnWeaponState();
}

void AWeapon::OnWeaponState()
{
	switch(WeaponState)
	{
	case EWeaponState::EWS_Equipped:
		// 执行武器被装备的相关的逻辑
		OnEquipped();
		break;

	case EWeaponState::EWS_EquippedSecondary:
		// 执行武器被放置到后背背包时的相关逻辑
		OnEquippedSecondary();
		break;
		
	case EWeaponState::EWS_Dropped:
		// 服务器上的武器的所有者被设置为空指针，但客户端这里的还没有，因此这里需要设置为空指针。Owner是一个默认网络复制更新的属性，因此客户端实际不必自己设置
		// SetOwner(nullptr);
		// 执行武器掉落相关的逻辑
		UE_LOG(LogTemp, Warning, TEXT("旗子的掉落逻辑执行中"));
		OnDropped();
		break;
	}
}

void AWeapon::OnRep_WeaponState()
{
	/*
	 * 客户端的角色的武器状态被改变时，本函数会被自动执行
	 */
	// 根据武器变更的状态执行对应逻辑
	OnWeaponState();
}

void AWeapon::OnEquipped()
{
	// 关闭武器拾取UI的显示
	ShowPickupWidget(false);
	// 关闭检测球体的碰撞检测
	AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 关闭网格体模拟物理
	WeaponMesh->SetSimulatePhysics(false);
	// 关闭网格体重力应用
	WeaponMesh->SetEnableGravity(false);
	// 关闭网格体碰撞
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	// 武器为冲锋枪时，需要的额外设置。比如使得冲锋枪的枪带自然下垂(如果物理资产支持的话)
	if(WeaponType == EWeaponType::EWT_SubmachineGun)
	{
		// 启用什么类型的碰撞。（ QueryAndPhysics，可用于空间查询（光线投射、扫掠、重叠）和模拟（刚体、约束）。）
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		// 开启重力应用
		WeaponMesh->SetEnableGravity(true);
		// 关闭网格体碰撞
		WeaponMesh->SetCollisionResponseToChannels(ECollisionResponse::ECR_Ignore);
	}
	// 取消武器网格体边框高亮。
	EnableCustomDepth(false);

	/*
	 * 绑定该武器的一些行为逻辑作为委托到玩家角色的控制器上。
	 * 比如：根据控制器上的变量(是否高延迟)，修改本武器的变量(是否启用服务器倒带)
	 * 注意：既然装备该武器需要绑定，那么相应的，该武器掉落时也需要解除绑定，注意这些情况下执行的判断条件不同
	 */
	// 先获取并转换玩家角色
	BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : BlasterOwnerCharacter;
	if(!BlasterOwnerCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("BlasterOwnerCharacter转换获取失败！！！"));
		if(GetOwner())
		{
			UE_LOG(LogTemp, Warning, TEXT("可以正常获取GetOwner！！！"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("获取GetOwner失败！！！"));
		}
	}
	// 获取成功。且本武器开启了服务器倒带时才需要绑定，以响应控制器高Ping的变化
	if(BlasterOwnerCharacter && bUseServerSideRewind)
	{
		// 通过玩家角色获取玩家的控制器并转换
		BlasterOwnerController = BlasterOwnerController == nullptr ? Cast<ABlasterPlayerController>(BlasterOwnerCharacter->Controller) : BlasterOwnerController;
		/* 条件判断：
         * 1.BlasterOwnerController获取并转换成功。
         * 2.当前机器为服务器，因为客户端无需处理该委托的绑定，Weapon的变量改变由服务器进行网络更新进行同步。
         * 3.控制器的该委托未绑定任何委托
         */ 
		if(BlasterOwnerController && HasAuthority() && !BlasterOwnerController->HighPingDelegate.IsBound())
		{
			/*
             * 绑定动态多播。注意绑定的函数必须要被UFUNCTION()标记
             * 疑惑：为什么是动态多播？
             */
			BlasterOwnerController->HighPingDelegate.AddDynamic(this, &AWeapon::OnPingTooHigh);
			UE_LOG(LogTemp, Warning, TEXT("已绑定HighPingDelegate"));
		}
	}
	else
	{
		if(HasAuthority())
		{
			UE_LOG(LogTemp, Warning, TEXT("BlasterOwnerCharacter和bUseServerSideRewind不符合条件！这是服务器的打印警告！！！"));
		}
		else
		{
			/*
			 * 因为所有代码都会在服务器和每个客户端机器上执行，因此非服务器上BlasterOwnerCharacter和bUseServerSideRewind不符合条件时，
			 * 通常是非服务器(客户端)上的本武器的武器状态没有瞬间同步导致没有本的Owner为空,从而导致上面的BlasterOwnerCharacter转换失败，就会走到这里打印日志
			 */ 
			UE_LOG(LogTemp, Warning, TEXT("BlasterOwnerCharacter和bUseServerSideRewind不符合条件！这是非服务器的打印警告~~~"));
		}
		if(!bUseServerSideRewind)
		{
			UE_LOG(LogTemp, Warning, TEXT("bUseServerSideRewind不为真！！"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("bUseServerSideRewind为真！！"));
		}
	}
}

void AWeapon::OnDropped()
{
	// 开启球体碰撞检测，注意：武器拾取只发生在服务器上，因此需要判断当前代码执行的机器是否具有权限（是否是服务器机器）
	if(HasAuthority())
	{
		AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	// 开启网格体模拟物理
	WeaponMesh->SetSimulatePhysics(true);
	// 确保网格体应用了重力，前提是开启了模拟物理，否则不生效
	WeaponMesh->SetEnableGravity(true);
	// 开启网格体碰撞
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// 设置全通道碰撞为阻塞
	WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	// 单独设置pawn碰撞忽略，这是为了让玩家角色能穿过武器
	WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
	// 设置武器网格体忽略摄像机的碰撞
	WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECR_Ignore);

	/*
	 * 设置武器网格体边框高亮。注意武器网格体未被拾取是紫色，掉落后是蓝色。
	 */
	// 设置自定义深度的高亮颜色值
	WeaponMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_BLUE);
	// 标记渲染状态为Dirty
	WeaponMesh->MarkRenderStateDirty();
	// 启用自定义深度
	EnableCustomDepth(true);

	/*
	 * 解除绑定：该武器的一些行为逻辑之前作为委托到玩家角色的控制器上，现在武器掉落，需要解除绑定
	 */
	// 先获取并转换玩家角色
	BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : BlasterOwnerCharacter;
	// 获取成功。且本武器开启了服务器倒带时才需要绑定，以响应控制器高Ping的变化
	if(BlasterOwnerCharacter && bUseServerSideRewind)
	{
		// 通过玩家角色获取玩家的控制器并转换
		BlasterOwnerController = BlasterOwnerController == nullptr ? Cast<ABlasterPlayerController>(BlasterOwnerCharacter->GetController()) : BlasterOwnerController;
		/* 条件判断：
		 * 1.BlasterOwnerController获取并转换成功。
		 * 2.当前机器为服务器，因为客户端无需处理该委托的绑定，Weapon的变量改变由服务器进行网络更新进行同步。
		 * 3.控制器的该委托已绑定委托
		 */ 
		if(BlasterOwnerController && HasAuthority() && BlasterOwnerController->HighPingDelegate.IsBound())
		{
			/*
			 * 绑定动态多播。注意绑定的函数必须要被UFUNCTION()标记
			 * 疑惑：为什么是动态多播？
			 */
			BlasterOwnerController->HighPingDelegate.RemoveDynamic(this, &AWeapon::OnPingTooHigh);
			UE_LOG(LogTemp, Warning, TEXT("已解除绑定HighPingDelegate"));
		}
	}
}

void AWeapon::OnEquippedSecondary()
{
	// 关闭武器拾取UI的显示
	ShowPickupWidget(false);
	// 关闭检测球体的碰撞检测
	AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 关闭网格体模拟物理
	WeaponMesh->SetSimulatePhysics(false);
	// 关闭网格体重力应用
	WeaponMesh->SetEnableGravity(false);
	// 关闭网格体碰撞
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	// 武器为冲锋枪时，需要的额外设置。比如使得冲锋枪的枪带自然下垂(如果物理资产支持的话)
	if(WeaponType == EWeaponType::EWT_SubmachineGun)
	{
		// 启用什么类型的碰撞。（ QueryAndPhysics，可用于空间查询（光线投射、扫掠、重叠）和模拟（刚体、约束）。）
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		// 开启重力应用
		WeaponMesh->SetEnableGravity(true);
		// 关闭网格体碰撞
		WeaponMesh->SetCollisionResponseToChannels(ECollisionResponse::ECR_Ignore);
	}

	// 开启武器网格体边框高亮。（P191后，不再在此处开启，而是武器切换动画即将播放完毕后才开启。）
	// EnableCustomDepth(true);
	
	// 确保副武器的网格体存在
	if(WeaponMesh)
	{
		// 将副武器的网格体的自定义深度值设置为棕褐色
		WeaponMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_TAN);
		// 标记副武器的网格体的渲染状态为Dirty
		WeaponMesh->MarkRenderStateDirty();
	}
}

void AWeapon::ShowPickupWidget(bool bShowWidget)
{
	if(PickupWidget)
	{
		PickupWidget->SetVisibility(bShowWidget);
	}
}

void AWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// 注册要复制的属性
	DOREPLIFETIME(AWeapon, WeaponState);
	// 优化为使用客户端预测，不再通过网络复制服务器和客户端之间Ammo的同步
	// DOREPLIFETIME(AWeapon, Ammo);
	// 有条件的复制(此属性将只发送给参与者的所有者)
	DOREPLIFETIME_CONDITION(AWeapon, bUseServerSideRewind, COND_OwnerOnly);
}

void AWeapon::Fire(const FVector& HitTarget)
{
	// 校验开火动画资源的存在
	if(FireAnimation)
	{
		// 要播放的资产，以及是否循环播放
		WeaponMesh->PlayAnimation(FireAnimation, false);
	}
	// 生成弹壳
	if(CasingClass)
	{
		// 获取弹壳抛出口的插槽
		const USkeletalMeshSocket* AmmoEjectSocket = WeaponMesh->GetSocketByName(FName("AmmoEject"));
		// 获取武器的弹壳抛出口的插槽的转换
		FTransform SocketTransform = AmmoEjectSocket->GetSocketTransform(WeaponMesh);
		// 获取世界关卡
		UWorld* World = GetWorld();
		// 获取的结果有效
		if(World)
		{
			World->SpawnActor<ACasing>(
				CasingClass,
				SocketTransform.GetLocation(),
				SocketTransform.GetRotation().Rotator()
			);
		}
	}
	// 仅在服务器上才扣除子弹
	// if(HasAuthority())
	// {
	// 	SpendRound();
	// }
	// 优化为使用客户端预测，因此在所有机器上都调用
	SpendRound();
}


void AWeapon::Dropped()
{
	// 设置武器状态为掉落
	SetWeaponState(EWeaponState::EWS_Dropped);
	/*
	 * 解除武器对玩家角色的附着
	 */
	// 创建新的解除附着规则
	FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
	// 应用新的附着规则
	WeaponMesh->DetachFromComponent(DetachRules);
	// 设置所有者为空指针
	SetOwner(nullptr);
	// 将武器存储的所有者的玩家角色设置为空指针
	BlasterOwnerCharacter = nullptr;
	// 将武器存储的所有者的玩家角色的控制器设置为空指针
	BlasterOwnerController = nullptr;
}

bool AWeapon::IsEmpty()
{
	// 弹匣数量少于等于0，则返回空
	return Ammo <= 0;
}

bool AWeapon::IsFull()
{
	// 弹药数量等于弹匣最大值，则返回true
	return Ammo == MagCapacity;
}

FVector AWeapon::TraceEndWithScatter(const FVector& HitTarget)
{
	/*
	 * 在枪口前方的一定距离生成一个球体，随机获取该球体内的一个点，从枪口到该点的方向即为本次单个弹丸的射击方向。
	 * 一个变量，如果在它的生命周期内它的值不会被修改，那么尽量使用const将其修饰
	 */
	// 获取射线武器的网格体，通过网格体获取插槽
	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
	// 插槽获取失败(为nullptr)，直接返回空的FVector
	if(MuzzleFlashSocket == nullptr) return FVector();
	// 获取插槽的转换Tranform。需要传入插槽来自的骨骼网格组件(指的就是武器的网格体)
	const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
	// 枪口插槽的世界转换作为射线射出的起始点
	const FVector TraceStart = SocketTransform.GetLocation();
	
	// 计算枪口到瞄准目标的单位方向
	const FVector ToTargetNormalized = (HitTarget - TraceStart).GetSafeNormal();
	// 计算世界关卡的散布球体的球心位置 = 枪口起始点 + 单位方向 * 预设置的球体半径
	const FVector SphereCenter = TraceStart + ToTargetNormalized * DistanceToSphere;
	// 散布球体内随机一个点位置 = 随机方向的单位向量 * 随机半径。			备注： 注意区分FRandRange和RandRange
	const FVector RanLoc = UKismetMathLibrary::RandomUnitVector() * FMath::FRandRange(0.f, SphereRadius);
	// 期望的世界关卡中的散布球体的随机点 = 世界关卡中的散布球体的球心 + 散布球体内的随机点
	const FVector EndLoc = SphereCenter + RanLoc;
	// 枪口到散布球体的随机落点的向量 = 随机落点位置 - 枪口位置
	const FVector ToEndLoc = EndLoc - TraceStart;
	
	/* 绘制球体用于游戏运行时查看
	// 绘制散布球体
	DrawDebugSphere(GetWorld(), SphereCenter, SphereRadius, 12, FColor::Red, true);
	// 绘制散步球体中随机到的落点
	DrawDebugSphere(GetWorld(), EndLoc, 4.f, 12, FColor::Orange,true);
	// 绘制射击弹道
	DrawDebugLine(
		GetWorld(),
		TraceStart,
		FVector(TraceStart + ToEndLoc * TRACE_LENGTH / ToEndLoc.Size()),
		FColor::Cyan,
		true
	);
	*/

	// 射击的子弹的最终落点 = 枪口位置 + 枪口到散布球体的随机落点的向量 * 射线检测的长度 / 枪口到散布球体的随机落点的向量的大小			疑惑：为什么不直接在前面就将ToEndLoc标准化为单位向量
	return FVector(TraceStart + ToEndLoc * TRACE_LENGTH / ToEndLoc.Size());
}