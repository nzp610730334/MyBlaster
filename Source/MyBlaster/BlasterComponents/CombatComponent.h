// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MyBlaster/BlasterCamera/BlasterMovingMatineeCameraShake.h"
#include "MyBlaster/BlasterTypes/CombatState.h"
#include "MyBlaster/HUD/BlasterHUD.h"
#include "MyBlaster/Weapon/WeaponTypes.h"
#include "CombatComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MYBLASTER_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UCombatComponent();
	// 设置友元，玩家角色ABlasterCharacter需要能访问战斗组件UCombatComponent的所有内容
	friend class ABlasterCharacter;
	friend class AAIBlasterCharacter;
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 拾取并装备武器
	void EquipWeapon(class AWeapon* WeaponToEquip);
	// 交换主副武器
	void SwapWeapons();
	
	// 重新装弹
	void Reload();
	// 开火(当前只有全自动开火模式，按下按键一直开火，直到松开)
	void Fire();
	// 开火类型为实体弹丸的武器
	void FireProjectileWeapon();
	// 开火类型为射线的武器
	void FireHitScanWeapon();
	// 开火类型为复数射线的武器(霰弹枪武器)
	void FireShotgun();
	
	// 本地机器处理开火视效声效等内容。FVector_NetQuantize为优化过的用于网络传输的向量。疑惑：为什么使用引用而非指针
	void LocalFire(const FVector_NetQuantize& TraceHitTarget);

	// 本地机器处理复数射线武器(霰弹枪)的开火视效声效等内容
	void ShotgunLocalFire(const TArray<FVector_NetQuantize>& TraceHitTargets);
	
	// 开火按键是否有按下
	UFUNCTION(BlueprintCallable)
	void FireButtonPressed(bool bPressed);

	/*
	 * 开火RPC。（FVector_NetQuantize是一种经过了优化的Fvector，有利于网络传送且不损失精度）
	 * WithValidation	使用了该标记server的RPC函数，服务器收到客户端的RPC请求后，在执行前，会调用 函数名_Valdation ，在里面编写验证手段。
	 * 验证函数 函数名_Valdation ，无需额外声明，只需实现。这里对FireDelay进行验证作为示例。则客户端发送的RPC请求中需要带有该参数
	 * 疑惑：是否可以通过传FireDelay的引用，在服务器上纠正之后发送RPC要求客户端修正。（但客户端可能后续仍会修改该值）
	 */ 
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerFire(const FVector_NetQuantize& TraceHitTarget, float FireDelay);

	// 开火多播
	UFUNCTION(NetMulticast, Reliable)
	void MulticastFire(const FVector_NetQuantize& TraceHitTarget);

	// 复数射线武器(霰弹枪)的开火RPC，由客户端向服务器发送.TraceHitTargets是一个数组，里面的元素FVector_NetQuantize是一种经过了优化的Fvector，有利于网络传送且不损失精度
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerShotgunFire(const TArray<FVector_NetQuantize>& TraceHitTargets, float FireDelay);

	// 复数射线武器(霰弹枪)的开火多播，由服务器通知所有客户端执行。TraceHitTargets是一个数组，里面的元素FVector_NetQuantize是一种经过了优化的Fvector，有利于网络传送且不损失精度
	UFUNCTION(NetMulticast, Reliable)
	void MulticastShotgunFire(const TArray<FVector_NetQuantize>& TraceHitTargets);

	// 鼠标镜头十字准星瞄准处理
	virtual void TraceUnderCrosshairs(FHitResult& TraceHitResult);

	// 装填动画完成，将战斗状态设置回待机状态，需要暴露给动画蓝图调用。它被动画蓝图的动画通知调用。
	UFUNCTION(BlueprintCallable)
	void FinishedReload();

	// 切换武器的动画播放完成，重置状态。需要暴露给动画蓝图调用。它被动画蓝图的动画通知调用。
	UFUNCTION(BlueprintCallable)
	void FinishSwap();

	UFUNCTION(BlueprintCallable)
	void FinishSwapAttachWeapons();
	
	UFUNCTION(BlueprintCallable)
	// 霰弹枪装填弹药的方法
	void ShotgunShellReload();

	// 角色动画跳转到霰弹枪装填结束段落
	void JumpToShotgunEnd();

	// 投掷手榴弹的过程完成，归位玩家战斗状态(由动画蓝图在动画通知中调用)
	UFUNCTION(BlueprintCallable)
	void ThrowGrenadeFinished();

	// 隐藏手上手榴弹，并生成要抛出去的手榴弹(由动画蓝图在动画通知中调用)
	UFUNCTION(BlueprintCallable)
	void LaunchGrenade();

	// 客户端发送RPC到服务器请求投掷手榴弹
	UFUNCTION(Server, Reliable)
	void ServerLauncherGrenade(const FVector_NetQuantize& Target);

	// 根据拾取的弹药数量增加玩家角色身上的对应弹药数量。
	void PickupAmmo(EWeaponType WeaponType, int32 AmmoAmount);

	/*
	 * 当前角色是否处于本地换弹逻辑中。
	 * 用于解决高延迟装弹场景下，使用客户端预测时，本地先一步装弹处理但手部IK受CombatState还未被网络更新的影响，从而导致换弹动画手部抽搐的问题。
	 */
	bool bLocallyReloading = false;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	// 是否执行瞄准行为
	UFUNCTION(BlueprintCallable)
	void SetAiming(bool bIsAiming);

	UFUNCTION(Server, Reliable)
	void SererSetAmiming(bool bIsAiming);

	// 在EquipWeapon变化时调用
	UFUNCTION()
	void OnRep_EquipWeapon();
	// 在SecondaryWeapon变化时调用
	UFUNCTION()
	void OnRep_SecondaryWeapon();

	// 处理每帧武器准星的扩散和偏移
	void SetHUDCrosshairs(float DeltaTime);

	// 发送RPC给服务器，告知服务器玩家要换弹
	UFUNCTION(Server, Reliable)
	void ServerReload();
	// 服务器和所有客户端都会调用的装弹行为
	void HandleReload();
	// 计算要装填的子弹数
	int32 AmountToReload();

	// 客户端投掷手榴弹
	void ThrowGrenade();
	
	// 客户端发送RPC给服务器，申请投掷手榴弹
	UFUNCTION(Server, Reliable)
	void ServerThrowGrenade();

	// 要抛出去的激活形态下的手榴弹
	UPROPERTY(EditAnywhere)
	TSubclassOf<class AProjectile> GrenadeClass;
	
	// 更新弹药
	void UpdateAmmoValues();
	// 更新霰弹枪的弹药数量
	void UpdateShotgunAmmoValues();

	// 放下当前手上的武器
	void DropEquippedWeapon();
	// 将Actor(通常是游戏场景内的某个物品)附加到玩家右手上
	void AttachActorToRightHand(AActor* ActorToAttach);
	// 将Actor(通常是游戏场景内的某个物品)附加到玩家左手上
	void AttachActorToLeftHand(AActor* ActorToAttach);
	// 将旗子附着到左手上
	void AttachFlagToLefthand(AActor* Flag);
	// 将Actor(通常是游戏场景内的某个物品)附加到玩家角色的背包插槽上
	void AttachActorToBackpack(AActor* ActorToAttach);
	
	// 根据当前手上的武器类型检索角色对应的后备弹药数量，并将其刷新到HUD上
	void UpdateCarriedAmmo();
	// 播放拾取装备武器的声音
	void PlayEquipWeaponSound(AWeapon* WeaponToEquip);
	// 手上装备的武器的弹匣如若为空，则装填弹匣
	void ReloadEmptyWeapon();
	
	// 是否显示玩家角色的手部的手榴弹
	void ShowAttachedGrenade(bool bShowGrenade);

	// 切换到主武器
	void EquipPrimaryWeapon(AWeapon* WeaponToEquip);
	// 切换到副武器
	void EquipSecondaryWeapon(AWeapon* WeaponToEquip);

	// 鼠标准星瞄准的位置点。(TickComponent函数中执行的函数TraceUnderCrosshairs(HitResult)每帧更新该值)。不仅作为protected变量，还需要暴露给UE的行为树的自定义任务蓝图。
	UPROPERTY(BlueprintReadWrite)
	FVector HitTarget;

	// 战斗组件所属的玩家角色
	class ABlasterCharacter* Character;
	
private:
	
	// 战斗组件所属的玩家控制器
	class ABlasterPlayerController* Controller;
	// 绘制UI的HUD
	class ABlasterHUD* HUD;

	// 设置一个属性为可网络复制时，需要在GetLifetimeReplicatedProps生命周期中注册
	// 装备的武器(主武器)
	UPROPERTY(ReplicatedUsing = OnRep_EquipWeapon)
	AWeapon* EquippedWeapon;

	// 副武器(次要武器)
	UPROPERTY(ReplicatedUsing = OnRep_SecondaryWeapon)
	AWeapon* SecondaryWeapon;
	
	// 玩家角色当前是否处于瞄准中的状态
	// UPROPERTY(Replicated)	// 原本仅有网络复制，没有回调函数处理，高延迟下会导致客户端回退问题。
	UPROPERTY(ReplicatedUsing = OnRep_Aiming)
	bool bAiming = false;

	// 玩家当前是否按住鼠标右键进行瞄准。该变量用于解决高延迟状态下使用客户端预测，客户端瞬间快速缩放镜头，被服务器后续回复RPC，强制修正镜头的问题(客户端回退问题)
	bool bAimButtomPressed = false;
	
	// bAiming被服务器网络更新后的执行的回调函数
	UFUNCTION()
	void OnRep_Aiming();

	// 玩家角色基本移速
	UPROPERTY(EditAnywhere)
	float BaseWalkSpeed;

	// 玩家角色瞄准时的移速
	UPROPERTY(EditAnywhere)
	float AimWalkSpeed;

	// 是否按下开火键
	bool bFireButtonPressed;

	// 角色移动速度影响的准星偏移因子
	float CrosshairVelocityFactor;
	// 角色是否在空中(跳跃和落下)影响准星偏移因子
	float CrosshairInAirFactor;
	// 玩家角色瞄准时影响准星偏移的因子
	float CrosshairAimFactor;
	// 玩家射击时影响准星偏移的因子
	float CrosshairShootingFactor;

	// 准星偏移和颜色的数据包
	FHUDPackage HUDPackage;

	// 开火时相机镜头抖动
	void FireCameraShake();
	
	/*
	 * Aiming and FOV
	 */
	// 不开镜时的移速
	// Field of view when not aiming; set to the camera's base FOV in BeginPlay.
	// 默认的摄像机视野
	float DefaultFOV;
	// 当前摄像机视野
	float CurrentFOV;
	// 从武器处获取的开镜视野大小
	UPROPERTY(EditAnywhere, Category = Combat)
	float ZoomedFOV = 30.f;
	// 从武器处获取的开镜视野速度
	UPROPERTY(EditAnywhere, Category = Combat)
	float ZoomInterpSpeed = 20.f;
	// 开镜函数
	void InterpFOV(float DeltaTime);

	/*
	 * Automatic Fire
	 */
	FTimerHandle FireTimer;
	
	// 能否开火tag，射击间隔影响该tag
	bool bCanFire = true;

	// 开火条件判断（包含射击间隔等条件校验）
	bool CanFire();

	// 设置开火定时器
	void StartFireTimer();
	// 被开火定时器定时执行的开火函数
	void FireTimerFinished();

	// Carried Ammo for the currently-equipped weapon
	// 玩家当前携带的武器的对应类型的弹药
	UPROPERTY(ReplicatedUsing = OnRep_CarriedAmmo)
	int32 CarriedAmmo;

	// 回调函数，携带的弹药被网络更新时调用
	UFUNCTION()
	void OnRep_CarriedAmmo();

	// map并不能直接网络复制，因为使用哈希算法算出的结果在每台机器上都不同？
	// 武器类型和弹药类型的映射
	TMap<EWeaponType, int32> CarriedAmmoMap;

	// 玩家当前携带的武器的对应的弹药的最大后备弹药数量
	UPROPERTY(EditAnywhere)
	int32 MaxCarriedAmmo = 500;
	
	// 角色初始携带的AR枪械的弹药数量
	UPROPERTY(EditAnywhere)
	int32 StartingARAmmo = 30;
	// 角色初始携带的火箭弹的弹药数量
	UPROPERTY(EditAnywhere)
	int32 StartingRocketAmmo = 0;
	// 角色初始携带的手枪弹药数量
	UPROPERTY(EditAnywhere)
	int32 StartingPistolAmmo = 0;
	// 角色初始携带的冲锋枪弹药数量
	UPROPERTY(EditAnywhere)
	int32 StartingSubmachineAmmo = 0;
	// 角色初始携带的霰弹枪弹药数量
	UPROPERTY(EditAnywhere)
	int32 StartingShotgunAmmo = 0;
	// 角色初始携带的狙击枪弹药数量
	UPROPERTY(EditAnywhere)
	int32 StartingSniperAmmo = 0;
	// 角色初始携带的榴弹枪弹药数量
	UPROPERTY(EditAnywhere)
	int32 StartingGrenadeLauncherAmmo = 0;
	
	// 初始化玩家角色的各种枪械弹药数量
	void InitializeCarriedAmmo();

	// 战斗状态的枚举值，需要被网络更新复制，被网络更新时调用回调函数OnRep_CombatState
	UPROPERTY(ReplicatedUsing = OnRep_CombatState)
	ECombatState CombatState = ECombatState::ECS_Unoccupied;
	// 客户端的战斗状态被网络更新时，执行的回调函数
	UFUNCTION()
	void OnRep_CombatState();
	// 计算本次换弹需要往弹匣中添加的子弹数量
	int32 AmmoToReload();

	// 玩家角色当前的手榴弹数量。客户端的该属性接收服务器的网络更新，需要在GetLifetimeReplicatedProps中注册。EditAnywhere暴露给蓝图暂不添加。
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Grenades)
	int32 Grenades = 4;

	// 客户端玩家手榴弹数量被网络更新后，执行的回调函数。执行逻辑包括刷新客户端玩家的手榴弹UI
	UFUNCTION()
	void OnRep_Grenades();
	
	// 玩家角色手榴弹数量的最大携带数量。暴露给蓝图，可编辑。
	UPROPERTY(EditAnywhere)
	int32 MaxGrenades = 4;

	// 更新玩家角色的手榴弹UI显示
	void UpdateHUDGrenades();

	// 玩家角色当前是否举着旗子。该变量需要被网络复制，以正确显示模拟代理的玩家角色正确姿态
	UPROPERTY(ReplicatedUsing = OnRep_HoldingTheFlag)
	bool bHoldingTheFlag = false;

	UFUNCTION()
	void OnRep_HoldingTheFlag();

	// 玩家当前举着的旗子
	UPROPERTY()
	AWeapon* TheFlag;

	// 角色(瞄准状态下)移动时的镜头抖动相关参数
	UPROPERTY(EditAnywhere)
	TSubclassOf<UBlasterMovingMatineeCameraShake> BlasterMovingMatineeCameraShake;
	// 角色(瞄准状态下)移动时的镜头抖动
	void MoveCameraShake();
	
public:
	FORCEINLINE uint32 GetGrenades() const { return Grenades; }
	// 是否应该交换主副武器。（完整的业务逻辑：同时拥有主副武器时，按下e键将交换主副武器）
	bool ShouldSwapWeapons();
};
