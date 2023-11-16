// Fill out your copyright notice in the Description page of Project Settings.


#include "AmmoPickup.h"

#include "MyBlaster/BlasterComponents/CombatComponent.h"
#include "MyBlaster/Character/BlasterCharacter.h"

void AAmmoPickup::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 父类的OnSphereOverlap当前为空，但以后可能添加功能，因此这里照例先调用。
	Super::OnSphereOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);

	// 尝试转换与本对象发生碰撞重叠的Actor为玩家角色类
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);
	// 转换成功
	if(BlasterCharacter)
	{
		// 获取玩家角色的战斗组件
		UCombatComponent* Combat = BlasterCharacter->GetCombat();
		// 获取成功
		if(Combat)
		{
			// 调用战斗组件的添加弹药函数
			Combat->PickupAmmo(WeaponType, AmmoAmount);
		}
	}
	// 销毁本对象,本对象继承的销毁函数Destroyed在Destroy函数执行的过程中被执行
	Destroy();
}
