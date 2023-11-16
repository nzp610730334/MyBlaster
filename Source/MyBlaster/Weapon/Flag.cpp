// Fill out your copyright notice in the Description page of Project Settings.


#include "Flag.h"

#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"

AFlag::AFlag()
{
	// 创建静态网格体
	FlagMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flag Mesh"));
	// 将其为到根组件
	SetRootComponent(FlagMesh);
	// 将碰撞检测球体附着到该静态网格体
	GetAreaSphere()->SetupAttachment(FlagMesh);
	// 将拾取提示组件附着到该静态网格体
	GetPickupWidget()->SetupAttachment(FlagMesh);
	//所有通道的碰撞反馈设置为忽略
	FlagMesh->SetCollisionResponseToChannels(ECollisionResponse::ECR_Ignore);
	// 关闭碰撞
	FlagMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AFlag::BeginPlay()
{
	Super::BeginPlay();

	// 存储记录旗子当前的变换
	InitialTransform = GetActorTransform();
}

void AFlag::Dropped()
{
	// 设置武器状态为掉落
	SetWeaponState(EWeaponState::EWS_Dropped);
	/*
	 * 解除武器对玩家角色的附着
	 */
	// 创建新的解除附着规则
	FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
	// 应用新的附着规则
	FlagMesh->DetachFromComponent(DetachRules);
	// 设置所有者为空指针
	SetOwner(nullptr);
	// 将武器存储的所有者的玩家角色设置为空指针
	BlasterOwnerCharacter = nullptr;
	// 将武器存储的所有者的玩家角色的控制器设置为空指针
	BlasterOwnerController = nullptr;
}

void AFlag::ResetFlag()
{
	// 获取旗子当前的所有者（执旗手）并转换为玩家角色
	ABlasterCharacter* FlagBearer = Cast<ABlasterCharacter>(GetOwner());
	if(FlagBearer)
	{
		// 重置玩家角色的举旗tag
		FlagBearer->SetHoldingTheFlag(false);
		// 手动重置玩家角色的OverlappedWeapon。因为本函数后续将直接重置本Actor的变换，因此结束重叠的事件函数不会执行，其中的SetOverlappedWeapon也不会执行，因此这里补上
		FlagBearer->SetOverlappedWeapon(nullptr);
		// 解除玩家角色的下蹲
		FlagBearer->UnCrouch();
	}

	// 本函数的后续行为只在服务器上执行。因此非服务器直接返回
	if(!HasAuthority()) return;
	/*
	 * 解除武器对玩家角色的附着
	 */
	// 创建新的解除附着规则
	FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
	// 应用新的附着规则
	FlagMesh->DetachFromComponent(DetachRules);
	// 将旗子(武器)的状态设置为初始化
	SetWeaponState(EWeaponState::EWS_Initial);
	// 重新开启碰撞检测球体的碰撞
	GetAreaSphere()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// 碰撞检测只检测Pawn，玩家角色通常属于Pawn。碰撞检测类型为重叠Overlap
	GetAreaSphere()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
	
	// 设置所有者为空指针
	SetOwner(nullptr);
	// 将武器存储的所有者的玩家角色设置为空指针
	BlasterOwnerCharacter = nullptr;
	// 将武器存储的所有者的玩家角色的控制器设置为空指针
	BlasterOwnerController = nullptr;
	
	// 设置旗子的变换为之前存储的初始变换
	SetActorTransform(InitialTransform);
}

void AFlag::OnEquipped()
{
	
	// 关闭武器拾取UI的显示
	ShowPickupWidget(false);
	// 关闭检测球体的碰撞检测
	GetAreaSphere()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 关闭网格体模拟物理
	FlagMesh->SetSimulatePhysics(false);
	// 关闭网格体重力应用
	FlagMesh->SetEnableGravity(false);
	// 网格体碰撞设置为仅查询
	FlagMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	// 设置一个单独碰撞通道WorldDynamic，并将碰撞反馈设置为重叠
	FlagMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Overlap);
	// 取消武器网格体边框高亮。
	EnableCustomDepth(false);
}

void AFlag::OnDropped()
{
	// 开启球体碰撞检测，注意：武器拾取只发生在服务器上，因此需要判断当前代码执行的机器是否具有权限（是否是服务器机器）
	if(HasAuthority())
	{
		GetAreaSphere()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	// 开启网格体模拟物理
	FlagMesh->SetSimulatePhysics(true);
	// 确保网格体应用了重力，前提是开启了模拟物理，否则不生效
	FlagMesh->SetEnableGravity(true);
	// 开启网格体碰撞
	FlagMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// 设置全通道碰撞为阻塞
	FlagMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	// 单独设置pawn碰撞忽略，这是为了让玩家角色能穿过武器
	FlagMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
	// 设置武器网格体忽略摄像机的碰撞
	FlagMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECR_Ignore);

	/*
	 * 设置武器网格体边框高亮。注意旗子网格体未被拾取是紫色，掉落后是蓝色。
	 */
	// 设置自定义深度的高亮颜色值
	FlagMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_BLUE);
	// 标记渲染状态为Dirty
	FlagMesh->MarkRenderStateDirty();
	// 启用自定义深度
	EnableCustomDepth(true);
	
	UE_LOG(LogTemp, Warning, TEXT("旗子的掉落逻辑执行完毕"));
}
