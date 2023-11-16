// Fill out your copyright notice in the Description page of Project Settings.

/*
 * 本类是地面上的可拾取的道具类
 */
#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Actor.h"
#include "Pickup.generated.h"

UCLASS()
class MYBLASTER_API APickup : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APickup();
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	// 父类的销毁方法不能满足本类的需求，因此需要将其重写
	virtual void Destroyed() override;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 碰撞重叠事件时，需要执行的函数。本基类的该方法暂时留空，子类需要覆盖实现它们实际需要的功能。
	UFUNCTION()
	virtual void OnSphereOverlap(
		// 碰撞检测球体组件
		UPrimitiveComponent* OverlappedComponent,
		// 碰撞的Actor
		AActor* OtherActor,
		// 碰撞的Actor的组件
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		// 碰撞结果
		const FHitResult& SweepResult
	);

private:
	// 碰撞检测球体组件
	UPROPERTY(EditAnywhere)
	class USphereComponent* OverlapSphere;

	// 被拾取时播放的声音(被拾取后销毁，销毁时播放)
	UPROPERTY(EditAnywhere)
	class USoundCue* PickupCueSound;

	// 该可拾取道具的网格体
	UPROPERTY(EditAnywhere)
	class UStaticMeshComponent* PickupMesh;

	// 道具自转速率
	float BaseTurnRate = 45.f;
	
	// 道具在关卡中的视觉效果
	UPROPERTY(VisibleAnywhere)
	class UNiagaraComponent* PickupEffectComponent;

	// 道具被拾取后产生的视觉效果
	UPROPERTY(EditAnywhere)
	class UNiagaraSystem* PickupEffect;

	/*
	 * PickupSpawnPoint中，通过道具生成点的道具被生成后，如果马上被玩家拾取并销毁了，
	 * 那么代码还没执行到将再次生成道具的函数StartSpawnPickupTimer绑定到道具对象的销毁事件的多播委托的那一步。
	 * 这将导致处理下一次道具生成的StartSpawnPickupTimer函数没有被正确执行，从而导致业务层面上的道具再也不会生成。
	 * 解决方法：使用计时器延迟绑定本对象的碰撞覆盖事件的委托。
	 * 缺陷：这种方案会导致道具生成后，一直站在上面的玩家无法拾取，玩家必须从道具对象的碰撞覆盖范围中出去再进来才能重新触发碰撞重叠事件。
	 */
	// 用于延迟绑定碰撞覆盖事件委托的计时器
	FTimerHandle BindOverlapTimer;
	// 延迟的时长。这时长足以让道具对象被spawn后，再完成自身销毁事件和StartSpawnPickupTimer的委托绑定。
	float BindOverlapTime = 0.25f;
	// 计时器到时需要执行的函数，它将碰撞覆盖函数绑定委托到本对象的碰撞覆盖时间。
	void BindOverlapTimerFinished();

public:
	
};
