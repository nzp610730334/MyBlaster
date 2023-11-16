// Fill out your copyright notice in the Description page of Project Settings.


#include "PickupSpawnPoint.h"

#include "Pickup.h"

// Sets default values
APickupSpawnPoint::APickupSpawnPoint()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// 本类对象的spawn生成只在服务器上，因此需要设置为可网络复制，才能复制到所有客户端
	bReplicates = true;
}

// Called when the game starts or when spawned
void APickupSpawnPoint::BeginPlay()
{
	Super::BeginPlay();

	/*
	 * 游戏开始时或本Actor被Spawn时就进入生成道具的周期。因为是第一次执行，因此无法传入一个已生成的SpawnedActor道具对象，
	 * 因此传入转换成AActor类型的空指针。通常情况下指针转换类型是十分危险的，但空指针的雷翔转换除外？
	 */ 
	StartSpawnPickupTimer((AActor*)nullptr);
}

void APickupSpawnPoint::SpawnPickup()
{
	// 获取道具类数组长度
	int32 NumPickupClasses = PickupClasses.Num();
	// 数组长度大于0时才有用于生成的道具类
	if(NumPickupClasses > 0)
	{
		// 随机一个索引号，该索引号的道具类将用于后续生成
		int32 Selection = FMath::RandRange(0, NumPickupClasses - 1);
		// 根据索引号，生成对应的道具。生成的道具的Transform与本对象保持一致。
		SpawnedPickup = GetWorld()->SpawnActor<APickup>(PickupClasses[Selection], GetActorTransform());

		/*
		 * 绑定委托：将开始下一个道具生成周期的函数StartSpawnPickupTimer绑定到生成的道具SpawnedPickup的销毁事件上。
		 * SpawnedPickup被销毁时，StartSpawnPickupTimer将被执行。
		 * 疑惑：为什么不在Begin中处理绑定行为？是因为SpawnedPickup默认初始化为null，无法绑定？
		 * 注意：该行为同样只发生在服务器上。且SpawnPickup在此之前要成功生成
		 */
		if(HasAuthority() && SpawnedPickup)
		{
			SpawnedPickup->OnDestroyed.AddDynamic(this, &APickupSpawnPoint::StartSpawnPickupTimer);
		}
	}
}

void APickupSpawnPoint::SpawnPickupTimerFinished()
{
	// 生成道具的行为只发生在服务器上，且本类的对象需要被设置为可复制，才能网络复制到所有客户端上
	if(HasAuthority())
	{
		// 随机生成道具
		SpawnPickup();
	}
}

void APickupSpawnPoint::StartSpawnPickupTimer(AActor* DestroyedActor)
{
	// 根据最大最小生成时间间隔，随机获得本次的时间间隔，因为不会被修改，因此使用const修饰
	const float SpawnTime = FMath::RandRange(SpawnPickupTimeMin, SpawnPickupTimeMax);
	// 设置计时器
	GetWorldTimerManager().SetTimer(
		// 生成的新的计时器将会赋值给SpawnPickupTimer
		SpawnPickupTimer,
		// 哪个对象调用该计时器函数
		this,
		// 计时器要调用的方法 的内存地址
		&APickupSpawnPoint::SpawnPickupTimerFinished,
		// 计时器调用函数的时间间隔是多少
		SpawnTime);
}

// Called every frame
void APickupSpawnPoint::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

