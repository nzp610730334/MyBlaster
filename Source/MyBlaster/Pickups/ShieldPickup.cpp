// Fill out your copyright notice in the Description page of Project Settings.


#include "ShieldPickup.h"

#include "MyBlaster/BlasterComponents/BuffComponent.h"
#include "MyBlaster/Character/BlasterCharacter.h"


void AShieldPickup::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 这里还是照旧调用，虽然父类的OnSphereOverlap当前为空，但以后可能添加功能
	Super::OnSphereOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
	
	// 尝试转换与本对象发生碰撞重叠的Actor为玩家角色类
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);
	// 转换成功
	if(BlasterCharacter)
	{
		// 获取玩家角色的buff组件
		UBuffComponent* Buff = BlasterCharacter->GetBuff();
		// 获取成功
		if(Buff)
		{
			// 调用buff组件的补充玩家当前护盾值的方法
			Buff->ReplenishShield(ShieldReplenishAmount, ShieldReplenishTime);
		}
	}
	// 销毁本对象,本对象继承的销毁函数Destroyed在Destroy函数执行的过程中被执行
	Destroy();
}

