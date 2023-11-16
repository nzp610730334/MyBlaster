// Fill out your copyright notice in the Description page of Project Settings.


#include "AICombatComponent.h"

#include "MyBlaster/Character/BlasterCharacter.h"

void UAICombatComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                       FActorComponentTickFunction* ThisTickFunction)
{
	UCombatComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 绘制子弹飞行轨迹，需要的参数
	FHitResult HitResult;
	// 传入HitResult接收射线碰撞检测结果
	TraceUnderCrosshairs(HitResult);
	// 当前没有攻击目标时，HitTarget设为头部瞄准方向
	if(TargetActor == nullptr)
	{
		// 从结果中取出碰撞点
		HitTarget = HitResult.ImpactPoint;
		UE_LOG(LogTemp, Warning, TEXT("当前没有攻击目标时，HitTarget设为头部瞄准方向！！"));
	}
	// 当前有攻击目标时，HitTarget设为攻击目标的位置
	else
	{
		HitTarget = TargetActor->GetActorLocation();
		UE_LOG(LogTemp, Warning, TEXT("当前有攻击目标时，HitTarget设为攻击目标的位置！！"));
	}
}

void UAICombatComponent::TraceUnderCrosshairs(FHitResult& TraceHitResult)
{
	if(Character == nullptr || Character->GetMesh() == nullptr) return;
	// 获取AI角色网格体的头部位置
	FVector HeadVector = Character->GetMesh()->GetBoneLocation("head");
	// 获取AI角色的朝向
	FVector HeadDirdection = Character->GetActorRotation().Vector();
	//
	FVector StartToEnd = HeadDirdection * FieldOfView;
	//
	FVector End = HeadVector + StartToEnd;
	DrawDebugLine(GetWorld(), HeadVector, End, FColor::Red);
	TraceHitResult.ImpactPoint = End;

	// GetWorld()->LineTraceSingleByChannel(
	// 	TraceHitResult,
	// 	HeadVector,
	// 	End,
	// 	// 碰撞检测通道
	// 	ECollisionChannel::ECC_Visibility
	// 	);
	// 没检测到东西时，将瞄准目标点设置为前方某段距离的点(警戒点)
	// if(!TraceHitResult.bBlockingHit)
	// {
	// 	TraceHitResult.ImpactPoint = End;
	// 	UE_LOG(LogTemp, Warning, TEXT("没检测到东西时，将瞄准目标点设置为前方某段距离的点！！"));
	// }
}

void UAICombatComponent::ClearTargetActor()
{
	// if(TargetActor)
	// {
	// 	TargetActor = nullptr;
	// }
	TargetActor = nullptr;
	// GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, TEXT("Target已被设置为nullptr"));
}
