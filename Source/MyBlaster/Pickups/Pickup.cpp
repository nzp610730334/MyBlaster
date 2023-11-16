// Fill out your copyright notice in the Description page of Project Settings.


#include "Pickup.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "MyBlaster/Weapon/WeaponTypes.h"
#include "Sound/SoundCue.h"

// Sets default values
APickup::APickup()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// 被拾取之后需要被销毁，即服务器和客户端上都需要被销毁，因此应该是可复制的Actor
	bReplicates = true;

	// 创建一个场景组件代替从父类继承而来的默认的根组件，作为新的根组件
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	// 创建球体组件作为检测用的球体
	OverlapSphere = CreateDefaultSubobject<USphereComponent>(TEXT("OverlapSphere"));
	// 将其附着到根组件
	OverlapSphere->SetupAttachment(RootComponent);
	// 设置球体大小
	OverlapSphere->SetSphereRadius(150.f);
	// 检测球体只需要检测事件，不需要物理反馈
	OverlapSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	// 检测球体的所有碰撞响应通道设置均为忽略
	OverlapSphere->SetCollisionResponseToChannels(ECollisionResponse::ECR_Ignore);
	// 单独设置检测球体的pawn碰撞响应通道的开启，开启为overlap可穿透
	OverlapSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
	// 设置检测球体的z轴高度为85，其余保持不变
	// 局部旋转
	// OverlapSphere->AddLocalOffset(FVector(0.f, 0.f, 85.f));
	// 世界旋转
	OverlapSphere->AddWorldOffset(FVector(0.f, 0.f, 85.f));

	// 创建可拾取道具的静态网格体
	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	// 将其附着到根组件
	PickupMesh->SetupAttachment(RootComponent);
	// 可拾取道具的网格体仅作为视觉效果，因此无需碰撞。碰撞检测逻辑由检测球体OverlapSphere处理
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 设置网格体的3D相对缩放倍数为3
	PickupMesh->SetRelativeScale3D(FVector(3.f, 3.f, 3.f));
	// 开启网格体的自定义深度
	PickupMesh->SetRenderCustomDepth(true);
	// 设置自定义深度的值为紫色的值
	PickupMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_PURPLE);
	
	// 创建道具的视觉效果
	PickupEffectComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PickupEffectComponent"));
	// 将其附着到根组件
	PickupEffectComponent->SetupAttachment(RootComponent);
	
}

// Called every frame
void APickup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 处理道具自转，先确定网格体已设置
	if(PickupMesh)
	{
		/*
		 * 增加道具网格体的旋转角度，达到旋转的视觉效果。
		 * 注意，网格体的旋转没有被网络复制，只是因为服务器和客户端都在Tick函数里面执行了下面的代码AddLocalRotation
		 */ 
		PickupMesh->AddLocalRotation(FRotator(0.f, BaseTurnRate * DeltaTime, 0.f));
	}
}

void APickup::Destroyed()
{
	Super::Destroyed();

	// 判断拾取声音已经设置
	if(PickupCueSound)
	{
		// 播放道具被拾取的声音
		UGameplayStatics::PlaySoundAtLocation(
			// 上下文对象，即本对象
			this,
			// 要播放的声音
			PickupCueSound,
			// 播放地点(在本Actor的位置播放)
			GetActorLocation()
		);
	}
	
	// 确定销毁效果已经设置
	if(PickupEffect)
	{
		// 产生道具被拾取时的销毁效果
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			// 上下文对象
			this,
			PickupEffect,
			GetActorLocation(),
			GetActorRotation()
		);
	}
	
}

// Called when the game starts or when spawned
void APickup::BeginPlay()
{
	Super::BeginPlay();

	// 只在服务器上才绑定拾取响应的逻辑
	if(HasAuthority())
	{
		GetWorldTimerManager().SetTimer(
			BindOverlapTimer,
			this,
			&APickup::BindOverlapTimerFinished,
			BindOverlapTime);
	}
}

void APickup::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	
}

void APickup::BindOverlapTimerFinished()
{
	// 绑定碰撞重叠函数。让碰撞检测球体OverlapSphere发生OnComponentBeginOverlap重叠事件开始时，执行碰撞重叠函数OnSphereOverlap。
	OverlapSphere->OnComponentBeginOverlap.AddDynamic(this, &APickup::OnSphereOverlap);
}



