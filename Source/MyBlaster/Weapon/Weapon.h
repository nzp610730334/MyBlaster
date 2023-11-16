// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Casing.h"
#include "WeaponTypes.h"
#include "GameFramework/Actor.h"
#include "MyBlaster/Character/BlasterCharacter.h"
#include "Weapon.generated.h"

// 声明武器状态枚举类
UENUM(BlueprintType)
enum class EWeaponState: uint8
{
	// 初始状态，无主无归属状态
	EWS_Initial UMETA(DisplayName = "Initial State"),
	// 被装备时
	EWS_Equipped UMETA(DisplayName = "Equipped"),
	// 被切换(装备)为f副武器时
	EWS_EquippedSecondary UMETA(DisplayName = "Equipped Secondary"),
	// 掉落到地面上时
	EWS_Dropped UMETA(DisplayName = "Dropped"),
	// 默认枚举值
	EWS_MAX UMETA(DisplayName = "DefaultMAX")
};

UENUM(BlueprintType)
enum class EFireType : uint8
{
	// 射线开火类型
	EFT_HitScan UMETA(DisplayName = "Hit Scan Weapon"),
	// 实体弹丸类型
	EFT_Projectile UMETA(DisplayName = "Projectile Weapon"),
	// 霰弹枪类型(复数射线开火类型)
	EFT_Shotgun UMETA(DisplayName = "Shotgun Weapon"),
	// 默认最大值
	EFT_MAX UMETA(DisplayName = "DefaultMax")
};

// 武器开火镜头抖动相关
USTRUCT(BlueprintType)
struct FOnFireShake
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float ShakeStart = -0.0f;
	UPROPERTY(EditAnywhere)
	float ShakeEnd = -ShakeStart;
	UPROPERTY(EditAnywhere)
	float ShakeInterpSpeed = 1.f;
};

UCLASS()
class MYBLASTER_API AWeapon : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AWeapon();
	virtual void Tick(float DeltaTime) override;
	// 武器所有者网络复制时执行的回调函数。重写父类的方法
	virtual void OnRep_Owner() override;
	// 设置弹匣子弹数量UI
	void SetHUDAmmo();

	// 是否显示拾取武器的UI
	void ShowPickupWidget(bool bShowWidget);
	// 注册要进行网络复制的属性和变量
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 开火，播放开火动画、生成弹壳、扣除弹匣子弹数量 (需要传入要命中的点，引用的方式是为了节省内存)
	virtual void Fire(const FVector& HitTarget);

	/* 准星纹理，放在public通常不会有什么危险。方便其他地方访问它们。也可以放在private，但为每个提供get方法。
	 * Texture for the weapon crosshairs.
	 * 一般射击武器的准星纹理。一个准星UI通常由五部分组成，中间的点和上下左右的横杠
	 */
	UPROPERTY(EditAnywhere, Category = "Crosshairs")
	class UTexture2D* CrosshairsCenter;

	UPROPERTY(EditAnywhere, Category = "Crosshairs")
	UTexture2D* CrosshairsLeft;
	
	UPROPERTY(EditAnywhere, Category = "Crosshairs")
	UTexture2D* CrosshairsRight;

	UPROPERTY(EditAnywhere, Category = "Crosshairs")
	UTexture2D* CrosshairsTop;

	UPROPERTY(EditAnywhere, Category = "Crosshairs")
	UTexture2D* CrosshairsBottom;

	/*
	 * Zoomed FOV while aiming
	 */
	// 枪械开镜视野大小(变焦)
	UPROPERTY(EditAnywhere)
	float ZoomedFOV = 30.f;
	// 开镜速度
	UPROPERTY(EditAnywhere)
	float ZoomInterpSpeed = 20.f;

	// 不同武器类型开火间隔不同，因此需要暴露给蓝图
	UPROPERTY(EditAnywhere, Category = Combat)
	float FireDelay = 0.15f;
	// 武器的开火类型(是否全自动)
	UPROPERTY(EditAnywhere, Category = Combat)
	bool bAutoMatic = true;

	// 装备武器的声音
	UPROPERTY(EditAnywhere)
	class USoundCue* EquipSound;

	/*
	 * Enable or disable custom depth	启用或禁用自定义深度
	 */
	void EnableCustomDepth(bool bEnable);

	// 武器的子弹开火类型(射线、实体弹丸、复数射线)
	UPROPERTY(EditAnywhere)
	EFireType FireType;

	// 该tag的值表示是否开启弹丸的随机散布
	UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
	bool bUseScatter = false;
	
	// 计算弹丸随机落点的方法。传入瞄准的目标位置
	FVector TraceEndWithScatter(const FVector& HitTarget);
	
	// 武器的持有者
	UPROPERTY()
	ABlasterCharacter* BlasterOwnerCharacter;
	// 武器的所有者的控制器
	UPROPERTY()
	ABlasterPlayerController* BlasterOwnerController;

	// 武器开火抖动相关
	UPROPERTY(EditAnywhere, Category = FireShake)
	FOnFireShake OnFireShake;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 根据武器状态的不同执行不同功能
	virtual void OnWeaponState();
	// 武器变为装备状态时执行的函数
	virtual void OnEquipped();
	// 武器被设置为副武器时(武器被放置到后背背包时)
	virtual void OnEquippedSecondary();
	// 武器变为掉落状态时执行的函数
	virtual void OnDropped();

	// 碰撞检测球体绑定的开始重叠的函数。
	UFUNCTION()
	virtual void OnSphereOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	// 碰撞检测球体绑定的重叠结束的函数。
	UFUNCTION()
	void OnSphereEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex
		);
	
	/*
	 * Trace end with scatter	带有散布的弹丸落点
	 */
	// 枪口距离散布处理的球体的距离
	UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
	float DistanceToSphere = 800.f;
	// 散布处理的球体的半径
	UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
	float SphereRadius = 75.f;
	
	// 本武器的子弹伤害。需要暴露给蓝图，在蓝图中可设置子弹伤害。射线武器没有实体子弹，射线武器的伤害需要存储在武器上。(实弹武器的伤害存储在子弹类上。)
	UPROPERTY(EditAnywhere)
	float Damage = 20.f;

	// 爆头时的伤害
	UPROPERTY(EditAnywhere)
	float HeadShotDamage = 40.f;

	// 原教程没有设置武器的范围伤害，导致火箭炮 
	// 范围伤害内圈大小,就是正常伤害
	UPROPERTY(EditAnywhere)
	float DamageInnerRadius = Damage;
	// 范围伤害外圈圈大小,硬编码写死为正常伤害的三分之一
	UPROPERTY(EditAnywhere)
	float DamageOuterRadius = Damage / 3;

	// 该武器的命中判定是否应用服务器倒带处理
	UPROPERTY(Replicated, EditAnywhere)
	bool bUseServerSideRewind = false;

private:
	UPROPERTY(VisibleAnywhere, Category = "Weapon Properties")
	USkeletalMeshComponent* WeaponMesh;

	UPROPERTY(VisibleAnywhere, Category = "Weapon Properties")
	class USphereComponent* AreaSphere;

	// GetLifetimeReplicatedProps()里注册了WeaponState会被网络更新
	// 声明武器状态枚举，并暴露给蓝图。配置ReplicatedUsing，当WeaponState的值被网络更新时，触发OnRep_WeaponState函数的调用
	UPROPERTY(ReplicatedUsing = OnRep_WeaponState, VisibleAnywhere, Category = "Weapon Properties")
	EWeaponState WeaponState;

	UFUNCTION()
	void OnRep_WeaponState();

	// 武器拾取图标
	UPROPERTY(VisibleAnywhere, Category = "Weapon Properties")
	class UWidgetComponent* PickupWidget;

	// 动画资源
	UPROPERTY(EditAnywhere, Category = "Weapon Properties")
	class UAnimationAsset* FireAnimation;

	// 弹壳类，它作为Subclass，到时候开火后需要被Spwan生成
	UPROPERTY(EditAnywhere)
	TSubclassOf<class ACasing> CasingClass;

	// 弹匣子弹数量，武器初始化时，弹匣中的弹药数
	// UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Ammo)
	// 优化为使用客户端预测，不再通过网络复制服务器和客户端之间Ammo的同步
	UPROPERTY(EditAnywhere)
	int32 Ammo;

	// 服务器通知客户端更新弹药的RPC
	UFUNCTION(Client, Reliable)
	void ClientUpdateAmmo(int32 ServerAmmo);

	// 服务器通知客户端添加弹药的RPC
	UFUNCTION(Client, Reliable)
	void ClientAddAmmo(int32 AmmoToAdd);
	
	// 弹匣子弹数量被网络复制时执行的函数。注意：像弹匣子弹数量此类重要属性都只在服务器上进行修改，然后通过网络复制更新到每个玩家的客户端上，然后在回调函数OnRep中刷新UI
	UFUNCTION()
	void OnRep_Ammo();

	// 开火时扣除子弹数量
	void SpendRound();

	// 弹匣最大弹药数
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_MagCapacity)
	int32 MagCapacity;

	/*
	 * The number of unprocessed server requests for Ammo.
	 * 未处理的服务器对弹药的请求数。
	 * Incremented in SpendRound, decremented in ClientUpdateAmmo.
	 * 序列数，在函数SpendRound中被增加，在ClientUpdateAmmo函数中被减少。
	 */
	int32 Sequence = 0;
	
	// 弹匣最大弹药数网络复制时执行的函数
	UFUNCTION()
	void OnRep_MagCapacity();

	// 武器类型的枚举
	UPROPERTY(EditAnywhere)
	EWeaponType WeaponType;
	
	// 该函数将在武器被装备时，绑定到玩家角色的控制器的委托HighPingDelegate上
	UFUNCTION()
	void OnPingTooHigh(bool bPingTooHigh);

	// 武器(旗子)所属的队伍。当前仅有旗子会用到该属性
	UPROPERTY(EditAnywhere)
	ETeam Team;

public:
	// FORCEINLINE 标记函数为内联，存放一份在代码区中，频繁调用时不必重复复制代码到内存？
	void SetWeaponState(EWeaponState State);
	// 获取碰撞检测球体
	FORCEINLINE USphereComponent* GetAreaSphere() const { return AreaSphere; }
	// 获取武器的网格体
	FORCEINLINE USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; };
	// 获取拾取提示组件
	FORCEINLINE UWidgetComponent* GetPickupWidget() const { return PickupWidget; }
	// 获取武器的开镜视野大小
	FORCEINLINE float GetZoomedFOV() const { return ZoomedFOV; }
	// 获取武器的开镜速度大小
	FORCEINLINE float GetZoomInterpSpeed() const { return ZoomInterpSpeed; }
	// 校验弹匣数量是否为空
	bool IsEmpty();
	// 检查弹匣弹药数量是否已满
	bool IsFull();
	// 获取武器类型的枚举
	FORCEINLINE EWeaponType GetWeaponType() const { return WeaponType; };
	// 获取武器的弹匣当前子弹数量
	FORCEINLINE int32 GetAmmo() const { return Ammo; };
	// 获取武器的弹匣最大子弹数量
	FORCEINLINE int32 GetMagCapacity() const { return MagCapacity; };
	// 获取武器伤害
	FORCEINLINE float GetDamage() const { return Damage; }
	// 获取武器的爆头伤害
	FORCEINLINE float GetHeadShotDamage() const { return HeadShotDamage; };
	// 获取武器所属队伍。通常用于在武器是旗子时，本函数用于返回旗子所属队伍
	FORCEINLINE ETeam GetTeam() const { return Team; }
	
	// 丢弃武器。使用virtual修饰让子类能重写该函数
	virtual void Dropped();
	// 装填武器弹匣子弹
	void AddAmmo(int32 AmmoToAdd);
	
	/*
	 * 该武器是否应该被销毁。该tag当前用于开局武器上，因为角色死亡手上的武器会掉落，普通武器掉落是正常的，一个世界关卡中武器就那么多，
	 * 但角色每次重生时都携带一把开局武器，那么随着角色越来越多次的死亡和重生，世界关卡中的开局武器会越来越多，占用机器性能开销。
	 * 因此开局武器在spawn时，该tag设置为true，角色死亡武器掉落时，判断武器的该tag，为true时直接销毁武器而非掉落。
	 */
	bool bDestroyWeapon = false;

	// 开火抖动的相机镜头参数。不同武器配置不同的相机镜头参数蓝图
	UPROPERTY(EditAnywhere, Category = FireShake)
	TSubclassOf<UCameraShakeBase> BlasterMatineeCameraShake;
	// 是否开启开火时相机镜头的抖动
	UPROPERTY(EditAnywhere, Category = FireShake)
	bool bFireCameraShake = true;
	// 是否开启开火时强制玩家准星上飘(模拟后坐力导致准星上移)
	UPROPERTY(EditAnywhere, Category = FireShake)
	bool bFireAddPitchInput = false;
};
