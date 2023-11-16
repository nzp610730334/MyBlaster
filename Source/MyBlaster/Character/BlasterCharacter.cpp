// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterCharacter.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "MyBlaster/BlasterComponents/CombatComponent.h"
#include "MyBlaster/Weapon/Weapon.h"
#include "Net/UnrealNetwork.h"
#include "MyBlaster/MyBlaster.h"
#include "MyBlaster/BlasterComponents/BuffComponent.h"
#include "MyBlaster/BlasterComponents/LagCompensationComponent.h"
#include "MyBlaster/BlasterTypes/CharacterGameAttributes.h"
#include "MyBlaster/GameMode/BlasterGameMode.h"
#include "MyBlaster/GameState/BlasterGameState.h"
#include "MyBlaster/PlayerController/BlasterPlayerController.h"
#include "MyBlaster/PlayerStart/TeamPlayerStart.h"
#include "MyBlaster/PlayerState/BlasterPlayerState.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"

// Sets default values
ABlasterCharacter::ABlasterCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh());
	// 将相机臂附加到胶囊体，则玩家下蹲时，胶囊体体积减半，相机镜头也随之下降，但目前的处理导致下降较为生硬，暂不使用
	// CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->TargetArmLength = 600.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	// OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	// OverheadWidget->SetupAttachment(RootComponent);

	// 创建组件并设置为可复制（组件无需注册即可设置可复制）
	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	Combat->SetIsReplicated(true);
	// 创建buff组件，并设置为可网络复制
	Buff = CreateDefaultSubobject<UBuffComponent>(TEXT("BuffComponent"));
	Buff->SetIsReplicated(true);
	// 创建演出补偿组件，它仅在服务器上被使用到，因此不需要网络复制
	LagCompensation = CreateDefaultSubobject<ULagCompensationComponent>(TEXT("LagCompensation"));

	// 创建手榴弹静态网格体
	AttachedGrenade = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Attached Grenadde"));
	// 将其附着到玩家角色网格体的手部对应的手榴弹插槽
	AttachedGrenade->SetupAttachment(GetMesh(), FName("GrenadeSocket"));
	// 将其碰撞设置为无碰撞
	AttachedGrenade->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 设置角色可下蹲。也能在角色蓝图中的movement组件中character movement capabities中设置CanCrouch
	GetMovementComponent()->NavAgentProps.bCanCrouch = true;

	// 设置胶囊体忽略和摄像机的阻挡
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	// 设置玩家角色碰撞类型为自定义碰撞类型ECC_SkeletaMesh
	GetMesh()->SetCollisionObjectType(ECC_SkeletaMesh);
	// 设置网格体忽略世界内存在的其他摄像机的阻挡
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	// 设置网格体应用通道为ECC_Visibility的射线的阻挡？即通道channel为ECC_Visibility的射线会碰撞本对象的网格体。
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	
	// 设置角色转身速度。（绕Z轴的旋转速度）
	// 教程传的参数不正确，FRotator的三个参数分别是绕x轴、绕z轴、绕y轴。这里需要改变的是角色转身速度，也就是绕z轴的旋转速度。
	// GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 850.f);
	GetCharacterMovement()->RotationRate = FRotator(0.f, 850.f, 0.f);

	// 初始化转身tag
	TurningInPlace = ETurningInPlace::ETIP_NotTurning;

	// 网络更新频率
	NetUpdateFrequency = 66.f;
	MinNetUpdateFrequency = 33.f;

	// 创建时间线组件
	DissolveTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DissolveTimelineComponent"));

	/*
	 * Hit Boxes used for Server-side rewind
	 * 创建盒子组件，用于服务器倒带的碰撞盒子。为了方便，名称与角色骨骼保持一致(包括大小写)
	 * 疑惑：不同角色模型HitBoxes数量也不相同，下面的操作是否应该重构为一个方法。比如使用策略模式？
	 */
	// 创建组件，新建一个值为head的TEXT作为参数传入，将组件的名称显示设置为head
	head = CreateDefaultSubobject<UBoxComponent>(TEXT("head"));
	// 将该组件附着到网格体，里面的head插槽
	head->SetupAttachment(GetMesh(), FName("head"));
	// 将创建的组件添加到创建的碰撞盒子映射中
	HitCollisionBoxes.Add(FName("head"), head);

	pelvis = CreateDefaultSubobject<UBoxComponent>(TEXT("pelvis"));
	pelvis->SetupAttachment(GetMesh(), FName("pelvis"));
	HitCollisionBoxes.Add(FName("pelvis"), pelvis);

	spine_02 = CreateDefaultSubobject<UBoxComponent>(TEXT("spine_02"));
	spine_02->SetupAttachment(GetMesh(), FName("spine_02"));
	HitCollisionBoxes.Add(FName("spine_02"), spine_02);

	spine_03 = CreateDefaultSubobject<UBoxComponent>(TEXT("spine_03"));
	spine_03->SetupAttachment(GetMesh(), FName("spine_03"));
	HitCollisionBoxes.Add(FName("spine_03"), spine_03);

	upperarm_l = CreateDefaultSubobject<UBoxComponent>(TEXT("upperarm_l"));
	upperarm_l->SetupAttachment(GetMesh(), FName("upperarm_l"));
	HitCollisionBoxes.Add(FName("upperarm_l"), upperarm_l);

	upperarm_r = CreateDefaultSubobject<UBoxComponent>(TEXT("upperarm_r"));
	upperarm_r->SetupAttachment(GetMesh(), FName("upperarm_r"));
	HitCollisionBoxes.Add(FName("upperarm_r"), upperarm_r);

	lowerarm_l = CreateDefaultSubobject<UBoxComponent>(TEXT("lowerarm_l"));
	lowerarm_l->SetupAttachment(GetMesh(), FName("lowerarm_l"));
	HitCollisionBoxes.Add(FName("lowerarm_l"), lowerarm_l);

	lowerarm_r = CreateDefaultSubobject<UBoxComponent>(TEXT("lowerarm_r"));
	lowerarm_r->SetupAttachment(GetMesh(), FName("lowerarm_r"));
	HitCollisionBoxes.Add(FName("lowerarm_r"), lowerarm_r);
	
	hand_l = CreateDefaultSubobject<UBoxComponent>(TEXT("hand_l"));
	hand_l->SetupAttachment(GetMesh(), FName("hand_l"));
	HitCollisionBoxes.Add(FName("hand_l"), hand_l);

	hand_r = CreateDefaultSubobject<UBoxComponent>(TEXT("hand_r"));
	hand_r->SetupAttachment(GetMesh(), FName("hand_r"));
	HitCollisionBoxes.Add(FName("hand_r"), hand_r);

	backpack = CreateDefaultSubobject<UBoxComponent>(TEXT("backpack"));
	backpack->SetupAttachment(GetMesh(), FName("backpack"));
	HitCollisionBoxes.Add(FName("backpack"), backpack);

	blanket = CreateDefaultSubobject<UBoxComponent>(TEXT("blanket"));
	// 背包上的毯子附着到背包骨骼backpack上
	blanket->SetupAttachment(GetMesh(), FName("backpack"));
	HitCollisionBoxes.Add(FName("blanket"), blanket);

	thigh_l = CreateDefaultSubobject<UBoxComponent>(TEXT("thigh_l"));
	thigh_l->SetupAttachment(GetMesh(), FName("thigh_l"));
	HitCollisionBoxes.Add(FName("thigh_l"), thigh_l);

	thigh_r = CreateDefaultSubobject<UBoxComponent>(TEXT("thigh_r"));
	thigh_r->SetupAttachment(GetMesh(), FName("thigh_r"));
	HitCollisionBoxes.Add(FName("thigh_r"), thigh_r);

	calf_l = CreateDefaultSubobject<UBoxComponent>(TEXT("calf_l"));
	calf_l->SetupAttachment(GetMesh(), FName("calf_l"));
	HitCollisionBoxes.Add(FName("calf_l"), calf_l);

	calf_r = CreateDefaultSubobject<UBoxComponent>(TEXT("calf_r"));
	calf_r->SetupAttachment(GetMesh(), FName("calf_r"));
	HitCollisionBoxes.Add(FName("calf_r"), calf_r);

	foot_l = CreateDefaultSubobject<UBoxComponent>(TEXT("foot_l"));
	foot_l->SetupAttachment(GetMesh(), FName("foot_l"));
	HitCollisionBoxes.Add(FName("foot_l"), foot_l);

	foot_r = CreateDefaultSubobject<UBoxComponent>(TEXT("foot_r"));
	foot_r->SetupAttachment(GetMesh(), FName("foot_r"));
	HitCollisionBoxes.Add(FName("foot_r"), foot_r);

	// 遍历所有碰撞盒子，将碰撞检测类型修改为自定义的ECC_HitBox,
	for(auto Box : HitCollisionBoxes)
	{
		// 先确定碰撞盒子正常存在
		if(Box.Value)
		{
			// 更改此对象(碰撞盒子)移动时使用的碰撞通道为自定义碰撞类型ECC_HitBox
			Box.Value->SetCollisionObjectType(ECC_HitBox);
			// 更改碰撞盒子对忽略所有碰撞检测通道
			Box.Value->SetCollisionResponseToChannels(ECollisionResponse::ECR_Ignore);
			// 单独开启对ECC_HitBox检测通道的阻塞
			Box.Value->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
			// 先关闭碰撞盒子的碰撞检测，需要的时候(命中判定时)才开启
			Box.Value->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

void ABlasterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 无条件复制ABlasterCharacter的属性OverlappingWeapon到每个客户端上
	// DOREPLIFETIME(ABlasterCharacter, OverlappingWeapon);
	// 有条件的复制,客户端机器A是某个ABlasterCharacter的所有者，那么该ABlasterCharacter的OverlappingWeapon会被复制并发送给机器A
	DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappingWeapon, COND_OwnerOnly);
	// 每个客户端的所有玩家的当前血量都需要复制，因此是无条件复制
	DOREPLIFETIME(ABlasterCharacter, Health);
	DOREPLIFETIME(ABlasterCharacter, Shield);
	DOREPLIFETIME(ABlasterCharacter, bDisableGameplay);

	// 2023年5月16日22:30:07 即便复制了属性依旧导致玩家和服务器以外的第三者看到玩家左右偏移抽搐
	// 角色瞄准偏移(无条件复制)
	// DOREPLIFETIME(ABlasterCharacter, AO_Yaw);
	// DOREPLIFETIME(ABlasterCharacter, AO_Pitch);
}

void ABlasterCharacter::DropOrDestroyWeapon(AWeapon* Weapon)
{
	// 传入的武器为空则直接返回
	if(Weapon == nullptr) return;
	// 如果该tag为true，则说明是开局武器，需要被销毁
	if(Weapon->bDestroyWeapon)
	{
		Weapon->Destroy();
	}
	// 否则该武器需要被掉落
	else
	{
		Weapon->Dropped();
	}
}

void ABlasterCharacter::DropOrDestroyWeapons()
{
	// 角色死亡，调用武器的掉落处理，注意先判断要访问的对象是否为空
	if(Combat)
	{
		// 判断主武器是否存在
		if(Combat->EquippedWeapon)
		{
			// 处理该武器是否应该被掉落
			DropOrDestroyWeapon(Combat->EquippedWeapon);
		}
		// 判断副武器是否存在
		if(Combat->SecondaryWeapon)
		{
			// 处理该武器是否应该被掉落
			DropOrDestroyWeapon(Combat->SecondaryWeapon);
		}
		// 判断角色是否持有旗子
		if(Combat->TheFlag)
		{
			// 执行旗子掉落的行为
			DropOrDestroyWeapon(Combat->TheFlag);
		}
	}
}

void ABlasterCharacter::SetSpawnPoint()
{
	// 队伍出生点的设置，仅在服务器上发生，且玩家队伍身份不为NoTeam时(即当前处于团队模式下)
	if(HasAuthority() && BlasterPlayerState->GetTeam() != ETeam::ET_NoTeam)
	{
		// 初始化新的数组，用于存储队伍出生点
		TArray<AActor*> PlayerStarts;
		// 获取世界场景中的所有队伍出生点类的对象，将结果存储在数组PlayerStarts中
		UGameplayStatics::GetAllActorsOfClass(this, ATeamPlayerStart::StaticClass(), PlayerStarts);
		// 初始化新的数组，用于存储与本玩家对象的阵营一致的出生点
		TArray<ATeamPlayerStart*> TeamPlayerStarts;
		// 遍历所有队伍出生点，存储与本玩家对象的阵营一致的出生点于数组中
		for(auto Start : PlayerStarts)
		{
			ATeamPlayerStart* TeamStart = Cast<ATeamPlayerStart>(Start);
			if(TeamStart && TeamStart->Team == GetTeam())
			{
				TeamPlayerStarts.Add(TeamStart);
				// 疑惑：为什么用的不是AddUnique
				// TeamPlayerStarts.AddUnique(TeamStart);
			}
		}
		// 与本玩家对象阵营一致的队伍出生点数组不为空时
		if(TeamPlayerStarts.Num() > 0)
		{
			// 从本玩家的队伍出生点中随机获取一个
			ATeamPlayerStart* ChosenPlayerStart = TeamPlayerStarts[FMath::RandRange(0, TeamPlayerStarts.Num() - 1)];
			// 将玩家角色设置到该出生点
			SetActorLocationAndRotation(
				ChosenPlayerStart->GetActorLocation(),
				ChosenPlayerStart->GetActorRotation()
				);
		}
	}
}

void ABlasterCharacter::OnPlayerStateInitialized()
{
	// 对玩家状态的游戏属性初始化
	BlasterPlayerState->AddToScore(0.f);
	BlasterPlayerState->AddToDefeats(0);

	// 设置该玩家的普通材质以及死亡时的溶解材质
	SetTeamColor(BlasterPlayerState->GetTeam());

	SetSpawnPoint();
}

void ABlasterCharacter::Destroyed()
{
	Super::Destroyed();
	
	// 判断要销毁的死亡粒子效果是否存在
	if(ElimBotComponent)
	{
		// 销毁角色死亡时生成的粒子效果
		ElimBotComponent->DestroyComponent();
	}
	/*
	 * 当玩家角色被销毁时，如果游戏阶段不处于游戏中的阶段，则一并销毁武器。处于其他阶段时玩家被销毁才掉落武器
	 */
	// 获取游戏模式
	// ABlasterGameMode* BlasterGameMode = Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this));
	BlasterGameMode = BlasterGameMode == nullptr ? GetWorld()->GetAuthGameMode<ABlasterGameMode>() : BlasterGameMode;
	/*
	 * 判断校验当前游戏阶段不处于游戏中。注意区分 MatchState和GameState。
	 * GameState管理与游戏有关的所有属性和状态，是一个类。而GetMatchState获取的只是一个FName，用于区分不同游戏阶段。
	 */ 
	bool bMatchNotInProgress = BlasterGameMode && BlasterGameMode->GetMatchState() != MatchState::InProgress;
	if(Combat && Combat->EquippedWeapon && bMatchNotInProgress)
	{
		Combat->EquippedWeapon->Destroy();
	}
}

void ABlasterCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 先确定战斗组件的存在
	if(Combat)
	{
		// 将玩家角色自身赋值给战斗系统的Character
		Combat->Character = this;
	}
	// 先确定Buff组件的存在
	if(Buff)
	{
		// 将玩家角色自身赋值给Buff组件的Character
		Buff->Character = this;
		// 记录玩家的初始速度
		Buff->SetInitialSpeeds(
			GetCharacterMovement()->MaxWalkSpeed,
			GetCharacterMovement()->MaxWalkSpeedCrouched
			);
		// 记录玩家角色的z轴跳跃速率
		Buff->SetInitialJumpVelocity(GetCharacterMovement()->JumpZVelocity);
	}
	// 先确定延迟组件的存在
	if(LagCompensation)
	{
		// 将本对象赋值给延迟补偿组件类的玩家角色变量
		LagCompensation->Character = this;
		// 确定控制器的存在
		if(Controller)
		{
			// 将控制器赋值给延迟补偿组件的控制器变量
			LagCompensation->Controller = Cast<ABlasterPlayerController>(Controller);
		}
	}
}

void ABlasterCharacter::SpawnDefaultWeapon()
{
	// 获取游戏模式
	BlasterGameMode = BlasterGameMode == nullptr ? GetWorld()->GetAuthGameMode<ABlasterGameMode>() : BlasterGameMode;
	// 获取世界关卡
	UWorld* World = GetWorld();
	/*
	 * 条件判断：
	 * 1.成功获取世界关卡；
	 * 2.游戏模式获取成功；
	 * 3.默认武器类已经设置；
	 * 4.玩家未被淘汰；
	 */
	if(BlasterGameMode && World && !bElimmed && DefaultWeaponClass)
	{
		// 生成默认武器并赋值存储为开局武器
		AWeapon* StartingWeapon = World->SpawnActor<AWeapon>(DefaultWeaponClass);
		// 该武器是开局武器，需要设置为 该被销毁。
		StartingWeapon->bDestroyWeapon = true;
		// Combat已被初始化完毕且开局武器生成成功
		if(Combat && StartingWeapon)
		{
			// 将开局武器装备
			Combat->EquipWeapon(StartingWeapon);
		}
	}
}

void ABlasterCharacter::MulticastGainedTheLead_Implementation()
{
	// 确定皇冠特效已经在蓝图中设置，为空指针时直接返回
	if(CrownSystem == nullptr) return;
	// 该特效没有被实例化生成在世界关卡中时，才进行spawn生成
	if(CrownComponent == nullptr)
	{
		// 以附着的方式生成特效，赋值缓存生成的特效，便于后续操作，如销毁等行为
		CrownComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			// 要生成的特效
			CrownSystem,
			// 生成之后该特效附着的场景组件。(附着在角色的胶囊体上，角色下蹲，胶囊体高度减半，特效也会随之降低)
			// GetCapsuleComponent(),
			// 生成之后该特效附着的场景组件。(附着在角色的网格体上)
			GetMesh(),
			// 附着点的名称(留空即可)
			FName(),
			// 生成的位置(角色actor位置的头上)
			GetActorLocation() + FVector(0.f, 0.f, 110.f),
			// 生成的朝向
			GetActorRotation(),
			// 自动计算相对变换，以便附着的零部件保持相同的世界变换
			EAttachLocation::KeepWorldPosition,
			// false不自动销毁。因为业务的需求后面会手动将其销毁
			false
		);
	}
	// 皇冠特效已被生成，则将其激活显示
	if(CrownComponent)
	{
		CrownComponent->Activate();
		// UE_LOG(LogTemp, Warning, TEXT("---------------------已激活皇冠特效！！"));
	}
}

void ABlasterCharacter::MulticastLostTheLead_Implementation()
{
	// 确定生成的特效存在
	if(CrownComponent)
	{
		// 将其销毁
		CrownComponent->DestroyComponent();
		UE_LOG(LogTemp, Warning, TEXT("|||||||||||||||||||||已销毁皇冠特效！！"));
	}
}

// Called when the game starts or when spawned
void ABlasterCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 生成开局武器并装备
	SpawnDefaultWeapon();
	/*
	 * 面生成了开局武器并装备，因此这里需要更新弹药HUD。
	 * 疑惑：SpawnDefaultWeapon里面调用的Combat->EquipWeapon已经有HUD的相关更新了，这里为什么多此一举？注释掉，开局武器的HUD弹药功能也正常
	 */ 
	UpdateHUDAmmo();
	
	// 初次设置玩家角色血量信息
	UpdateHUDHealth();

	// 初始化玩家角色的护盾值
	UpdateHUDShield();
	
	// 伤害处理前需要子弹碰撞，而子弹碰撞只发生在服务器上。此处也需要校验是否有权限(在服务器上)
	if(HasAuthority())
	{
		// 绑定委托，发生伤害处理时，调用函数ReceiveDamage，注意传的是函数的地址。
		OnTakeAnyDamage.AddDynamic(this, &ABlasterCharacter::ReceiveDamage);
	}
	// 玩家角色构建完毕后，先将手上的手榴弹隐藏
	if(AttachedGrenade)
	{
		AttachedGrenade->SetVisibility(false);
	}
}

void ABlasterCharacter::ReceiveDamage(AActor* DamageActor, float Damage, const UDamageType* DamageType,
	AController* InstigatorController, AActor* DamageCauser)
{
	// UE_LOG(LogTemp, Warning, TEXT("玩家即将受到伤害！！伤害值： %f"), Damage);
	
	// 获取并转换游戏模式
	// GetAuthGameMode()如果在服务器上执行，则return游戏模式，<ABlasterGameMode>为将return的结果尝试转换为ABlasterGameMode类型
	// BlasterGameMode = BlasterGameMode == nullptr ? Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this)) : BlasterGameMode;
	BlasterGameMode = BlasterGameMode == nullptr ? GetWorld()->GetAuthGameMode<ABlasterGameMode>() : BlasterGameMode;
	
	// 玩家角色已经死亡时，不该再触发伤害应用，直接返回。否则世界关卡中产生的范围伤害可能仍旧作用于玩家，导致死亡特效动画重复播放。
	// 游戏模式获取为空，则也直接返回
	if(bElimmed || BlasterGameMode == nullptr) return;

	// (多态的使用-策略模式？)根据当前游戏模式计算应得的伤害。传入攻击者的控制器、受击者（本对象）的控制器、原始的伤害
	Damage = BlasterGameMode->CalculateDamage(InstigatorController, Controller, Damage);

	// 初始化实际对血量造成的伤害
	float DamageToHealth = 0.f;
	// 护盾值大于等于本次伤害时，扣除护盾值，实际对血量造成的伤害为0
	if(Shield >= Damage)
	{
		// 同样使用Clamp将数值限制在符合业务逻辑的范围内
		Shield = FMath::Clamp(Shield - Damage, 0.f, MaxShield);
		DamageToHealth = 0.f;
	}
	/*
	 * 护盾值小于本次伤害时，实际对血量造成的伤害 = 本次所受伤害 - 护盾值，然后将护盾值为0
	 * 原教程就是先设置Shield为0，然后本次伤害全打血量上。
	 * 后期改进思路：通常玩法设计中，护盾值小于本次伤害，应该抵挡等值于护盾值伤害，剩下的再扣血
	 */ 
	else
	{
		// Shield = 0.f;	// P203教程中已修正先扣除护盾再计算伤害的该问题。
		DamageToHealth = FMath::Clamp(Damage - Shield, 0.f, Damage);
		Shield = 0.f;	// 后期改进
	}
	
	// 将Health - DamageToHealth伤害处理结果限制在符合逻辑的范围内，[0.f, MaxHealth]。Health是变动的，因此不该作为限制的最大值。
	Health = FMath::Clamp(Health - DamageToHealth, 0.f, MaxHealth);
	// 刷新角色血量信息
	UpdateHUDHealth();
	// 刷新角色护盾值信息
	UpdateHUDShield();
	
	/*
	 * 多人游戏中，应尽量减少RPC的调用。原本逻辑为角色被子弹打中，调用多播RPC通知所有客户端播放受击动画。
	 * Health本来就需要被网络复制更新，因此可以优化为
	 * 在Health被更新后，在OnRep_Health函数中播放角色受击动画即可。所有的客户端的该角色的Health都会被更新，因此所有客户端都会播放该角色受击动画。
	 * 但OnRep_Health不会在服务器上执行，导致服务器上的角色不会播放受击动画。
	 * 本函数ReceiveDamage绑定了伤害处理委托，伤害处理因子弹碰撞而执行，子弹碰撞设计为只在服务器上执行。因此可以在此播放角色受击动画作为弥补。
	 */
	// 播放角色受击动画。
	PlayHitReactMontage();
	// 血量少于等于0时，判定角色死亡
	// if(Health <= 0.f)
	// {
	// 	BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
	// 	class ABlasterPlayerController* AttackerController = Cast<ABlasterPlayerController>(InstigatorController);
	// 	ABlasterGameMode* BlasterGameMode = GetWorld()->GetAuthGameMode<ABlasterGameMode>();
	// 	if(BlasterGameMode && BlasterPlayerController && AttackerController)
	// 	{
	// 		BlasterGameMode->PlayerEliminated(this, BlasterPlayerController, AttackerController);
	// 	}
	// }

	/*
	 * 2023年6月1日14:46:05 教程https://www.bilibili.com/video/BV1Zr4y1G79Z/?p=90&spm_id_from=pageDriver&vd_source=fc0e4231b35a5cab738b18c593928bcd
	 * P89，当前存在bug，持有武器被射击，血量降为0所有客户端不会播放死亡动画，角色再次受击客户端才会播放死亡动画。
	 * 未持有武器时，血量降为0，服务器和所有客户端都能能正常播放死亡动画
	 * 备注：判断死亡改为Health <= 0.f ,否则可能出BUG
	 */
	if(Health == 0.f)
	{
		// 判断转换是否成功
		if(BlasterGameMode)
		{
			// 使用三元运算符校验，BlasterPlayerController为空时才进行Cast转换，否则使用旧的BlasterPlayerController进行赋值
			BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
			// 转换攻击者的控制器
			ABlasterPlayerController* AttackerController = Cast<ABlasterPlayerController>(InstigatorController);
			// 处理玩家的死亡流程
			BlasterGameMode->PlayerEliminated(this, BlasterPlayerController, AttackerController);
			UE_LOG(LogTemp, Warning, TEXT("执行角色死亡流程PlayerEliminated"));
		}
	}

	/*
	 * 原教程没有的功能：绘制受击伤害数值到攻击者的屏幕上
	 */
	// 确保是在服务器上
	if(!HasAuthority()) return;
	// 获取受击者
	// 随机模拟是否暴击
	bool bCriticalStrike = FMath::RandBool();
	// 根据攻击者的控制器获取攻击者的HUD
	ABlasterPlayerController* AttackerController = Cast<ABlasterPlayerController>(InstigatorController);
	if(AttackerController)
	{
		AttackerController->BroadcastDamageNumber(Damage, this, bCriticalStrike);
	}
}

void ABlasterCharacter::Elim(bool bPlayerLeftGame)
{
	/*
	 * bPlayerLeftGame: 死亡/销毁的该玩家是否是因为离开了游戏而被销毁
	 */
	// 处理角色身上的所有武器的掉落和销毁
	DropOrDestroyWeapons();
	// 多播RPC，通知所有客户端开始角色死亡动画的播放处理、暂停玩家操作、处理武器掉落等流程
	MulticastElim(bPlayerLeftGame);
}

void ABlasterCharacter::MulticastElim_Implementation(bool bPlayerLeftGame)
{
	// 更新该角色是否离开了游戏的状态记录。顺便在本多播内更新bLeftGame而不是将bLeftGame作为一个新的网络复制的变量，节省宽带开销？
	bLeftGame = bPlayerLeftGame;
	// 判断要操作的对象存在
	if(BlasterPlayerController)
	{
		// 将玩家弹匣子弹数量UI显示置为0
		BlasterPlayerController->SetHUDWeaponAmmo(0);
	}
	
	// 设置为角色死亡tag状态
	bElimmed = true;
	// 播放死亡动画
	PlayElimMontage();
	// UE_LOG(LogTemp, Warning, TEXT("MulticastElim_Implementation调用完毕！！！"));
	
	// 需要设置角色死亡时的材质为溶解材质，这里先判断要设置的材质是否存在
	if(DissolveMaterialInstance)
	{
		// 使用初始的溶解材质创建动态的溶解材质
		DynamicDissolveMaterialInstance = UMaterialInstanceDynamic::Create(DissolveMaterialInstance, this);
		// 设置网格体的材质为动态的溶解材质
		GetMesh()->SetMaterial(0, DynamicDissolveMaterialInstance);
		// 初始化该动态材质的参数
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Dissolve"), 0.55f);
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Glow"), 200.f);
	}
	// 开始处理溶解材质的参数随时间的变化
	StartDissolve();

	/*
	 * 禁止角色操作
	 */
	// 禁止角色移动
	GetCharacterMovement()->DisableMovement();
	// 禁止角色转动。（注意：是禁止角色转向，这跟玩家移动鼠标转向镜头没有关系）
	GetCharacterMovement()->StopMovementImmediately();
	// 判断控制器是否正常存在，禁用其输入
	// if(BlasterPlayerController)
	// {
	// 	DisableInput(BlasterPlayerController);
	// }
	// 禁用输入的业务逻辑优化为一个tag开关
	bDisableGameplay = true;
	// 玩家死亡碰撞被取消后，会掉落地板，需要禁止玩家角色的位移
	GetCharacterMovement()->DisableMovement();
	/*
	 * 停止角色的开火行为。因为角色死亡时，可能正处于左键自动开火状态
	 */
	if(Combat)
	{
		// 开火按键设置为没有按下。
		Combat->FireButtonPressed(false);
	}
	
	/*
	 * 取消角色碰撞
	 */
	// 取消胶囊体的碰撞
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 取消网格体的碰撞
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 取消手榴弹网格体的碰撞
	AttachedGrenade->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	/*
	 * 播放角色死亡的粒子效果。注意和角色材质的溶解效果做出区分
	 */
	// 先判断蓝图中设置的粒子效果是否存在
	if(ElimBotEffect)
	{
		// 实例化粒子效果的播放位置
		FVector ElimBotSpawnPoint(GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z + 200.f);
		// 生成粒子效果并存储
		ElimBotComponent = UGameplayStatics::SpawnEmitterAtLocation(
			// 在那个世界关卡生成
			GetWorld(),
			// 具体的粒子效果
			ElimBotEffect,
			// 生成位置
			ElimBotSpawnPoint,
			// 生成的朝向
			GetActorRotation()
		);
	}
	// 播放角色死亡时的声音
	// 先判断是否已经设置
	if(ElimBotSound)
	{
		// 在指定位置播放声音
		UGameplayStatics::SpawnSoundAtLocation(
			// 声源
			this,
			// 具体声音
			ElimBotSound,
			// 位置
			GetActorLocation());
	}
	/*
	 * 当角色开镜时死亡时，需要解除他的开镜行为。注意条件判断
	 * 1.开镜关镜都只发生在当前机器上的玩家，与其他玩家无关；
	 * 2.战斗组件存在；
	 * 3.已装备了武器；
	 * 4.装备的武器时狙击枪
	 * 5.当前正在瞄准(开镜)中
	 */
	bool bHideSniperScope = IsLocallyControlled() &&
			Combat &&
			Combat->bAiming &&
			Combat->EquippedWeapon &&
			Combat->EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SniperRifle;
	if(bHideSniperScope)
	{
		// 关闭狙击枪瞄准UI
		ShowSniperScopeWidget(false);
	}
	// 确保战斗组件存在
	if(Combat)
	{
		// 将玩家的摄像机视野恢复战斗组件里面记录的数值
		GetFollowCamera()->SetFieldOfView(Combat->DefaultFOV);
	}

	// 销毁该玩家的领先皇冠特效(如果有)
	if(CrownComponent)
	{
		CrownComponent->DestroyComponent();
	}
	
	/*
	 * 角色死亡特效生效等善后工作处理完毕，设置玩家死亡后的计时器，该计时器定时执行的函数里包含了将玩家角色重生等行为
	 * 从世界关卡定时器管理中设置定时器。
	 */ 
	GetWorldTimerManager().SetTimer(
		// 定时器，如果有传入该参数，则Set成功后，新的定时器会替换旧的
		ElimTimer,
		// 调用定时器的对象
		this,
		// 定时器要触发的函数的地址
		&ABlasterCharacter::ElimTimerFinished,
		// 定时器触发间隔
		ElimDelay
	);
}

void ABlasterCharacter::ElimTimerFinished()
{
	/*
	 * 注意：实际运行中，本函数会在所有机器上运行，但只有服务器机器可以获取游戏模式，其余机器只能得到空指针
	 */
	// 获取服务器当前的游戏模式并转换成ABlasterGameMode类型
	BlasterGameMode = BlasterGameMode == nullptr ? GetWorld()->GetAuthGameMode<ABlasterGameMode>() : BlasterGameMode;
	// 转换成功且该玩家并不是离开了游戏时才进行重生。
	if(BlasterGameMode && !bLeftGame)
	{
		// 销毁玩家角色尸体并重新生成。
		BlasterGameMode->RequestRespawn(this, Controller);
	}
	// 当该玩家离开了游戏时，本地控制的玩家角色才需要发送广播，模拟代理的玩家角色死亡则不需要
	if(bLeftGame && IsLocallyControlled())
	{
		// 调用该玩家的离开游戏的委托的实例进行广播，所有绑定了该委托实例的函数都被执行
		OnLeftGame.Broadcast();
	}
	// 销毁角色死亡时生成的粒子效果。本函数只在服务器上执行，因此只有服务器上的粒子效果能被销毁。
	// if(ElimBotComponent)
	// {
	// 	ElimBotComponent->DestroyComponent();
	// }
}

void ABlasterCharacter::ServerLeaveGame_Implementation()
{
	// 服务器收到客户端玩家的退出游戏的RPC请求后，在这里处理
	// 通过世界关卡获取游戏模式，并将其转换为ABLasterGamemode，存储在BlasterGameMode中
	BlasterGameMode = BlasterGameMode == nullptr ? GetWorld()->GetAuthGameMode<ABlasterGameMode>() : BlasterGameMode;
	// 获取玩家状态并转换成ABlasterPlayerState
	BlasterPlayerState = BlasterPlayerState == nullptr ? GetPlayerState<ABlasterPlayerState>() : BlasterPlayerState;
	// 成功获取游戏模式和玩家状态
	if(BlasterGameMode && BlasterPlayerState)
	{
		// 通过游戏模式，将该玩家退出游戏
		BlasterGameMode->PlayerLeftGame(BlasterPlayerState);
	}
}

// Called every frame
void ABlasterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// 处理角色动画原地转身的逻辑
	RotateInPlace(DeltaTime);
	// 角色背靠墙体时，隐藏角色和武器。疑惑：代码逻辑可能有误，造成枪械无法正确隐藏和显示，需要检查与教程代码是否一致。
	// 当前存在BUG：会导致本地机器的玩家自己的手上的武器不显示，需要再次检测代码是否与教程一致，以及跟随相机的设置和距离等属性
	HideCameraIfCharacterClose();

	PollInit();

	// 原教程没有的代码。检查玩家是否领先。如果玩家领先，则激活其皇冠特效.
	CheckPlayerLead();
}

void ABlasterCharacter::RotateInPlace(float DeltaTime)
{
	// 当玩家手持旗子时（旗子是一种特殊武器）
	if(Combat && Combat->bHoldingTheFlag)
	{
		// 将角色水平朝向随镜头移动设置为false
		bUseControllerRotationYaw = false;
		// 角色移动方式改为朝角色朝向移动
		GetCharacterMovement()->bOrientRotationToMovement = true;
		// 角色朝向的tag设置为不转动。
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		// 直接返回，不必再进行后续逻辑的处理
		return;
	}
	// 当玩家手持普通武器时
	if(Combat && Combat->EquippedWeapon) GetCharacterMovement()->bOrientRotationToMovement = false;
	if(Combat && Combat->EquippedWeapon) bUseControllerRotationYaw = true;
	
	// 当前玩家的游戏操作被禁用时
	if(bDisableGameplay)
	{
		// 将角色水平朝向随镜头移动设置为false
		bUseControllerRotationYaw = false;
		// 角色朝向的tag设置为不转动。
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		// 直接返回，不必再进行后续逻辑的处理
		return;
	}
	// 判断本地角色等级。枚举值之间可比较大小，网络角色ENetRole的枚举值为递增的整数。
	// 本地网络角色等级大于模拟代理(本地机器上的其他玩家角色时，说明本地网略角色为自主代理(本地机器玩家控制的)或者授权(服务器权威))且本地控制时
	if(GetLocalRole() > ENetRole::ROLE_SimulatedProxy && IsLocallyControlled())
	{
		AimOffset(DeltaTime);
	}
	else
	{
		// 否则说明本地网略角色等级为模拟代理，调用模拟代理转身函数处理
		// SimProxiesTurn();
		/*
		 * Tick函数每帧执行，角色移动的属性需要网络复制，网络复制隔一段时间才执行，如果在Tick里执行SimProxiesTurn，
		 * 则角度差永远也达不到转身的要求，因此优化为在角色移动的属性发生了网络复制更新后，
		 * 在角色移动属性的网络复制的回调函数OnRep_ReplicatedMovement中执行SimProxiesTurn
		*/
		// 每帧累计时间，超过一定值如0.25f，手动执行回调函数进行模拟代理的转身处理。角色移动属性被网络复制更新也会自动调用回调函数OnRep_ReplicatedMovement
		TimeSinceLastMovementReplication += DeltaTime;
		if(TimeSinceLastMovementReplication > 0.25f)
		{
			// UE_LOG(LogTemp, Warning, TEXT("定时调用OnRep_ReplicatedMovement"));
			OnRep_ReplicatedMovement();
		}
		
		// 持枪俯仰角的处理
		CalculateAO_Pitch();
	}
}

// Called to bind functionality to input
void ABlasterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	// 使用新的跳跃函数
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ABlasterCharacter::Jump);
	
	PlayerInputComponent->BindAxis("MoveForward", this, &ABlasterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ABlasterCharacter::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &ABlasterCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &ABlasterCharacter::LookUp);

	// 绑定玩家输入Equip事件，触发事件会执行函数EquipButtonPressed
	PlayerInputComponent->BindAction("Equip", IE_Pressed, this, &ABlasterCharacter::EquipButtonPressed);
	// 绑定下蹲事件
	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ABlasterCharacter::CrouchButtonPressed);
	// 绑定冲刺事件
	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &ABlasterCharacter::SprintButtonPressed);
	PlayerInputComponent->BindAction("Sprint", IE_Released, this, &ABlasterCharacter::SprintButtonReleased);
	// 绑定瞄准事件
	PlayerInputComponent->BindAction("Aiming", IE_Pressed, this, &ABlasterCharacter::AimButtonPressed);
	PlayerInputComponent->BindAction("Aiming", IE_Released, this, &ABlasterCharacter::AimButtonReleased);
	// 绑定开火事件
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ABlasterCharacter::FireButtonPressed);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &ABlasterCharacter::FireButtonReleased);
	// 绑定重新装弹事件
	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &ABlasterCharacter::ReloadButtonPressed);
	// 绑定投掷手榴弹事件
	PlayerInputComponent->BindAction("ThrowGrenade", IE_Pressed, this, &ABlasterCharacter::GrenadeButtonPressed);
}

void ABlasterCharacter::MoveForward(float Value)
{
	// 当玩家的游戏操作被禁用时，直接返回
	if(bDisableGameplay) return;
	
	if(Controller != nullptr && Value != 0.f)
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X));
		AddMovementInput(Direction, Value);
	}
}

void ABlasterCharacter::MoveRight(float Value)
{
	// 当玩家的游戏操作被禁用时，直接返回
	if(bDisableGameplay) return;
	
	if(Controller != nullptr && Value != 0.f)
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y));
		AddMovementInput(Direction, Value);
	}
}

void ABlasterCharacter::Turn(float Value)
{
	AddControllerYawInput(Value);
}

void ABlasterCharacter::LookUp(float Value)
{
	AddControllerPitchInput(Value);
}

void ABlasterCharacter::EquipButtonPressed()
{
	UE_LOG(LogTemp,Log,TEXT("按下拾取武器按键！！！"));
	// 当玩家的游戏操作被禁用时，直接返回
	if(bDisableGameplay) return;
	
	// 校验Combat
	if(Combat)
	{
		// 处于举着旗子的状态下，不允许进行本操作，直接返回。
		if(Combat && Combat->bHoldingTheFlag) return;
		
		// // 当前机器是服务器机器时（HasAuthority()返回true时），才能执行装备武器的逻辑
		// if(HasAuthority())
		// {
		// 	UE_LOG(LogTemp,Log,TEXT("本地Combat->EquipWeapon(OverlappingWeapon)！！！"));
		// 	// OverlappingWeapon的获取在检测碰撞显示武器拾取UI的相关代码处已经处理
		// 	Combat->EquipWeapon(OverlappingWeapon);
		// }
		// else
		// {
		// 	UE_LOG(LogTemp,Log,TEXT("发送RPC，ServerEquipButtonPressed！！！"));
		// 	// 当机器没有权限时，改为RPC调用。实际会调用远程服务器上的代码，而非本地机器的代码
		// 	ServerEquipButtonPressed();
		// }

		/*
		 * 优化：上面两种情况最终结果没有区别，RPC都是要在服务器执行，因此可以优化如下。
		 * 疑惑：该条件是否准确，此处没有return，代码依旧往下走，后期需要优化
		 */
		if(Combat->CombatState == ECombatState::ECS_Unoccupied) ServerEquipButtonPressed();
		
		/*
		 *  检查是否满足切换武器的条件。
		 *  补充条件：要求角色处于待机状态下。
		 *  注意该行为仅限客户端，
		 *  服务器的玩家的动画播放和状态设置在该逻辑的RPC的方法的Combat->SwapWeapons()中。
		 *  玩家角色当前没有与地上的无主武器有接触。否则拾取武器也会播放切换武器的动画
		 */
		// 条件整合
		bool bSwap = Combat->ShouldSwapWeapons() &&
			!HasAuthority() &&
				Combat->CombatState == ECombatState::ECS_Unoccupied &&
					OverlappingWeapon == nullptr;
		if(bSwap)
		{
			// 播放切换武器的动画。疑惑：手上没有武器的时候，拾取第一把武器也播放切换动画不会导致观感很奇怪吗？
			PlaySwapMontage();
			// 将战斗状态设置为切换武器中
			Combat->CombatState = ECombatState::ECS_SwappingWeapons;
			// 本地切换武器的动画已经开始播放，tag设置为false
			bFinishedSwapping = false;
		}
	}
	else
	{
		UE_LOG(LogTemp,Log,TEXT("Combat不存在！！！"));
	}
}

void ABlasterCharacter::ReloadButtonPressed()
{
	// 当玩家的游戏操作被禁用时，直接返回
	if(bDisableGameplay) return;
	if(Combat)
	{
		// 处于举着旗子的状态下，不允许进行本操作，直接返回。
		if(Combat && Combat->bHoldingTheFlag) return;
		Combat->Reload();
	}
}

void ABlasterCharacter::CrouchButtonPressed()
{
	// 当玩家的游戏操作被禁用时，直接返回
	if(bDisableGameplay) return;

	// 处于举着旗子的状态下，不允许进行本操作，直接返回。
	if(Combat && Combat->bHoldingTheFlag) return;
	
	if(bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		// Character类提供的方法，幕后会检查胶囊体缩放等操作，并且相关属性bIsCrouched会复制到客户端，且触发OnRep_bIsCrouched函数
		Crouch();
	}
}

void ABlasterCharacter::SprintButtonPressed()
{
	// 当玩家的游戏操作被禁用时，直接返回
	if(bDisableGameplay) return;

	// 处于举着旗子的状态下，不允许进行本操作，直接返回。
	if(Combat && Combat->bHoldingTheFlag) return;
	// 冲刺
	Sprint();
}

void ABlasterCharacter::SprintButtonReleased()
{
	// 当玩家的游戏操作被禁用时，直接返回
	if(bDisableGameplay) return;

	// 处于举着旗子的状态下，不允许进行本操作，直接返回。
	if(Combat && Combat->bHoldingTheFlag) return;
	// 停止冲刺
	UnSprint();
}

void ABlasterCharacter::Sprint()
{
	if(HasAuthority())
	{
		if(GetCharacterMovement())
        	{
        		GetCharacterMovement()->MaxWalkSpeed += SprintSpeedExtent;
        	}
	}
	else
	{
		ServerSprint(true);
	}
	
}

void ABlasterCharacter::UnSprint()
{
	if(HasAuthority())
	{
		if(GetCharacterMovement())
        	{
        		GetCharacterMovement()->MaxWalkSpeed -= SprintSpeedExtent;
        	}
	}
	else
	{
		ServerSprint(false);
	}
}

void ABlasterCharacter::ServerSprint_Implementation(bool bSprint)
{
	if(bSprint)
	{
		Sprint();
	}
	else
	{
		UnSprint();
	}
}

void ABlasterCharacter::AimButtonPressed()
{
	// 当玩家的游戏操作被禁用时，直接返回
	if(bDisableGameplay) return;
	if(Combat)
	{
		// 处于举着旗子的状态下，不允许进行本操作，直接返回。
		if(Combat && Combat->bHoldingTheFlag) return;
		UE_LOG(LogTemp,Log,TEXT("玩家角色开始瞄准！！！"));
		Combat->SetAiming(true);
	}
}

void ABlasterCharacter::AimButtonReleased()
{
	// 原教程中，当进入赛后休息阶段，禁用玩家的游戏操作后，连瞄准释放也禁用，疑惑：是否有这个必要，因为这可能会导致玩家右键开镜时比赛结束，玩家松开右键也无法恢复镜头视野。
	// 当玩家的游戏操作被禁用时，直接返回
	if(bDisableGameplay) return;
	if(Combat)
	{
		// 处于举着旗子的状态下，不允许进行本操作，直接返回。
		if(Combat && Combat->bHoldingTheFlag) return;
		Combat->SetAiming(false);
	}
}


float ABlasterCharacter::CalculateSpeed()
{
	FVector Velocity = GetVelocity();
	// 不需要z轴速度大小
	Velocity.Z = 0.f;
	return Velocity.Size();
}

void ABlasterCharacter::AimOffset(float DeltaTime)
{
	// if(Combat)
	// {
	// 	UE_LOG(LogTemp, Warning, TEXT("Combat组件正常！！！"));
	// }
	// else
	// {
	// 	UE_LOG(LogTemp, Warning, TEXT("缺少Combat！！！"));
	// }
	// if(Combat && Combat->EquippedWeapon == nullptr)
	// {
	// 	UE_LOG(LogTemp, Warning, TEXT("未装备武器时，直接返回！！！"));
	// }
	// else
	// {
	// 	UE_LOG(LogTemp, Warning, TEXT("设置bUseControllerRotationYaw = true;！！！"));
	// }
	
	// 未装备武器时，直接返回
	if(Combat && Combat->EquippedWeapon == nullptr) return;
	// 获取速度大小
	float Speed = CalculateSpeed();

	bool bIsInAir = GetCharacterMovement()->IsFalling();
	// 速度为0且不在空中时才执行
	if (Speed == 0.f && !bIsInAir)
	{
		// 角色没有在位移中且在地面时，设置为应用持枪转身动画
		bRotateRootBone = true;
		// 存储角色旋转
		FRotator CurrentAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw,0.f);
		// 计算角色当前旋转和上一次移动或空中时之间的差值
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation);
		// 设置瞄准偏移
		AO_Yaw = DeltaAimRotation.Yaw;

		// 处理角色转向需要的根运动
		// 当前没有转身时，取当当前AO_Yaw用于插值运算
		if(TurningInPlace == ETurningInPlace::ETIP_NotTurning)
		{
			InterpAO_Yaw = AO_Yaw;
		}
		
		// 开启控制器控制角色旋转
		bUseControllerRotationYaw = true;
		// 原地转身动画处理
		TurnInPlace(DeltaTime);
		
	}
	// 角色发生了移动或状态改变为了在空中时
	if(Speed > 0.f || bIsInAir)
	{
		// 角色位移中或在空中时，无需设置应用持枪转身动画
		bRotateRootBone = false;
		// 存储新的旋转
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		// 重置瞄准偏移
		AO_Yaw = 0.f;
		// 开启控制器控制角色旋转
		bUseControllerRotationYaw = true;

		// 移动情况下，不转动身体
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}
	// 以上两种情况，角色都要有俯仰的瞄准偏移
	CalculateAO_Pitch();
}

void ABlasterCharacter::CalculateAO_Pitch()
{
	AO_Pitch = GetBaseAimRotation().Pitch;
	
	/*
	 * 注意区分玩家本地机器和服务器本地机器
	 * 瞄准偏移预见的BUG笔记：本地机器和服务器通信时，互相发送的数据会被压缩，比如有符号的数值会被压缩成无符号的数值再传送。
	 * 因为AO_Pitch的范围在-90到+90之间，数据压缩导致了AO_Pitch在本地为负值时，压缩发送到服务器上再被解压恢复后是正值，
	 * 导致服务器上的角色瞄准偏移和本地不一致，因此服务器在执行本函数时，需要在下面对AO_Pitch进行修正.
	 * 根据他人所测经验，本地-90到0之间压缩发送再在服务器上解压后为270到360之间。
	 *
	 * 源码相关：
	 * CharacterMovementComponent.cpp
	 * FSavedMove_Character::GetPackedAngles(uint32& YawAndPitchPack, uint8& RollPack)处的UCharacterMovementComponent::PackYawAndPitchTo32 处理了角度数据的压缩发送
	 * CharacterMovementComponent.h
	 * UCharacterMovementComponent::PackYawAndPitchTo32(const float Yaw, const float Pitch) 里 FRotator::CompressAxisToShort(Pitch) 具体处理了数据压缩换换
	 */
	// 本地未控制该角色且AO_Pitch大于90时，说明当前代码执行是在服务器上，且经过压缩网络发送再解压后AO_Pitch已出错，此时需要修正AO_Pitch.
	if(AO_Pitch > 90.f && !IsLocallyControlled())
	{
		// 实例化要作为范围使用的FVector2d
		FVector2d InRange(270.f, 360.f);
		FVector2d OutRange(-90.f, 0.f);
		// AO_Pitch在InRange范围内时，映射成对应在OutRange上的值并返回。
		AO_Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AO_Pitch);
	}
}

void ABlasterCharacter::FireButtonPressed()
{
	// 当玩家的游戏操作被禁用时，直接返回
	if(bDisableGameplay) return;
	if(Combat)
	{
		// 处于举着旗子的状态下，不允许进行本操作，直接返回。
		if(Combat && Combat->bHoldingTheFlag) return;
		Combat->FireButtonPressed(true);
	}
}

void ABlasterCharacter::FireButtonReleased()
{
	// 原教程中，当进入赛后休息阶段，禁用玩家的游戏操作后，连开火释放也禁用，疑惑：是否有这个必要，因为这可能会导致玩家开火时比赛结束，玩家松开开火键也无法停止开火。
	// 当玩家的游戏操作被禁用时，直接返回
	if(bDisableGameplay) return;
	if(Combat)
	{
		// 处于举着旗子的状态下，不允许进行本操作，直接返回。
		if(Combat && Combat->bHoldingTheFlag) return;
		Combat->FireButtonPressed(false);
	}
}

void ABlasterCharacter::GrenadeButtonPressed()
{
	// 当玩家的游戏操作被禁用时，直接返回（原教程没有的代码，可能遗漏？）
	if(bDisableGameplay) return;
	// 客户端开始本地的投掷逻辑，先判断Combat的存在
	if(Combat)
	{
		// 处于举着旗子的状态下，不允许进行本操作，直接返回。
		if(Combat && Combat->bHoldingTheFlag) return;
		// 客户端本地开始投掷
		Combat->ThrowGrenade();
	}
}

// 这里函数ServerEquipButtonPressed定义的具体实施
void ABlasterCharacter::ServerEquipButtonPressed_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("收到客户端的RPC请求，EquipButtonPressed的RPC！"));
	if(Combat)
	{
		// 当前玩家角色与可拾取的武器碰撞覆盖时，执行逻辑为拾取武器
		if(OverlappingWeapon)
		{
			// UE_LOG(LogTemp,Log,TEXT("服务器调用ServerEquipButtonPressed_Implementation！！！"));
			Combat->EquipWeapon(OverlappingWeapon);
		}
		// 当前玩家角色已经拥有主副武器时，交换主副武器的位置
		else if(Combat->ShouldSwapWeapons())
		{
			Combat->SwapWeapons();
		}
		
	}
}

void ABlasterCharacter::OnRep_OverLappingWeapon(AWeapon* LastWeapon)
{
	// OverLappingWeapon的值被改变的时候，OnRep_OverLappingWeapon会被自动调用，且会传入OverLappingWeapon改变前的最后一次的值作为LastWeapon
	if(OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(true);
	}
	// 
	if(LastWeapon)
	{
		LastWeapon->ShowPickupWidget(false);
	}
}

void ABlasterCharacter::HideCameraIfCharacterClose()
{
	/*
	 * 当角色背靠墙体时，需要隐藏角色和武器。
	 * 该行为只需要在玩家角色的本地机器执行。模拟代理不会因为背靠墙体而隐藏自己，
	 */
	// 该角色非本地控制时，直接返回
	if(!IsLocallyControlled()) return;
	// if(FollowCamera && (FollowCamera->GetComponentLocation() - GetActorLocation()).Size() < CameraThreshold)
	if((FollowCamera->GetComponentLocation() - GetActorLocation()).Size() < CameraThreshold)
	{
		// 设置角色网格体不可见
		GetMesh()->SetVisibility(false);
		// 判断Combat对象以及武器和其网格体是否为空
		if(Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh())
		{
			// 设置当组件所有者不可见时，组件也不可见
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = true;
		}
		// 隐藏副武器
		if(Combat && Combat->SecondaryWeapon && Combat->SecondaryWeapon->GetWeaponMesh())
		{
			// 设置当组件所有者不可见时，组件也不可见
			Combat->SecondaryWeapon->GetWeaponMesh()->bOwnerNoSee = true;
		}
	}
	else
	{
		// 设置角色网格体可见
		GetMesh()->SetVisibility(true);
		// 判断Combat对象以及武器和其网格体是否为空
		if(Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh())
		{
			// 设置当组件所有者不可见时，组件也不可见
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = false;
		}
		// 显示副武器
		if(Combat && Combat->SecondaryWeapon && Combat->SecondaryWeapon->GetWeaponMesh())
		{
			// 设置当组件所有者不可见时，组件也不可见
			Combat->SecondaryWeapon->GetWeaponMesh()->bOwnerNoSee = false;
		}
	}
}

void ABlasterCharacter::OnRep_ReplicatedMovement()
{
	Super::OnRep_ReplicatedMovement();
	
	SimProxiesTurn();
	// 重置计时
	TimeSinceLastMovementReplication = 0.f;
}

void ABlasterCharacter::UpdateHUDAmmo()
{
	// 使用三元运算符校验，BlasterPlayerController为空指针时才Cast转换，否则使用旧的值赋值。类似单例模式的思想。
	BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
	if(BlasterPlayerController && Combat && Combat->EquippedWeapon)
	{
		// 设置角色弹药HUD信息。
		BlasterPlayerController->SetHUDCarriedAmmo(Combat->CarriedAmmo);
		BlasterPlayerController->SetHUDWeaponAmmo(Combat->EquippedWeapon->GetAmmo());
	}
}

void ABlasterCharacter::UpdateHUDHealth()
{
	// 使用三元运算符校验，BlasterPlayerController为空指针时才Cast转换，否则使用旧的值赋值。类似单例模式的思想。
	BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
	if(BlasterPlayerController)
	{
		// 设置角色血量信息。
		BlasterPlayerController->SetHUDHealth(Health, MaxHealth);
	}
}

void ABlasterCharacter::UpdateHUDShield()
{
	// 使用三元运算符校验，BlasterPlayerController为空指针时才Cast转换，否则使用旧的值赋值。类似单例模式的思想。
	BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
	if(BlasterPlayerController)
	{
		// 设置角色护盾值信息。
		BlasterPlayerController->SetHUDShield(Shield, MaxShield);
	}
}

void ABlasterCharacter::PollInit()
{
	// 为空指针时，才进行处理
	if (BlasterPlayerState == nullptr)
	{
		// 获取控制当前对象的玩家的玩家状态，并转换为ABlasterPlayerState
		BlasterPlayerState = GetPlayerState<ABlasterPlayerState>();
		// 转换成功
		if (BlasterPlayerState)
		{
			// 初始化玩家状态
			OnPlayerStateInitialized();
		}
		// 将原教程的代码重构为一个函数
		CheckPlayerLead();
		
		// 以下为原教程的代码
		/*
		 * 如果玩家领先，在保持玩家的领先特效
		 */
		// // 通过游戏上下文即this获取并转换游戏状态
		// ABlasterGameState* BlasterGameState = Cast<ABlasterGameState>(UGameplayStatics::GetGameState(this));
		// // 转换成功且玩家状态位于领先者的玩家状态数组中
		// if(BlasterGameState && BlasterGameState->TopScoringPlayers.Contains(BlasterPlayerState))
		// {
		// 	// 显示领先的皇冠特效
		// 	MulticastGainedTheLead();
		// }
	}
}

void ABlasterCharacter::CheckPlayerLead()
{
	/*
	 * 如果玩家领先，在保持玩家的领先特效
	 */
	// 通过游戏上下文即this获取并转换游戏状态
	BlasterGameState = BlasterGameState == nullptr ? Cast<ABlasterGameState>(UGameplayStatics::GetGameState(this)) : BlasterGameState;
	// 转换成功且玩家状态位于领先者的玩家状态数组中
	if(BlasterGameState && BlasterGameState->TopScoringPlayers.Contains(BlasterPlayerState))
	{
		// 显示领先的皇冠特效
		MulticastGainedTheLead();
	}
}

void ABlasterCharacter::OnRep_Health(float LastHealth)
{
	// 血量发生变化，刷新玩家血条信息
	UpdateHUDHealth();

	/*
	 * 根据血量变化情况执行不同逻辑。
	 * 血量比之前低时，才播放受击动画
	 */
	if(Health < LastHealth)
	{
		// 播放受击动画
		PlayHitReactMontage();
	}
	
}

void ABlasterCharacter::OnRep_Shield(float LastShield)
{
	// 护盾值发生网络更新，因此需要刷新玩家的护盾HUD
	UpdateHUDShield();
	
	/*
	 * 根据护盾值变化情况执行不同逻辑。
	 * 护盾值比之前低时，才播放受击动画
	 */
	if(Shield < LastShield)
	{
		// 播放受击动画
		PlayHitReactMontage();
	}
}

void ABlasterCharacter::UpdateDissolveMaterial(float DissolveValue)
{
	if(DynamicDissolveMaterialInstance)
	{
		// 改变动态材质的参数，使得材质表现效果发生变化。
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Dissolve"), DissolveValue);
	}
}

void ABlasterCharacter::StartDissolve()
{
	// 绑定更新材质的委托
	DissolveTrack.BindDynamic(this, &ABlasterCharacter::UpdateDissolveMaterial);
	// 确保要访问的内容地址不为空，否则可能导致游戏崩溃
	if(DissolveCurve && DissolveTimeline)
	{
		// 对曲线值插值处理
		DissolveTimeline->AddInterpFloat(DissolveCurve, DissolveTrack);
		// 播放时间线
		DissolveTimeline->Play();
	}
}

void ABlasterCharacter::SetOverlappedWeapon(AWeapon* Weapon)
{
	// 因为服务器本地上的内容不会被网络更新，因此服务器的OverlappingWeapon不会被网络复制为Weapon。
	// 服务器本地的玩家角色(如果有人在服务器上玩的话)离开武器碰撞检测球体时，OverlappingWeapon不会被设置为nullptr，因而不会触发OnRep_OverLappingWeapon从而取消显示UI
	// 所以这里选择先关闭显示UI。（如果是服务器的玩家角色，则UI在这里提前直接关闭了,达到关闭UI的目的。
	// 如果是客户端的玩家角色，后面有OverlappingWeapon = Weapon;导致OverlappingWeapon改变，即发生了网络复制，从而触发OnRep_OverLappingWeapon，进而取消显示UI
	if(OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(false);
	}
	
	OverlappingWeapon = Weapon;
	// 因为服务器本地上的内容不会被网络更新，因此服务器的OverlappingWeapon不会被网络复制为Weapon。
	// 这里IsLocallyControlled(),如果该Pawn是本地控制的（注意：本函数场景下本地即是服务器），则手动触发原本OnRep_OverLappingWeapon里该做的内容。从而开启显示UI
	if(IsLocallyControlled())
	{
		if(OverlappingWeapon)
		{
			OverlappingWeapon->ShowPickupWidget(true);
		}
	}
}

bool ABlasterCharacter::IsWeaponEquipped()
{
	return (Combat && Combat->EquippedWeapon);
}

bool ABlasterCharacter::IsAiming()
{
	return (Combat && Combat->bAiming);
}

AWeapon* ABlasterCharacter::GetEquippedWeapon()
{
	if(Combat == nullptr) return nullptr;
	return Combat->EquippedWeapon;
}

void ABlasterCharacter::TurnInPlace(float DeltaTime)
{
	// 右转身
	if(AO_Yaw > 90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Right;
	}
	// 左转身
	else if(AO_Yaw < -90.f){
		TurningInPlace = ETurningInPlace::ETIP_Left;
	}
	// 处理角色转向需要的根运动
	// 当前发生转身时，对InterpAO_Yaw进行插值运算
	if(TurningInPlace != ETurningInPlace::ETIP_NotTurning)
	{
		// 传入当前值、目标值、时间、插值速度
		InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaTime, 4.f);
		// 赋值给AO_Yaw，最终AO_Yaw被传递给动画实例并产生影响。
		AO_Yaw = InterpAO_Yaw;
		// 控制器(一般是玩家鼠标)，水平偏转的绝对值小于一定角度，则不用旋转角色
		if(FMath::Abs(AO_Yaw) < 15.f)
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
			// 同时更新角色水平朝向的存储
			StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		}
	}
}

void ABlasterCharacter::SimProxiesTurn()
{
	// 持枪动画转身的根骨骼运动处理与否只发生在装备了武器的情况下
	if(Combat == nullptr || Combat->EquippedWeapon == nullptr) return;
	// 设置为无需应用根骨骼转身动画
	bRotateRootBone = false;
	// 获取角色移速
	float Speed = CalculateSpeed();
	// UE_LOG(LogTemp, Warning, TEXT("当前速度Speed是：%f"), Speed);
	// 当角色处于移动时，设置为无需转身，直接return返回
	if(Speed > 0.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		return;
	}
	
	// 持枪航向角的处理
	// 将当前转向赋值为上一帧的转向
	ProxyRotatorLastFrame = ProxyRotation;
	// 获取并更新当前转向
	ProxyRotation = GetActorRotation();
	// 计算当前帧和上一帧角色朝转向之间的角度差(只计算Yaw上的)
	ProxyYaw = UKismetMathLibrary::NormalizedDeltaRotator(ProxyRotation, ProxyRotatorLastFrame).Yaw;
	// UE_LOG(LogTemp, Warning, TEXT("当前角度差ProxyYaw是：%f"), ProxyYaw);
	// 角度差大于一定角度时，模拟代理角色需要进行转身动画如何播放的处理
	if(FMath::Abs(ProxyYaw) > TurnThreshold)
	{
		// 右转身
		if(ProxyYaw > TurnThreshold)
		{
			TurningInPlace = ETurningInPlace::ETIP_Right;
		}
		// 左转身
		else if (ProxyYaw < -TurnThreshold)
		{
			TurningInPlace = ETurningInPlace::ETIP_Left;
		}
		// 无需转身
		else
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		}
		// 转身设置完毕，直接返回
		return;
	}
	// 角度差绝对值没有达到要求，设置为不转身。
	TurningInPlace = ETurningInPlace::ETIP_NotTurning;
}

void ABlasterCharacter::Jump()
{
	// 当玩家的游戏操作被禁用时，直接返回
	if(bDisableGameplay) return;
	// 处于举着旗子的状态下，不允许进行本操作，直接返回。
	if(Combat && Combat->bHoldingTheFlag) return;
	/*
	 * 为了让角色在蹲下时，按跳跃键，能取消蹲下姿势。使用本函数替代玩家输入中绑定的父类的Jump函数
	 */
	if(bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Super::Jump();
	}
}

void ABlasterCharacter::PlayFireMontage(bool bAiming)
{
	if(Combat == nullptr || Combat->EquippedWeapon == nullptr) return;
	// 从网格体获取动画实例
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if(AnimInstance && FireWeaponMontage)
	{
		AnimInstance->Montage_Play(FireWeaponMontage);
		FName SectionName;
		// 根据是否瞄准选择蒙太奇中不同开火动画的Section
		SectionName = bAiming ? FName("RifleAim") : FName("RifleHip");
		// 从蒙太奇动画的指定Section开始播放
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::PlayReloadMontage()
{
	// 判断当前是否持有武器
	if(Combat == nullptr && Combat->EquippedWeapon == nullptr) return;
	// 从网格体处获取动画实例
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	
	// 获取成功且动画资源已配置
	if(AnimInstance && ReloadMontage)
	{
		// 准备播放蒙太奇动画。后续需要选择跳转到对应的section
		AnimInstance->Montage_Play(ReloadMontage);
		// 实例化Fname
		FName SectionName;
		// 根据武器类型设置FName，因为不同武器类型的换弹动画不一样。疑惑：这一段是否可以使用策略模式进行重构
		switch (Combat->EquippedWeapon->GetWeaponType())
		{
		case EWeaponType::EWT_Assaulrifle:
			SectionName = FName("Rifle");
			break;
		case EWeaponType::EWT_RocketLauncher:
			SectionName = FName("RocketLauncher");
			break;
		case EWeaponType::EWT_Pistol:
			SectionName = FName("Pistol");
			break;
		case EWeaponType::EWT_SubmachineGun:
			// 当前没有冲锋枪的开火动画，暂用手枪的替代
			SectionName = FName("Pistol");
			break;
		case EWeaponType::EWT_Shotgun:
			// 当前没有霰弹枪的开火动画，暂用步枪的替代
			SectionName = FName("Shotgun");
			break;
		case EWeaponType::EWT_SniperRifle:
			// 当前没有狙击枪的开火动画，暂用步枪的替代
			SectionName = FName("SniperRifle");
			break;
		case EWeaponType::EWT_GrenadeLauncher:
			//
			SectionName = FName("Rifle");
			break;
		}
		// 跳转并播放对应section的动画
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::PlayElimMontage()
{
	// 从网格体获取动画实例
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	// 判断是否有效
	if(AnimInstance && ElimMontage)
	{
		// 该蒙太奇没有分Section，直接播放即可
		AnimInstance->Montage_Play(ElimMontage);
	}
	UE_LOG(LogTemp, Warning, TEXT("死亡动画播放完毕！！！"));
}

void ABlasterCharacter::PlayThrowGrenadeMontage()
{
	// 从网格体出获取动画实例
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	// 获取成功且投掷动画已设置
	if(AnimInstance && ThrowGrenadeMontage)
	{
		// 该蒙太奇没有分Section，直接从头到尾播放即可
		AnimInstance->Montage_Play(ThrowGrenadeMontage);
	}
}

void ABlasterCharacter::PlaySwapMontage()
{
	// 从网格体中获取动画实例
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	// 获取成功且要播放的动画已被设置
	if(AnimInstance && SwapMontage)
	{
		// 直接播放动画
		AnimInstance->Montage_Play(SwapMontage);
	}
}

void ABlasterCharacter::PlayHitReactMontage()
{
	// 因为目前只有持枪受击动画，因此如果不持枪情况下播放受击动画观感会不好。后期需要优化该问题。
	if(Combat == nullptr || Combat->EquippedWeapon == nullptr) return;
	// 获取角色动画实例
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	// 动画实例存在且受击动画存在
	if(AnimInstance && HitReactMontage)
	{
		// 播放受击动画
		AnimInstance->Montage_Play(HitReactMontage);
		FName SectionName("FromFront");
		// 从蒙太奇动画的指定Section开始播放
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}


void ABlasterCharacter::MulticastHit_Implementation()
{
	PlayHitReactMontage();
}

FVector ABlasterCharacter::GetHitTarget() const
{
	if(Combat == nullptr) return FVector();
	return Combat->HitTarget;
}

ECombatState ABlasterCharacter::GetCombatState() const
{
	// 为空时直接返回默认的最大值
	if(Combat == nullptr) return ECombatState::ECS_MAX;
	return Combat->CombatState;
}

bool ABlasterCharacter::IsHoldingTheFlag() const
{
	// 战斗组件不存在时直接返回即可
	if(Combat == nullptr) return false;
	return Combat->bHoldingTheFlag;
}

bool ABlasterCharacter::IsLocallyReloading()
{
	// 战斗组件为空时直接返回false
	if(Combat == nullptr) return false;
	//
	return Combat->bLocallyReloading;
}

void ABlasterCharacter::SetTeamColor(ETeam Team)
{
	// 玩家状态和原始材质存在时，否则直接返回
	if(BlasterPlayerState == nullptr && OriginalMaterial == nullptr) return;
	// if(BlueDissolveMatInst == nullptr || RedDissolveMatInst == nullptr) return;
	// 根据玩家的所属队伍，执行不同行为
	switch (Team)
	{
		// 无队伍时
	case ETeam::ET_NoTeam:
		// 无队伍时，材质设置为原始材质。当前该Mesh只有一种材质，因此索引号为0
		GetMesh()->SetMaterial(0, OriginalMaterial);
		// 当前的处理方式：无队伍时，也使用蓝队的溶解特效
		DissolveMaterialInstance = BlueDissolveMatInst;
		// UE_LOG(LogTemp, Warning, TEXT("玩家 %d 队伍颜色为默认颜色！！"), BlasterPlayerState->GetPlayerId());
		break;
	// 蓝队
	case ETeam::ET_BlueTeam:
		// 设置材质为蓝队材质。当前该Mesh只有一种材质，因此索引号为0
		GetMesh()->SetMaterial(0, BlueMaterial);
		// 使用蓝队的溶解特效
		DissolveMaterialInstance = BlueDissolveMatInst;
		// UE_LOG(LogTemp, Warning, TEXT("玩家 %d 队伍颜色为蓝色颜色！！"), BlasterPlayerState->GetPlayerId());
		break;
	// 红队
	case ETeam::ET_RedTeam:
		// 设置材质为红队材质。当前该Mesh只有一种材质，因此索引号为0
		GetMesh()->SetMaterial(0, RedMaterial);
		// 使用红队的溶解特效
		DissolveMaterialInstance = RedDissolveMatInst;
		// UE_LOG(LogTemp, Warning, TEXT("玩家 %d 队伍颜色为红色颜色！！"), BlasterPlayerState->GetPlayerId());
		break;
	}
}

ETeam ABlasterCharacter::GetTeam()
{
	// 获取并转换玩家状态
	BlasterPlayerState = BlasterPlayerState ? GetPlayerState<ABlasterPlayerState>() : BlasterPlayerState;
	// 转换失败，直接返回无队伍
	if(BlasterPlayerState == nullptr) return ETeam::ET_NoTeam;
	// 否则返回存储在玩家状态中的队伍属性
	return BlasterPlayerState->GetTeam();
}

void ABlasterCharacter::SetHoldingTheFlag(bool bHolding)
{
	// 战斗状态为空时，直接返回
	if(Combat == nullptr) return;
	// 否则，设置战斗组件中的举旗tagh
	Combat->bHoldingTheFlag = bHolding;
}




