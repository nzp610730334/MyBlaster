// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MyBlaster/GameState/BlasterGameState.h"
#include "MyBlaster/HUD/CharacterOverlay.h"
#include "MyBlaster/PlayerState/BlasterPlayerState.h"
#include "BlasterPlayerController.generated.h"

/**
 * 
 */

/*
 * 声明一个动态多波委托，委托的名字：，该委托只有一个参数，类型为bool，参数名字：
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHighPingDelegate, bool, bPingTooHigh);

UCLASS()
class MYBLASTER_API ABlasterPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// 设置血量的HUD显示
	void SetHUDHealth(float Health, float MaxHealth);
	// 设置护盾的HUD显示
	void SetHUDShield(float Shield, float MaxShield);
	
	// 设置分数显示
	void SetHUDScore(float Score);
	// 设置死亡次数显示
	void SetHUDDefeats(int32 Defeats);
	// 设置弹匣子弹数量显示
	void SetHUDWeaponAmmo(int32 AmmoAmount);
	// 设置玩家携带的后备子弹数量
	void SetHUDCarriedAmmo(int32 Ammo);
	// 设置玩家的当前手榴弹数量（手榴弹的最大值暂不设置，UI写死）
	void SetHUDGrenades(int32 Grenades);
	// 设置游戏时间的刷新
	void SetHUDMatchCountdown(float CountDownTime);
	// 设置游戏正式开始前的热身时间倒计时
	void SetHUDAnnouncementCountdown(float CountDownTime);
	
	// 重写父类的对某个pawn受控时调用的方法
	virtual void OnPossess(APawn* InPawn) override;
	// 每帧调用的函数
	virtual void Tick(float DeltaTime) override;
	// 注册要网络复制更新的属性的方法，重写了父类的方法
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 隐藏团队分数显示
	void HideTeamScores();
	// 初始化团队分数显示
	void InitTeamScores();
	// 设置红队分数的显示
	void SetHUDRedTeamScore(int32 RedScore);
	// 设置蓝队分数的显示
	void SetHUDBlueTeamScore(int32 BlueScore);
	
	// 计算获得服务器的世界关卡的时间
	// Synced with server world clock.
	virtual float GetServerTime();  

	/* 来自父类的注释：
	 * Called after this PlayerController's viewport/net connection is associated with this player controller.
	 * 在该玩家控制器的视口/网络连接与该玩家控制器关联后调用。
	 * Sync with server clock as soon as possible.
	 */ 
	virtual void ReceivedPlayer() override;

	// 设置比赛状态(通常被游戏模式GameMode调用)。不传bTeamMatch时，该参数默认为false
	void OnMatchStateSet(FName State, bool bTeamsMatch = false);

	// 游戏进入"游戏中"的阶段时需要处理的UI绘制显示。不传bTeamMatch时，该参数默认为false
	void HandleMatchHasStarted(bool bTeamsMatch = false);
	// 游戏进入"比赛冷却"阶段时需要处理的UI绘制显示。比赛冷却阶段：一局比赛结束后的休息时间
	void HandleCooldown();

	// 客户端和服务器通信的单程时长(客户端到服务器的时间等于服务器到客户端的时间即是单程时长)
	float SingleTripTime = 0.f;

	// 已经声明过的委托。委托的实例名字HighPingDelegate
	FHighPingDelegate HighPingDelegate;

	// 本函数在游戏模式的玩家击杀处理中被调用(游戏模式仅存在于服务器上)，本函数中将对本控制器所在的客户端发送RPC请求通知，更新击杀通告。
	void BroadcastElim(APlayerState* Attacker, APlayerState* Victim);

	// 控制器广播伤害数字
	void BroadcastDamageNumber(float DamageNumber, AActor* HitCharacter, bool bCriticalStrike);
	
protected:
	// 重写父类的设置输入组件的方法
	virtual void SetupInputComponent() override;
	
	// 重写父类的BeginPlay方法
	virtual void BeginPlay() override;
	// 处理是否要刷新与时间有关的数值UI的变化逻辑。该函数被每帧调用，但只有经过一定时间(1s)才刷新游戏时间的UI显示.
	void SetHUDTime();

	// 供客户端的机器调用，发送给服务器的RPC请求。可视作客户端只发出消息(参数)，函数具体行为在服务器机器上执行。最终结果是更新客户端和服务器的世界关卡的时间差。
	UFUNCTION(Server, Reliable)
	void ServerRequestServerTime(float TimeOfClientRequest);
	// 供服务器的机器调用，发送给客户端的RPC请求。可视作服务器只发出消息(参数)，函数具体行为在客户端机器上执行。最终结果是更新客户端和服务器的世界关卡的时间差。
	UFUNCTION(Client, Reliable)
	void ClientReportServerTime(float TimeOfClientRequest, float TimeServerReceivedClientRequest);

	// 客户端和服务器之间的世界关卡的时间差。通常情况下，客户端的启动总在服务器之后，因此客户端的世界关卡时间总是落后于服务器的。
	// difference between client and server time.
	float ClientServerDelta = 0.f;

	// 客户端和服务器之间多久同步一次时间
	UPROPERTY(EditAnywhere, Category = Time)
	float TimeSyncFrequency = 5.f;
	// 距离上次客户端和服务器时间同步之后过去了多长时间。Tick函数每次被调用都累加更新本值，每次时间同步完成后重置本值。
	float TimeSyncRunningTime = 0.f;
	/*
	 * 客户端检查是否需要进行和服务器的时间同步。
	 * 所谓的时间同步，实际是更新客户端和服务器的时间差ClientServerDelta，客户端本地世界关卡时间+时间差可得服务器当前的世界关卡时间。
	 */ 
	void CheckTimeSync(float DeltaTime);
	// 某些确保执行到位的逻辑在PollInit()执行，PollInit()在Tick中被执行。
	void PollInit();

	// 客户端向服务器发送的RPC，要求更新服务器上的玩家控制器的比赛状态和时间设置
	UFUNCTION(Server, Reliable)
	void ServerCheckMatchState();
	// 服务器向客户端发送的RPC，更新客户端上的玩家控制器的比赛状态和时间设置。业务流程中，只在玩家加入游戏中，会进行一次调用，因此即便是Reliable，消耗的资源和占用的宽带资源是可以接受的
	UFUNCTION(Client, Reliable)
	void ClientJoinMidgame(FName StateOfMatch, float Warmup, float Match, float Cooldown, float StartingTime);

	// 开始高Ping警告的显示
	void HighPingWarning();
	
	// 停止高Ping警告的显示
	void StopHighPingWarning();

	// 检查Ping的值并处理高Ping图标的显示和隐藏
	void CheckPing(float DeltaTime);
	
	// 显示后隐藏菜单
	void ShowReturnToMainMenu();

	// 服务器发送RPC，通知客户端更新击杀通告
	UFUNCTION(Client, Reliable)
	void ClientElimAnnouncement(APlayerState* Attacker, APlayerState* Victim);

	// 服务器发送RPC，通知客户端命中的伤害数字
	UFUNCTION(Client, Reliable)
	void ClientHitedDamageNumber(float DamageNumber, AActor* HitCharacter, bool bCriticalStrike);

	// 是否显示团队得分
	UPROPERTY(ReplicatedUsing = OnRep_ShowTeamScores)
	bool bShowTeamScores = false;

	UFUNCTION()
	void OnRep_ShowTeamScores();

	// 获取得分最高的玩家，根据机器上玩家的不同，将得分信息转换为合适的FString并返回
	FString GetInfoText(const TArray<ABlasterPlayerState*>& Players);

	// 获取团队模式下应该显示的比赛结果。团队得分在游戏状态中
	FString GetTeamInfoText(ABlasterGameState* BlasterGameState);
	
private:
	// 绘制UI的类
	UPROPERTY()
	class ABlasterHUD* BlasterHUD;

	/*
	 * Return to Main menu
	 */
	// 在UE编辑器蓝图中设置的菜单组件
	UPROPERTY(EditAnywhere, Category = HUD)
	TSubclassOf<class UUserWidget> ReturnToMainMenuWidget;

	// 实例化之后的菜单组件
	UPROPERTY()
	class UReturnToMainMenu* ReturnToMainMenu;

	// 表示菜单是否已打开的tag
	bool bReturnToMainMenuOpen = false;

	// 游戏模式
	UPROPERTY()
	class ABlasterGameMode* BlasterGameMode;
	
	// 当前世界关卡的开始时间
	float LevelStartingTime = 0.f;
	// 热身阶段的时间长度
	float WarmupTime = 0.f;
	// 一局游戏的时间长度
	float MatchTime = 0.f;
	// 比赛结束后的休息时长
	float CooldownTime = 0.f;
	// 游戏时间的UI要展示的剩余的游戏时间
	uint32 CountdownInt = 0;

	// 比赛(游戏)状态。需要被网络复制更新。被网络复制更新时调用OnRep_MatchState回调函数
	UPROPERTY(ReplicatedUsing = OnRep_MatchState)
	FName MatchState;

	UFUNCTION()
	void OnRep_MatchState();

	// 玩家UI内容
	UPROPERTY()
	class UCharacterOverlay* CharacterOverlay;
	
	// 是否需要更新玩家UI。2023年6月15日02:04:39 P112 该变量目前没有发现被用于判断的地方
	// bool bInitializeCharacterOverlay = false;
	
	// HUD的组件都是最后初始化的，因此可能出现要进行网络更新，本地客户端的组件却还没初始化的情况，会导致HUD更新失败。
	// HUD更新失败时，需要暂存的血量
	float HUDHealth;
	// Health 的HUD是否更新(初始化)失败的tag
	bool bInitializeHealth = false;
	// HUD更新失败时，需要暂存的最大血量
	float HUDMaxHealth;
	bool bInitializeMaxHealth = false;

	// HUD更新失败时，需要暂存的分数
	float HUDScore;
	// 该HUD是否更新(初始化)失败的tag
	bool bInitializeScore = false;

	// HUD更新失败时，需要暂存的死亡次数
	int32 HUDDefeats;
	// 该HUD是否更新(初始化)失败的tag
	bool bInitializeDefeats = false;

	/*
	 * HUD更新失败时，需要暂存的手榴弹数量。这与玩家的后备子弹数不同，手榴弹数在玩家生成时就需要显示了，后备子弹数只在玩家拾取武器后才根据武器类型显示不同子弹数。
	 * 暂存的数值，用于在控制器中每帧实行的PollInit函数中更新玩家UI
	 */ 
	int32 HUDGrenades;
	// 该HUD是否更新(初始化)失败的tag，每个属性使用单独的tag是为了便于单独管理，否则使用同一个tag导致某个属性需要刷新，执行的却是全部刷新。
	bool bInitializeGrenades = false;
	
	// HUD更新失败时，需要暂存的护盾值
	float HUDShield;
	// 该HUD是否更新(初始化)失败的tag
	bool bInitializeShield = false;

	// HUD更新失败时，需要暂存的最大护盾
	float HUDMaxShield;
	// 该HUD是否更新(初始化)失败的tag
	bool bInitializeMaxShield = false;

	// HUD更新失败时，需要暂存的该种类的后备弹药数量
	int32 HUDCarriedAmmo;
	// 该HUD是否更新(初始化)失败的tag
	bool bInitializeCarriedAmmo = false;
	
	// HUD更新失败时，需要暂存的手上的武器的弹匣弹药数量
	int32 HUDWeaponAmmo;
	// 该HUD是否更新(初始化)失败的tag
	bool bInitializeWeaponAmmo = false;

	// 距离上次查询Ping值已经过去的时间
	float HighPingRunningTime = 0.f;
	// 高Ping警告图标显示的时长
	UPROPERTY(EditAnywhere)
	float HighPingDuration = 5.f;
	// 高Ping图标已经显示了多长时间
	float PingAnimationRunningTime = 0.f;
	// 查询ping值的频率，每20s查询一次
	UPROPERTY(EditAnywhere)
	float CheckPingFrequency = 20.f;

	// 客户端向服务器报告自己的延迟情况(是否高延迟)
	UFUNCTION(Server, Reliable)
	void ServerReportPingStatus(bool bHighPing);
	
	// 高Ping阈值（超过多少认为是高Ping）
	UPROPERTY(EditAnywhere)
	float HighPingThreshold = 50.f;
};
