// Fill out your copyright notice in the Description page of Project Settings.


#include "Casing.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"

// Sets default values
ACasing::ACasing()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// 构造弹壳静态网格体
	CasingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CasingMesh"));
	// 设置根组件
	SetRootComponent(CasingMesh);
	// 设置弹壳和摄像机碰撞为忽略
	CasingMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	// 开启模拟物理
	CasingMesh->SetSimulatePhysics(true);
	//应用重力
	CasingMesh->SetEnableGravity(true);
	// 开启碰撞通知
	CasingMesh->SetNotifyRigidBodyCollision(true);
	
	// 设置默认的弹壳抛出力度
	ShellEjectionImpulse = 10.f;
	
}

// Called when the game starts or when spawned
void ACasing::BeginPlay()
{
	Super::BeginPlay();

	// 绑定碰撞事件
	CasingMesh->OnComponentHit.AddDynamic(this, &ACasing::OnHit);
	
	// 给予一个方向的力.
	// 注意：GetActorForwardVector() 返回的是一个标准化的向量，长度为1，需要乘上力度得到一个具体的矢量力。
	CasingMesh->AddImpulse(GetActorForwardVector() * ShellEjectionImpulse);
}

// Called every frame
void ACasing::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ACasing::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpilse, const FHitResult& Hit)
{
	// 播放声音
	if(ShellSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ShellSound, GetActorLocation());
	}
	// 销毁
	Destroy();
}

