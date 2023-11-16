// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PickupSpawnPoint.generated.h"

/*
 * 本类是道具生成点相关的类，负责经过随机时间间隔后生成道具。
 */
UCLASS()
class MYBLASTER_API APickupSpawnPoint : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APickupSpawnPoint();
	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 存放Pickup道具类的所有子类的数组
	UPROPERTY(EditAnywhere)
	TArray<TSubclassOf<class APickup>> PickupClasses;

	// 本Actor生成的道具对象，使用UPROPERTY()后，SpawnedPickup将被默认初始化为null
	UPROPERTY()
	APickup* SpawnedPickup;
	
	// 随机生成哪种道具
	void SpawnPickup();
	// 校验本地机器的身份等级(服务器or客户端)，然后生成道具，这是计时器到点需要执行的函数
	void SpawnPickupTimerFinished();
	// 该函数开始设置一个控制道具生成的计时器。该函数需要绑定到动态多播委托，因此需要使用UFUNCTION()标记
	UFUNCTION()
	void StartSpawnPickupTimer(AActor* DestroyedActor);

private:
	// 计时器，控制道具的生成间隔
	FTimerHandle SpawnPickupTimer;

	// 最小的生成时间间隔
	UPROPERTY(EditAnywhere)
	float SpawnPickupTimeMin;
	// 最大的生成时间间隔
	UPROPERTY(EditAnywhere)
	float SpawnPickupTimeMax;
public:	

};
