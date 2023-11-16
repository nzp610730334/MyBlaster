// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CombatComponent.h"
#include "AICombatComponent.generated.h"

/**
 * 这是专用于AI的战斗组件，继承自战斗组件
 * 因为父类战斗组件的HitTarget的获取涉及玩家的屏幕窗口等属性，AI没有屏幕窗口，因此不能满足AI的使用要求。
 * 本类改写了Hitarget的设置更新，以及增加了TargetActor属性，未被ue编辑器的AI控制器设置TargetActor时，Hittarget使用AI头部前方一定距离的扫描木雕的位置
 */
UCLASS()
class MYBLASTER_API UAICombatComponent : public UCombatComponent
{
	GENERATED_BODY()

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void TraceUnderCrosshairs(FHitResult& TraceHitResult) override;

	// AI视野范围。注意此处的视野仅用于AI准星射线的长度，
	UPROPERTY(EditAnywhere)
	float FieldOfView = 500.f;

	// 设置友元，玩家角色ABlasterCharacter需要能访问战斗组件UCombatComponent的所有内容
	friend class AAIBlasterCharacter;

	// 当前攻击目标
	UPROPERTY(BlueprintReadWrite)
	class AActor* TargetActor;

	// 清空攻击目标
	UFUNCTION(BlueprintCallable)
	void ClearTargetActor();
};
