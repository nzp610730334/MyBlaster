// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "RocketMovementComponent.generated.h"

/**
 * 
 */
UCLASS()
class MYBLASTER_API URocketMovementComponent : public UProjectileMovementComponent
{
	GENERATED_BODY()
protected:
	/** 重写父类的方法
	 * 处理模拟更新过程中的阻塞命中。检查碰撞后模拟是否保持有效。
	 * 如果进行模拟，则调用HandleImpact（），并默认返回EHandleHitWallResult:：Deflect，以通过HandleDeflection（）启用多反弹和滑动支持。
	 * 如果不再模拟，则返回EHandleHitWallResult:：Abort，这将中止进一步模拟的尝试。
	 * @param  Hit						Blocking hit that occurred.	发生了阻塞命中。
	 * @param  TimeTick					Time delta of last move that resulted in the blocking hit.	导致拦网命中的最后一次移动的时间增量。
	 * @param  MoveDelta				Movement delta for the current sub-step.	当前子步骤的移动增量。
	 * @param  SubTickTimeRemaining		How much time to continue simulating in the current sub-step, which may change as a result of this function.	在当前子步骤中继续模拟的时间，该时间可能会因此功能而更改。
	 *									Initial default value is: TimeTick * (1.f - Hit.Time)	初始默认值为：TimeTick*（1.f-Hit.Time）
	 * @return Result indicating how simulation should proceed.	指示模拟应如何进行的结果。
	 * @see EHandleHitWallResult, HandleImpact()
	 */
	virtual EHandleBlockingHitResult HandleBlockingHit(const FHitResult& Hit, float TimeTick, const FVector& MoveDelta, float& SubTickTimeRemaining) override;

	/** 重写父类的方法
	 * 如果启用反弹逻辑以影响碰撞时的速度（使用ComputeBounceResult（）），则应用反弹逻辑，
	 * 如果未启用反弹或速度低于BounceVelocityStopSimulatingThreshold，则停止投射物。
	 * 触发适用的事件（OnProjectleBounce）。
	 */
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta) override;
};
