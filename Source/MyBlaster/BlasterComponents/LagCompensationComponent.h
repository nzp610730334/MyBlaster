// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MyBlaster/Weapon/Weapon.h"
#include "LagCompensationComponent.generated.h"

/*
 * 本类是延迟补偿组件。
 */

// 声明一个结构体，用于存储角色模型的某个碰撞箱子的具体信息
USTRUCT(BlueprintType)
struct FBoxInformation
{
	GENERATED_BODY()

	// 箱子的位置
	UPROPERTY()
	FVector Location;
	// 箱子的朝向
	UPROPERTY()
	FRotator Rotation;
	// 箱子的范围
	UPROPERTY()
	FVector BoxExtent;
};

// 声明一个结构体，用于存储一个角色模型的一帧的用于延迟补偿计算需要的信息
USTRUCT(BlueprintType)
struct FFramePackage
{
	GENERATED_BODY()

	// 这些信息产生的游戏时间
	UPROPERTY()
	float Time;

	// 声明一个映射map，单独记录角色模型每个碰撞箱子的具体信息
	TMap<FName, FBoxInformation> HitBoxInfo;

	// 受击角色
	UPROPERTY()
	ABlasterCharacter* Character;
};

// 命中结果的结构体
USTRUCT(BlueprintType)
struct FServerSideRewindResult
{
	GENERATED_BODY()
	// 是否命中
	UPROPERTY()
	bool bHitCofirmed;
	// 是否命中头部碰撞盒子
	UPROPERTY()
	bool bHeadShot;
};

// 专门用于霰弹枪的服务器倒带使用的命中结果的结构体
USTRUCT()
struct FShotgunServerSideRewindResult
{
	GENERATED_BODY()
	// 每个受击角色对应的头部受击次数
	UPROPERTY()
	TMap<ABlasterCharacter*, uint32> HeadShots;
	// 每个受击角色对应的身体受击次数
	UPROPERTY()
	TMap<ABlasterCharacter*, uint32> BodyShots;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MYBLASTER_API ULagCompensationComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	ULagCompensationComponent();
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 玩家角色类需要直接访问延迟补偿组件类，因此设置其为本类的友元
	friend class ABlasterCharacter;

	// 绘制帧信息用于调试。连颜色参数都使用引用以节省性能开销
	void ShowFramePackage(const FFramePackage& Package, const FColor& Color);

	// 服务器端的倒带算法。
	FServerSideRewindResult ServerSideRewind(
		// 命中的角色
		class ABlasterCharacter* HitCharacter,
		// 命中的射线的起始点
		const FVector_NetQuantize& TraceStart,
		// 命中的位置
		const FVector_NetQuantize& HitLocation,
		// 发生命中搞得时间
		float HitTime
	);

	// 专门用于霰弹枪命中判定使用的服务器倒带函数
	FShotgunServerSideRewindResult ShotgunServerSideRewind(
		// 该次射击所有受击角色
		const TArray<ABlasterCharacter*>& HitCharacters,
		// 枪口位置
		const FVector_NetQuantize& TraceStart,
		// 所有受击角色命中的点。疑惑：为什么数组里面不使用引用，仅在数组外面对数组的名字进行引用
		const TArray<FVector_NetQuantize>& HitLocations,
		// 命中时间
		float HitTime
		);

	// 投射物(实体弹丸)发射器命中判定使用搞得服务器倒带函数
	FServerSideRewindResult ProjectileServerSideRewind(
		// 命中的角色
		ABlasterCharacter* HitCharacter,
		// 命中的射线的起始点
		const FVector_NetQuantize& TraceStart,
		// 实体弹丸的矢量速度(大小和方向)
		const FVector_NetQuantize100& InitialVelocity,
		// 命中发生的时间
		float HitTime
	);

	// 客户端向服务器发送RPC请求，告知服务器本客户端命中了其他玩家。这是射线命中判定使用的服务器倒带函数
	UFUNCTION(Server, Reliable)
	void ServerScoreRequest(
		// 受击角色
		ABlasterCharacter* HitCharacter,
		// 开火的玩家的枪口(弹道起始点)
		const FVector_NetQuantize& TraceStart,
		// 命中的位置
		const FVector_NetQuantize& HitLocation,
		// 命中时间
		float HitTime
		// 造成伤害的原因(哪种武器)，不传伤害是为了防止客户端作弊，传很大的伤害值给服务器。疑惑：客户端传其它武器的伤害值怎么防止？服务器依靠服务器上的该玩家的持有武器情况进行判断？
		// AWeapon* DamageCauser	// P203 原教程修改为 不再向服务器传递造成伤害的武器，而是命中判定时读取攻击者当前的武器。疑惑：玩家射击后马上切换别的武器，切换武器RPC在命中判定RPC之前到达，那么伤害还正确吗？
		);

	// 客户端向服务器发送RPC请求，告知服务器本客户端的霰弹枪命中了其他玩家。这是射线(霰弹枪使用的也是射线)命中判定使用的服务器倒带函数
	UFUNCTION(Server, Reliable)
	void ShotgunServerScoreRequest(
		// 该次射击所有受击角色
		const TArray<ABlasterCharacter*>& HitCharacters,
		// 枪口位置
		const FVector_NetQuantize& TraceStart,
		// 所有受击角色命中的点。疑惑：为什么数组里面不使用引用，仅在数组外面对数组的名字进行引用
		const TArray<FVector_NetQuantize>& HitLocations,
		// 命中时间
		float HitTime
		);

	/*
	 * 客户端向服务器发送RPC请求，告知服务器本客户端的投射物发射器(实体弹道武器)命中了其他玩家。
	 * 本方法没有传参造成伤害的原因-实体弹道武器，服务器将从本组件的Character中获取Weapon作为受击角色的受伤原因。
	 * 疑惑：对于一些投射物速度慢的武器而言，如果玩家在射出子弹后瞬间切换武器，导致投射物命中后伤害不是原本的武器的伤害，这种情况如何解决？
	 */ 
	UFUNCTION(Server, Reliable)
	void ProjectileServerScoreRequest(
		ABlasterCharacter* HitCharacter,
		const FVector_NetQuantize& TraceStart,
		const FVector_NetQuantize100& InitialVelocity,
		float HitTime
		);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	// 传入角色帧信息的结构体的引用，将拥有本组件的角色的当前所有碰撞盒子组件的信息存入该结构体中
	void SaveFramePackage(FFramePackage& Package);
	// 传入命中时间，在较新和较旧的历史帧信息之间插值混合，获取命中时的帧信息
	FFramePackage InterpBetweenFrames(const FFramePackage& OlderFrame, const FFramePackage& YoungerFrame, float HitTime);

	/*
	 * 服务器收到客户端的命中确认后，根据命中时间获取受击角色的在命中时间前后的俩帧历史帧帧信息，根据这俩镇插值获取命中时间那一刻受击角色的帧信息，
	 * 服务器缓存受击角色的当前位置的帧信息，然后将受击角色移回插值结果的受击那一刻的位置，处理命中后，再将受击角色移回当前位置。这一切发生在一帧内，客户端上的玩家不会察觉体验基本不受影响。
	 */
	// 服务器上的命中判定确认
	FServerSideRewindResult ConfirmHit(
		// 插值后的受击角色的历史帧的帧信息
		const FFramePackage& Package,
		// 受击角色
		ABlasterCharacter* HitCharacter,
		// 射击起始点
		const FVector_NetQuantize& TraceStart,
		// 射击命中点
		const FVector_NetQuantize& HitLocation
		);

	/*
	 * 霰弹枪
	 */
	// 专用于霰弹枪命中判定的函数
	FShotgunServerSideRewindResult ShotgunConfirmHit(
		// 用于命中判定的所有历史帧
		const TArray<FFramePackage>& FramePackages,
		// 枪口位置
		const FVector_NetQuantize& TraceStart,
		// 所有命中点
		const TArray<FVector_NetQuantize>& HitLocations
	);
	
	/*
	 * 投射物发射器(有弹道实体的枪械)的命中判定函数
	 */
	FServerSideRewindResult ProjectileConfirmHit(
		// 插值后的受击角色的历史帧的帧信息
		const FFramePackage& Package,
		// 受击角色
		ABlasterCharacter* HitCharacter,
		// 射击起始点
		const FVector_NetQuantize& TraceStart,
		// 实体弹丸的速度(矢量，包括方向和大小)
		const FVector_NetQuantize100& InitialVelocity,
		// 命中时间
		float HitTime
	);
	
	// 缓存受击角色的当前碰撞盒子的帧信息
	void CacheBoxPosition(ABlasterCharacter* HitCharacter, FFramePackage& OutFramePackage);
	// 移动受击角色到命中判定的位置上
	void MoveBoxes(ABlasterCharacter* HitCharacter, const FFramePackage& Package);
	// 重置受击角色的碰撞盒子到缓存的位置上，并将关闭碰撞盒子的碰撞检测
	void ResetHitBoxes(ABlasterCharacter* HitCharacter, const FFramePackage& Package);
	/*
	 * 设置玩家角色(受击角色)的碰撞类型。用于临时关闭受击角色的网格体的碰撞。
	 * 避免在命中判定时，受击角色的当前网格体的位置阻塞了服务器生成的命中射线，导致不能正确检测历史帧的碰撞盒子的位置是否命中。
	 * 疑惑：为什么不自定义一个命中检测通道作为命中射线的检测通道？
	 */
	void EnableCharacterMeshCollision(ABlasterCharacter* HitCharacter, ECollisionEnabled::Type CollisionEnabled);

	/*
	 * 整合了玩家角色的历史帧的缓存等行为的函数。注意：存在与本函数同名但函数签名不同的函数(函数重载)
	 * 疑惑：为什么不直接使用新的名字？和已存在的函数同名且却不是为了进行函数重载，这不利于代码的阅读。
	 */ 
	void SaveFramePakcage();

	// 根据受击角色和受击时间，从该角色的历史帧信息中获取用于服务器倒带进行命中判定的帧信息(判定依据)
	FFramePackage GetFrameToCheck(ABlasterCharacter* HitCharacter, float HitTime);
	
	
private:
	// 本对象的玩家角色对象
	UPROPERTY()
	class ABlasterCharacter* Character;
	// 本对象的玩家控制器
	UPROPERTY()
	class ABlasterPlayerController* Controller;

	/*
	 * 双向链表，用于存储最近一段时间的帧信息。
	 * 业务需求：最近一段时间的帧信息，会不断添加新的帧信息，存储容量达到一定的长度后，再添加新的帧信息需要删除最旧的帧信息。
	 * 数组难以满足需求，因为添加删除的过程中需要不断移动数组的每一个元素，或者使用一个不定长度的数组，性能不好。
	 * 使用双向链表能满足需求，链表有头尾指针，添加新的帧信息，将头指针指向该帧信息的内存地址，将尾指针指向倒数第二的帧信息的内存地址即可。
	 */
	// 双向链表，用于存储最近一段时间的帧信息。
	TDoubleLinkedList<FFramePackage> FrameHistory;
	/*
	 * 双向链表的长度。通常只存最近几百毫秒的帧信息。这里为了演示效果，存4秒。
	 * 太久远的帧信息没有意义，因为使用太久远的帧信息作为判定，会极大影响延迟低网络好的玩家的体验。
	 * EditAnywhere暴露给蓝图便于修改
	 */
	UPROPERTY(EditAnywhere)
	float MaxRecordTime = 4.f;
	
};
