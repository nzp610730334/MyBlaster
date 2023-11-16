// Fill out your copyright notice in the Description page of Project Settings.


#include "BuffComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "MyBlaster/Character/BlasterCharacter.h"

// Sets default values for this component's properties
UBuffComponent::UBuffComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UBuffComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

void UBuffComponent::Heal(float HealAmount, float HealingTime)
{
	// 将血量状态设置为痊愈中
	bHealing = true;
	// 计算痊愈速率,痊愈速率 = 本次将要痊愈的血量 / 痊愈时长
	HealingRate = HealAmount / HealingTime;
	// 将本次的血量累加进要恢复的血量中
	AmountToHeal += HealAmount; 
}

void UBuffComponent::ReplenishShield(float ShieldAmount, float ReplenishTime)
{
	// 将护盾状态设置为补充中
	bReplenishingShield = true;
	// 计算护盾的补充速率
	ShieldReplenishRate = ShieldAmount / ReplenishTime;
	// 将本次的护盾值累加进要补充的护盾值中
	ShieldReplenishAmount += ShieldAmount;
}

void UBuffComponent::BuffSpeed(float BuffBaseSpeed, float BuffCrouchSpeed, float BuffTime)
{
	// 玩家角色如果为空，则直接返回
	if(Character == nullptr) return;
	// 创建计时器
	// GetWorld()->GetTimerManager().SetTimer(
	// 	SpeedBuffTimer,
	// 	this,
	// 	&UBuffComponent::ResetSpeeds,
	// 	SpeedBuffTime
	// 	);
	// 疑惑： GetWorld()和Character设置的计时器有什么不同?
	Character->GetWorldTimerManager().SetTimer(
		SpeedBuffTimer,
		this,
		&UBuffComponent::ResetSpeeds,
		BuffTime
		);

	// 确保玩家角色的组件存在，因为可能没有被正确初始化
	if(Character->GetCharacterMovement())
	{
		// 变更玩家角色的移速
		Character->GetCharacterMovement()->MaxWalkSpeed = BuffBaseSpeed;
		Character->GetCharacterMovement()->MaxWalkSpeedCrouched = BuffCrouchSpeed;
	}
	/*
	 * 本函数只是在服务器上执行，因此还需要通知所有客户端变更该玩家的移速。
	 * 疑惑：角色移速组件竟然没有网络复制？
	 */ 
	MulticastSpeedBuff(BuffBaseSpeed, BuffCrouchSpeed);
}

void UBuffComponent::SetInitialSpeeds(float BaseSpeed, float CrouchSpeed)
{
	InitialBaseSpeed = BaseSpeed;
	InitialCrouchSpeed = CrouchSpeed;
}

void UBuffComponent::ResetSpeeds()
{
	// 确保玩家角色以及玩家角色的移动组件存在，否则直接返回
	if(Character == nullptr || Character->GetCharacterMovement() == nullptr) return;

	Character->GetCharacterMovement()->MaxWalkSpeed = InitialBaseSpeed;
	Character->GetCharacterMovement()->MaxWalkSpeedCrouched = InitialCrouchSpeed;
	/*
	 * 本函数只是在服务器上执行，因此还需要通知所有客户端变更该玩家的移速。
	 * 疑惑：角色移速组件竟然没有网络复制？
	 */ 
	MulticastSpeedBuff(InitialBaseSpeed, InitialCrouchSpeed);
}

void UBuffComponent::BuffJump(float BuffJumpVelocity, float BuffTime)
{
	// 确定玩家角色的存在，否则直接返回
	if(Character == nullptr) return;

	// 设置buff过期的计时器，定时执行ResetXX函数重置玩家角色获得BUff之前的z轴跳跃速率
	Character->GetWorldTimerManager().SetTimer(
		JumpBuffTimer,
		this,
		&UBuffComponent::ResetJump,
		BuffTime
	);
	
	// 确定玩家角色移动组件的存在
	if(Character->GetCharacterMovement())
	{
		// 变更玩家角色的z轴跳跃速率
		Character->GetCharacterMovement()->JumpZVelocity = BuffJumpVelocity;

		// UE_LOG(LogTemp, Warning, TEXT("当前玩家的跳跃速率：%f"), Character->GetCharacterMovement()->JumpZVelocity);
	}
	// 调用多播，变更所有客户端上的该玩家的z轴跳跃速率
	MulticastJumpBuff(BuffJumpVelocity);
}

void UBuffComponent::SetInitialJumpVelocity(float Velocity)
{
	InitialJumpVelocity = Velocity;
}

void UBuffComponent::ResetJump()
{
	// 确定玩家角色以及玩家角色的移动组件存在，否则直接返回
	if(Character == nullptr || Character->GetCharacterMovement() == nullptr) return;

	// 变更玩家角色的z轴跳跃速率
	Character->GetCharacterMovement()->JumpZVelocity = InitialJumpVelocity;
	// 调用多播，变更所有客户端上的该玩家的z轴跳跃速率
	MulticastJumpBuff(InitialJumpVelocity);
}

void UBuffComponent::MulticastJumpBuff_Implementation(float JumpVelocity)
{
	// 确定玩家角色以及移动组件的存在
	if(Character && Character->GetCharacterMovement())
	{
		// 变更玩家角色的z轴跳跃速率
		Character->GetCharacterMovement()->JumpZVelocity = JumpVelocity;
	}
}

void UBuffComponent::MulticastSpeedBuff_Implementation(float BaseSpeedBuff, float CrouchSpeed)
{
	// 玩家角色和移动组件不存在时，直接返回
	// if(Character == nullptr || Character->GetCharacterMovement() == nullptr) return;
	if(Character && Character->GetCharacterMovement())
	{
		// 变更该玩家的基本移速
        Character->GetCharacterMovement()->MaxWalkSpeed = BaseSpeedBuff;
        // 变更该玩家的蹲伏状态的移速
        Character->GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;
	}
}

void UBuffComponent::HealRampUp(float DeltaTime)
{
	// 角色已经处于痊愈中或者被淘汰时，直接返回
	if(!bHealing || Character == nullptr || Character->IsElimmed()) return;
	/*
	 * 计算本次方法执行，需要恢复的血量。计算出来就不应该再被修改，因此使用const修饰
	 * 本次痊愈血量 = 痊愈速率 * 本次执行距离上次执行的时间差；
	 * 疑惑：DeltaTime是什么？是本次访问DeltaTime距离上次访问DeltaTime之间的时间差？
	 */
	const float HealThisFrame = HealingRate * DeltaTime;
	/*
	 * 设置玩家角色的血量。
	 * 本次痊愈后玩家的血量要符合业务逻辑，使用Clamp限制
	 */ 
	Character->SetHealth(FMath::Clamp(Character->GetHealth() + HealThisFrame, 0.f, Character->GetMaxHealth()));
	// 更新玩家血量HUD。注意：此处只是服务器上的行为，客户端的血量HUD更新在OnRep函数中
	Character->UpdateHUDHealth();
	// 从要痊愈的总血量中扣除本次痊愈血量
	AmountToHeal -= HealThisFrame;
	// 要痊愈的血量已经全部痊愈或者玩家血量已满，则取消玩家的痊愈中状态,且将要痊愈的血量归0
	if(AmountToHeal <= 0.f || Character->GetHealth() >= Character->GetMaxHealth())
	{
		// 对应于要痊愈的血量已经全部痊愈或者玩家血量已满
		bHealing = false;
		// 还剩余要痊愈的血量但玩家血量已满的情况，同样也将要痊愈的血量归零。
		AmountToHeal = 0.f;
	}
}

void UBuffComponent::ShieldRampUp(float DeltaTime)
{
	// 角色已经处于护盾补充中或者被淘汰时，直接返回
	if(!bReplenishingShield || Character == nullptr || Character->IsElimmed()) return;
	/*
	 * 计算本次方法执行，需要补充的护盾值。计算出来就不应该再被修改，因此使用const修饰
	 * 本次补充的护盾值 = 补充速率 * 本次执行距离上次执行的时间差；
	 * 疑惑：DeltaTime是什么？是本次访问DeltaTime距离上次访问DeltaTime之间的时间差？
	 */
	const float ReplenishThisFrame = ShieldReplenishRate * DeltaTime;
	/*
	 * 设置玩家角色的护盾值。
	 * 本次补充后玩家的护盾值要符合业务逻辑，使用Clamp限制
	 */ 
	Character->SetShield(FMath::Clamp(Character->GetShield() + ReplenishThisFrame, 0.f, Character->GetMaxShield()));
	// 更新玩家护盾HUD。注意：此处只是服务器上的行为，客户端的护盾HUD更新在OnRep函数中
	Character->UpdateHUDShield();
	// 从要补充的总的护盾值中扣除本次补充的护盾值
	ShieldReplenishAmount -= ReplenishThisFrame;
	// 要补充的护盾值已经全部补充完毕或者玩家护盾值已满，则取消玩家的护盾补充中的状态,且将要补充的总护盾值归0
	if(ShieldReplenishAmount <= 0.f || Character->GetShield() >= Character->GetMaxShield())
	{
		// 对应于要补充的护盾值已经全部补充完毕或者玩家护盾值已满
		bReplenishingShield = false;
		// 如果要补充的总的护盾值还有剩余，但玩家护盾值已满时，同样也将要补充的总的护盾值归零。
		ShieldReplenishAmount = 0.f;
	}
}

// Called every frame
void UBuffComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
	// 血量恢复
	HealRampUp(DeltaTime);
	// 护盾值恢复
	ShieldRampUp(DeltaTime);
}

