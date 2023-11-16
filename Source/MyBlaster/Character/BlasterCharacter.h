// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/TimelineComponent.h"
#include "GameFramework/Character.h"
#include "MyBlaster/BlasterTypes/CombatState.h"
#include "MyBlaster/BlasterTypes/Team.h"
#include "MyBlaster/BlasterTypes/TurningInPlace.h"
#include "MyBlaster/GameMode/BlasterGameMode.h"
#include "MyBlaster/Interfaces/InteractWithCrosshairsInterface.h"
#include "BlasterCharacter.generated.h"


// 声明一个动态多播委托。玩家离退出游戏时，调用该委托的实例，发送广播，所有绑定了该委托实例的函数都会被执行。（观察者模式，本委托相当于通知，绑定了本委托的函数都是订阅者(观察者)）
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLeftGame);

UCLASS()
class MYBLASTER_API ABlasterCharacter : public ACharacter, public IInteractWithCrosshairsInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABlasterCharacter();
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	// 重写父类的注册要网络复制更新的属性的方法
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// 重写父类的销毁方法
	virtual void Destroyed() override;
	// 初始化所有组件
	virtual void PostInitializeComponents() override;
	
	// 更新玩家弹药HUD
	void UpdateHUDAmmo();
	// 更新玩家血量HUD
	void UpdateHUDHealth();
	// 更新玩家护盾值HUD
	void UpdateHUDShield();

	// Poll for any relelvant classes and initialize our HUD
	// 在Tick中执行，保证玩家状态的属性不为空或保持玩家某些属性
	void PollInit();

	// 处理角色动画原地转身的逻辑
	void RotateInPlace(float DeltaTime);
	
	// 处理受到伤害的函。需要将该函数作为委托绑定到UE的伤害处理上。函数签名(参数种类和类型)需要符号UE的伤害处理的要求
	// DamageActor受伤的Actor、Damage伤害量、DamageType伤害类型、InstigatorController伤害来源者(通常为其他玩家)的控制器、DamageCauser伤害造成者(比如子弹)
	UFUNCTION()
	void ReceiveDamage(AActor* DamageActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser);

	// 角色死亡淘汰时的处理
	void Elim(bool bPlayerLeftGame);
	// 多播RPC，通知所有客户端开始该角色死亡动画的播放处理、暂停玩家操作、角色死亡粒子特效和声音等流程
	UFUNCTION(NetMulticast, Reliable)
	void MulticastElim(bool bPlayerLeftGame);

	// 是否打开狙击枪的瞄准镜UI界面。BlueprintImplementableEvent表示在蓝图中实现
	UFUNCTION(BlueprintImplementableEvent)
	void ShowSniperScopeWidget(bool bShowScope);

	// 生成预先设置的默认武器，并为玩家角色装备
	void SpawnDefaultWeapon();

	// 一个用于存放碰撞盒子组件指针的映射
	UPROPERTY()
	TMap<FName, class UBoxComponent*> HitCollisionBoxes;
	
	/*
	 * 客户端给服务器发送的RPC请求，告知服务器客户端的玩家要退出游戏
	 * 疑惑：这功能为什么写在角色里？角色被消灭在重生期间不就无法退出游戏了吗？应该写在玩家控制器里面吧、
	 */ 
	UFUNCTION(Server, Reliable)
	void ServerLeaveGame();
	
	// 实例化委托的实例
	FOnLeftGame OnLeftGame;

	// 多播RPC，通知所有客户端，玩家领先，显示其皇冠特效
	UFUNCTION(NetMulticast, Reliable)
	void MulticastGainedTheLead();
	// 多播RPC，通知所有客户端，玩家失去领先地位，销毁其皇冠特效
	UFUNCTION(NetMulticast, Reliable)
	void MulticastLostTheLead();

	// 根据玩家状态里的队伍设置该玩家的颜色
	void SetTeamColor(ETeam Team);
	
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);

	// 按键事件函数
	// 拾取并装备武器按钮事件
	void EquipButtonPressed();
	// 玩家角色下蹲事件
	void CrouchButtonPressed();
	// 玩家角色冲刺按钮按下与释放
	void SprintButtonPressed();
	void SprintButtonReleased();
	// 处理角色冲刺时最大走速的变化
	void Sprint();
	void UnSprint();
	UFUNCTION(Server, Reliable)
	void ServerSprint(bool bSprint);
	
	// 瞄准键按下与释放
	void AimButtonPressed();
	void AimButtonReleased();
	void CalculateAO_Pitch();

	// 瞄准偏移
	void AimOffset(float DeltaTime);

	// 开火键的按下与释放
	void FireButtonPressed();
	void FireButtonReleased();

	// 按下投掷手榴弹按键
	UFUNCTION(BlueprintCallable)
	void GrenadeButtonPressed();

	// 原地转身的处理
	void TurnInPlace(float DeltaTime);

	// 模拟代理的持枪转身处理
	void SimProxiesTurn();

	// 跳跃虚函数
	virtual void Jump() override;

	// 播放角色受击动画
	void PlayHitReactMontage();

	// 本函数当前仅用于角色死亡时，掉落武器或者销毁武器
	void DropOrDestroyWeapon(AWeapon* Weapon);
	// 本函数用于角色死亡时，处理角色身上的所有武器的掉落和销毁
	void DropOrDestroyWeapons();

	/*
	 * 团队模式下，不同队伍的出生点应该不同，因此在玩家角色生成后，需要将其移动到对应的队伍出生点的位置。
	 * 该函数获取所有玩家出生点过滤出与本玩家角色阵营一致的出生点，再将玩家设置在随意一处队伍出生点中。
	 * 本函数在OnPlayerStateInitialized玩家状态初始化时被调用。
	 */ 
	void SetSpawnPoint();
	// 玩家状态初始化
	void OnPlayerStateInitialized();

	// 战斗组件需要暴露给蓝图可见和读取，因为它是私有变量，还需要允许稀有变量被访问
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UCombatComponent* Combat;
	
	/*
	 * Hit Boxes used for Server-side rewind
	 * 用于服务器倒带的碰撞盒子。为了方便，名称与角色骨骼保持一致(包括大小写)
	 */
	// 头部
	UPROPERTY(EditAnywhere)
	class UBoxComponent* head;
	// 盆骨
	UPROPERTY(EditAnywhere)
	class UBoxComponent* pelvis;
	// 脊椎02
	UPROPERTY(EditAnywhere)
	class UBoxComponent* spine_02;
	// 脊椎03
	UPROPERTY(EditAnywhere)
	class UBoxComponent* spine_03;
	// 左上臂
	UPROPERTY(EditAnywhere)
	class UBoxComponent* upperarm_l;
	// 右上臂
	UPROPERTY(EditAnywhere)
	class UBoxComponent* upperarm_r;
	// 左下臂
	UPROPERTY(EditAnywhere)
	class UBoxComponent* lowerarm_l;
	// 右下臂
	UPROPERTY(EditAnywhere)
	class UBoxComponent* lowerarm_r;
	// 左手
	UPROPERTY(EditAnywhere)
	class UBoxComponent* hand_l;
	// 右手
	UPROPERTY(EditAnywhere)
	class UBoxComponent* hand_r;
	// 背包
	UPROPERTY(EditAnywhere)
	class UBoxComponent* backpack;
	// 背包上的毯子
	UPROPERTY(EditAnywhere)
	class UBoxComponent* blanket;
	// 左大腿
	UPROPERTY(EditAnywhere)
	class UBoxComponent* thigh_l;
	// 右大腿
	UPROPERTY(EditAnywhere)
	class UBoxComponent* thigh_r;
	// 左小腿
	UPROPERTY(EditAnywhere)
	class UBoxComponent* calf_l;
	// 右小腿
	UPROPERTY(EditAnywhere)
	class UBoxComponent* calf_r;
	// 左脚
	UPROPERTY(EditAnywhere)
	class UBoxComponent* foot_l;
	// 右脚
	UPROPERTY(EditAnywhere)
	class UBoxComponent* foot_r;
	
private:
	UPROPERTY(VisibleAnywhere, Category = Camera)
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = Camera)
	class UCameraComponent* FollowCamera;

	// EditAnywhere 使该成员能在任何地方被编辑
	// BlueprintReadOnly 使得蓝图可以以只读方式访问该成员
	// meta=(AllowPrivateAccess = "true")   使得该成员即便私有也能被访问
	// meta=(AllowPrivateAccess = "true")不存在则不能设置BlueprintReadOnly或BlueprintWriteOnly暴露给蓝图，因为创建的OverheadWidget属于private
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	class UWidgetComponent* OverheadWidget;

	// 该属性被网络更新时，调用OnRep_OverLappingWeapon ？
	UPROPERTY(ReplicatedUsing = OnRep_OverLappingWeapon)
	class AWeapon* OverlappingWeapon;

	UFUNCTION()
	void OnRep_OverLappingWeapon(AWeapon* LastWeapon);
	
	/*
	 * Blaster Components
	 * 角色组件
	 */
	// buff组件
	UPROPERTY(VisibleAnywhere)
	class UBuffComponent* Buff;

	// 延迟补偿组件
	UPROPERTY(VisibleAnywhere)
	class ULagCompensationComponent* LagCompensation;
	
	// 客户端发送RPC请求给服务器，通知服务器自己按下了装备按键
	UFUNCTION(Server, Reliable)
	void ServerEquipButtonPressed();

	// 该函数响应装弹按钮被按下的事件
	UFUNCTION()
	void ReloadButtonPressed();

	// 2023年5月16日22:30:07 即便复制了属性依旧导致玩家和服务器以外的第三者看到玩家左右偏移抽搐
	// 瞄准偏移相关属性
	// UPROPERTY(Replicated)
	float AO_Yaw;
	// InterpAO_Yaw 一个计算的中间量，用于计算转身相关的角色根运动。
	float InterpAO_Yaw;
	// UPROPERTY(Replicated)
	float AO_Pitch;
	// 
	FRotator StartingAimRotation;

	// 身体转向
	ETurningInPlace TurningInPlace;
	// 开火动画(角色因后坐力抖动)
	UPROPERTY(EditAnywhere, Category = Combat)
	class UAnimMontage* FireWeaponMontage;
	// 重新装弹动画
	UPROPERTY(EditAnywhere, Category = Combat)
	class UAnimMontage* ReloadMontage;

	// 受击动画
	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* HitReactMontage;

	// 角色死亡动画
	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* ElimMontage;

	// 角色投掷手榴弹的动画
	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* ThrowGrenadeMontage;

	// 角色切换武器的动画
	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* SwapMontage;
	
	// 角色背靠墙体时，隐藏角色和武器
	void HideCameraIfCharacterClose();
	// 用于设置角色背靠墙体就隐藏角色 的距离，这里设置为200.f
	UPROPERTY(EditAnywhere)
	float CameraThreshold = 200.f;

	/*
	 * 注意理解根骨骼运动和转身动画的关系。
	 * 角色动画转身了，但角色骨骼网格体还是朝着原来的方向，需要在动画蓝图中处理根骨骼运动，是否处理处理根骨骼运动以此处的bRotateRootBone为准。
	 */
	// 是否应该转动根骨骼 （角色持枪转身相关的处理）
	bool bRotateRootBone;

	// 模拟代理的角色的上一帧转向
	FRotator ProxyRotatorLastFrame;
	// 模拟代理的角色的当前转向
	FRotator ProxyRotation;
	// 模拟代理的角色需要转向的幅度
	float ProxyYaw;
	// 模拟代理的角色转身动画处理需要俩帧之间角度差(绝对值)达到多少才执行
	float TurnThreshold = 0.5f;
	// float TurnThreshold = 20.f;
	// 模拟代理的角色上一次转身的时间
	float TimeSinceLastMovementReplication;

	// 角色移动属性发生复制后会执行的回调函数。OnRep_ReplicatedMovement和OnRep_ReplicateMovement的区别是什么?
	virtual void OnRep_ReplicatedMovement() override;

	// 计算处理玩家角色的速度
	float CalculateSpeed();

	/*
	 * Player Health 玩家血量
	 */
	// 最大血量
	UPROPERTY(EditAnywhere, Category = "Player Stats")
	float MaxHealth = 100.f;
	// 当前血量。需要注册网络复制，更新所有客户端上每一个玩家的血量。
	// UPROPERTY(ReplicatedUsing = OnRep_Health, VisibleAnywhere, Category = "Player Stats")
	UPROPERTY(ReplicatedUsing = OnRep_Health, EditAnywhere, Category = "Player Stats")
	float Health = 100.f;
	// 角色血量变动后的回调函数。LastHealth是该属性被网络更新前的状态(值)，每个OnRep函数默认都有类似的值。
	UFUNCTION()
	void OnRep_Health(float LastHealth);
	
	/*
	 * Player Shield 玩家护盾值
	 */
	// 最大护盾值
	UPROPERTY(EditAnywhere, Category = "Player Stats")
	float MaxShield = 100.f;
	
	//  角色护盾值被网络复制后的回调函数。LastShield是该属性被网络更新前的状态(值)，每个OnRep函数默认都有类似的值。
	UFUNCTION()
	void OnRep_Shield(float LastShield);

	// 当前护盾值，需要注册网络复制，更新所有客户端上每一个玩家的护盾值。
	UPROPERTY(ReplicatedUsing = OnRep_Shield, EditAnywhere, Category = "Player Stats")
	float Shield = 0.f;
	
	// 角色是否已死亡
	bool bElimmed = false;

	// 角色死亡重生的定时器
	FTimerHandle ElimTimer;
	
	// 角色死亡重生的倒计时
	UPROPERTY(EditDefaultsOnly)
	float ElimDelay = 3.f;

	// 角色死亡重生定时器要执行的函数
	void ElimTimerFinished();

	/*
	 * 玩家退出游戏时的处理
	 */
	// 玩家是否已退出游戏
	bool bLeftGame = false;

	// 玩家角色的控制器
	class ABlasterPlayerController* BlasterPlayerController;

	/*
	 * DIssolve effect
	 * 角色死亡时用到的溶解效果
	 */
	// 溶解的时间曲线和值
	UPROPERTY(VisibleAnywhere)
	UTimelineComponent* DissolveTimeline;
	FOnTimelineFloat DissolveTrack;

	UPROPERTY(EditAnywhere)
	UCurveFloat* DissolveCurve;
	
	// 解材质的参数随时间的变化时，需要更新材质
	UFUNCTION()
	void UpdateDissolveMaterial(float DissolveValue);
	// 处理溶解材质的参数随时间的变化
	void StartDissolve();

	// Dynamic instance that we can change at runtime.
	// 角色死亡时需要应用的动态溶解材质(该材质实例中的参数Dissolve和Glow是会变化的)
	UPROPERTY(VisibleAnywhere, Category = Elim)
	UMaterialInstanceDynamic* DynamicDissolveMaterialInstance;

	// Material instance set on the Blueprint, used with the dynamic material instance
	// 角色死亡时的初始材质。使用初始材质创建动态材质，设置角色死亡时应用动态材质，通过Timeline改变动态材质的参数，使得角色网格体表现出被溶解的效果。
	// P207 优化为在SetTeamColor中设置，不再在蓝图中直接设置，因此EditAnywhere改为VisibleAnywhere
	UPROPERTY(VisibleAnywhere, Category = Elim)
	UMaterialInstance* DissolveMaterialInstance;

	/*
	 * Team Colors
	 * 玩家在不同颜色队伍里的溶解效果
	 */
	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* RedDissolveMatInst;

	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* RedMaterial;

	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* BlueDissolveMatInst;

	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* BlueMaterial;
	// 无队伍时的原始材质
	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* OriginalMaterial;
	
	/*
	 * Elim Effect	角色死亡时的粒子效果。非角色材质的溶解效果，注意区分
	 */
	// 需要在蓝图中选择并指定粒子效果资产
	UPROPERTY(EditAnywhere)
	UParticleSystem* ElimBotEffect;
	// 粒子组件，用于存储生成的粒子效果
	UPROPERTY(VisibleAnywhere)
	UParticleSystemComponent* ElimBotComponent;
	// 角色死亡粒子特效播放时需要一起播放的声音
	UPROPERTY(EditAnywhere)
	class USoundCue* ElimBotSound;

	/*
	 * 领先玩家头上的皇冠特效
	 */
	// 在蓝图中设置的ninagra皇冠
	UPROPERTY(EditAnywhere)
	class UNiagaraSystem* CrownSystem;
	// 实例化的皇冠
	UPROPERTY()
	class UNiagaraComponent* CrownComponent;

	// 控制本对象(Pawn)的玩家的状态
	class ABlasterPlayerState* BlasterPlayerState;

	// 未激活的手榴弹的静态网格体
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* AttachedGrenade;

	// 玩家角色默认武器，它是本类的次级类，是Aweapon的子类
	UPROPERTY(EditAnywhere)
	TSubclassOf<AWeapon> DefaultWeaponClass;

	/*
	 * 原教程没有的代码。
	 * 检查玩家是否领先。如果玩家领先，则激活其皇冠特效
	 */ 
	void CheckPlayerLead();
	// 原教程没有的代码。存储游戏状态
	class ABlasterGameState* BlasterGameState;

	// 游戏模式
	UPROPERTY()
	ABlasterGameMode* BlasterGameMode;
	
public:
	// FORCEINLINE void SetOverlappedWeapon(AWeapon* Weapon) { OverlappingWeapon = Weapon; }
	void SetOverlappedWeapon(AWeapon* Weapon);

	bool IsWeaponEquipped();

	bool IsAiming();

	// 
	FORCEINLINE float GetAO_Yaw() const { return AO_Yaw; }
	FORCEINLINE float GetAO_Pitch() const { return AO_Pitch; }
	// 获取角色摄像机的指针
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	
	AWeapon* GetEquippedWeapon();

	FORCEINLINE ETurningInPlace GetTurningInPlace() const { return TurningInPlace; }

	/*
	 * 疑惑：这大部分的动画播放，是能更能够重构为一个函数，传入一个名字或者什么东西，播放对应的动画。
	 */
	// 播放角色开火动画
	void PlayFireMontage(bool bAiming);
	// 播放角色重新装弹动画
	void PlayReloadMontage();
	// 播放角色死亡动画
	void PlayElimMontage();
	// 播放角色投掷手榴弹的动画
	void PlayThrowGrenadeMontage();
	// 播放角色切换武器的动画
	void PlaySwapMontage();

	// 多播RPC。因为这是外观视觉上的处理，真实权威的碰撞处理在服务器上，因此某个角色受击后玩家自己机器没播放动画也是可以接受的，使用不可靠Unreliable即可。
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastHit();

	FVector GetHitTarget() const;

	FORCEINLINE bool ShouldRotateRootBone() const { return bRotateRootBone; }
	UFUNCTION(BlueprintCallable)
	FORCEINLINE bool IsElimmed() const { return bElimmed; }
	FORCEINLINE float GetHealth() const { return Health; };
	FORCEINLINE void SetHealth(float Amount) { Health = Amount; }
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; };
	FORCEINLINE float GetShield() const { return Shield; };
	FORCEINLINE void SetShield(float Amount) { Shield = Amount; }
	FORCEINLINE float GetMaxShield() const { return MaxShield; }
	FORCEINLINE UAnimMontage* GetReloadMontage() const { return ReloadMontage; }
	FORCEINLINE UStaticMeshComponent* GetAttachedGrenade() const { return AttachedGrenade; }
	FORCEINLINE ULagCompensationComponent* GetLagCompensation() const { return LagCompensation; }
	
	// 获取玩家战斗组件的战斗状态
	ECombatState GetCombatState() const;
	// 获取战斗组件
	FORCEINLINE UCombatComponent* GetCombat() const { return Combat; }
	// 查询玩家当前的游戏操作是否被禁用
	FORCEINLINE bool GetDisableGameplay() const { return bDisableGameplay; }
	// 获取玩家角色的Buff组件
	FORCEINLINE UBuffComponent* GetBuff() const { return Buff; }
	// 获取该玩家角色是否举着旗子
	// FORCEINLINE bool IsHoldingTheFlag() const;
	bool IsHoldingTheFlag() const;

	// 是否禁用玩家的游戏操作(禁用除了鼠标转动镜头以外的大部分操作)，该值需要被网络复制
	UPROPERTY(Replicated)
	bool bDisableGameplay = false;

	// 角色是否处于本地换弹中
	bool IsLocallyReloading();

	/*
	 * 角色是否完成了本地切换武器动画中的切换玩武器的动作。本地播放切换武器动画代码执行后立刻设置false，切换武器动画播放即将结束时的通知触发的函数中将其设置回true。
	 * 主要用于动画蓝图读取该值，参与是否使用左手IK的判断
	 */ 
	bool bFinishedSwapping = false;

	// 获取玩家角色的所属队伍
	ETeam GetTeam();

	// 设置玩家是否处于举旗状态。
	void SetHoldingTheFlag(bool bHolding);
	
};
