// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BuffComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MYBLASTER_API UBuffComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UBuffComponent();
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	// 设置玩家角色类为本类的友元，使其可以直接访问本类的所有属性
	friend class ABlasterCharacter;
	// 被对外被调用的接口，治愈玩家角色，比如玩家拾取道具后调用Heal()，传入恢复的血量以及时长，开启血量恢复的过程。
	void Heal(float HealAmount, float HealingTime);
	// 被对外被调用的接口，补充玩家角色的护盾值，比如玩家拾取道具后调用Heal()，传入恢复的血量以及时长，开启血量恢复的过程。
	void ReplenishShield(float ShieldAmount, float ReplenishTime);
	// 被对外被调用的接口，提升玩家角色的速度，比如玩家拾取加速道具之后执行SpeedBufff提速
	void BuffSpeed(float BuffBaseSpeed, float BuffCrouchSpeed, float BuffTime);
	// 记录玩家角色的基础移速和蹲伏移速
	void SetInitialSpeeds(float BaseSpeed, float CrouchSpeed);
	// 记录玩家角色的初始跳跃速率
	void SetInitialJumpVelocity(float Velocity);
	// 提升玩家角色的z轴跳跃速率
	void BuffJump(float BuffJumpVelocity, float BuffTime);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	// 本函数被每帧调用，随时间恢复血量
	void HealRampUp(float DeltaTime);
	// 本函数被每帧调用，随时间补充护盾值
	void ShieldRampUp(float DeltaTime);
	
private:
	// 本组件对象的所属角色
	UPROPERTY()
	class ABlasterCharacter* Character;

	/*
	 * Heal Buff 血量增益
	 */
	// 当前角色是否处于血量恢复状态(痊愈中状态)
	bool bHealing = false;
	// 血量恢复速率
	float HealingRate = 0.f;
	// 剩余的需要恢复的血量
	float AmountToHeal = 0.f;

	/*
	 * Shield Buff 护盾增益
	 */
	// 当前角色是否处于护盾恢复状态
	bool bReplenishingShield = false;
	// 护盾恢复速率
	float ShieldReplenishRate = 0.f;
	// 护盾恢复的总量
	float ShieldReplenishAmount = 0.f;
	
	/*
	 * Speed Buff 速度增益
	 */
	// 计时器，Speed Buff的持续时间结束后,要执行的函数ResetSpeeds
	FTimerHandle SpeedBuffTimer;
	// 重置玩家的移速为Speed buff生效之前的状态
	void ResetSpeeds();
	// Speed buff生效之前，玩家的基本移速
	float InitialBaseSpeed;
	// Speed buff生效之前，玩家的蹲伏移速
	float InitialCrouchSpeed;
	// 多播，通知所有客户端变更该玩家角色的速度
	UFUNCTION(NetMulticast, Reliable)
	void MulticastSpeedBuff(float BaseSpeedBuff, float CrouchSpeed);

	/*
	 * Jump Buff 跳跃增益
	 */
	// 计时器，Jump Buff的持续时间结束后,要执行的函数ResetJump
	FTimerHandle JumpBuffTimer;
	// 重置玩家的移速为Jump buff生效之前的状态
	void ResetJump();
	// Jump buff生效之前，玩家的基本跳跃速度
	float InitialJumpVelocity;
	// 多播，通知所有客户端变更该玩家的z轴跳跃速率
	UFUNCTION(NetMulticast, Reliable)
	void MulticastJumpBuff(float JumpVelocity);
};
