// Fill out your copyright notice in the Description page of Project Settings.


#include "FlagZone.h"

#include "Components/SphereComponent.h"
#include "MyBlaster/GameMode/CaptureFlagGameMode.h"
#include "MyBlaster/Weapon/Flag.h"

// Sets default values
AFlagZone::AFlagZone()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	// 本类不需要每帧执行，关闭以提升性能
	PrimaryActorTick.bCanEverTick = false;

	// 创建碰撞检测区
	ZoneSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ZoneSphere"));
	// 将其设置为根组件
	SetRootComponent(ZoneSphere);
}

// Called when the game starts or when spawned
void AFlagZone::BeginPlay()
{
	Super::BeginPlay();

	ZoneSphere->OnComponentBeginOverlap.AddDynamic(this, &AFlagZone::OnSphereOverlap);
}

void AFlagZone::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 将碰撞对象转换为旗子
	AFlag* OverlappingFlag = Cast<AFlag>(OtherActor);
	if(OverlappingFlag)
	{
		// 判断旗子所属队伍与本判定区所属队伍是否一致，不一致才得分
		if(OverlappingFlag->GetTeam() != Team)
		{
			// 获取游戏模式并转换为夺旗模式
			ACaptureFlagGameMode* GameMode = Cast<ACaptureFlagGameMode>(GetWorld()->GetAuthGameMode());
			if(GameMode)
			{
				// 调用夺旗模式的夺旗得分判定
				GameMode->FlagCapture(OverlappingFlag, this);
				
			}
			// 重置旗子的状态
            OverlappingFlag->ResetFlag();
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("得分区发生碰撞！"));
}

// Called every frame
void AFlagZone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

