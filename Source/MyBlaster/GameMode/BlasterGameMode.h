// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "BlasterGameMode.generated.h"


namespace MatchState
{
	// 声明本项目自定义的游戏状态，extern标明它在其他文件中被初始化。
	extern MYBLASTER_API const FName Cooldown;		// Match Duration has been reached.Display winner and begin cooldown timer.
}
/**
 * 
 */
UCLASS()
class MYBLASTER_API ABlasterGameMode : public AGameMode
{
	GENERATED_BODY()
public:
	ABlasterGameMode();
	virtual void Tick(float DeltaTime) override;
	
	// 玩家淘汰时执行的函数。ElimmedCharacter被淘汰的角色、VictimController淘汰者的控制器、AttackerController攻击者的控制器
	virtual void PlayerEliminated(class ABlasterCharacter* ElimmedCharacter, class ABlasterPlayerController* VictimController, ABlasterPlayerController* AttackerController);
	// 处理角色死亡重生的函数
	virtual void RequestRespawn(ACharacter* ElimmedCharacter, AController* ElimmedController);
	
	// 应用伤害前需要进行的计算处理(比如判别友军等逻辑)。virtual修饰为了使子类能重写
	virtual float CalculateDamage(AController* Attacker, AController* Victim, float BaseDamage);
	
	/*
	 * 设置的热身倒计时的长度。热身倒计时结束后才生成玩家开始游戏。
	 * 注意：继承本类的团队模式类中，游戏开始前对玩家角色进行了材质修改。当前存在bug，如果DelayStart设置为false或者WarmupTime过小，比如小于2s，游戏测试时，
	 * 服务器比客户端先启动，服务器WarmupTime结束，服务器游戏状态进入了下一阶段，而此时客户端才启动完毕，直接进入了下一阶段，客户端没有被成功设置材质，游戏会报错崩溃。
	 */ 
	UPROPERTY(EditDefaultsOnly)
	float WarmupTime = 10.f;

	// 设置一句游戏的时间长度
	UPROPERTY(EditDefaultsOnly)
	float MatchTime = 120.f;

	// 比赛冷却时间(一局比赛结束之后的休息时间)
	UPROPERTY(EditDefaultsOnly)
	float CooldownTime = 10.f;
	
	// 当前世界关卡生成时的时间
	float LevelStartingTime = 0.f;

	// (服务器)处理要要退出游戏的该玩家的相关事宜(如分数排行榜的处理)
	void PlayerLeftGame(class ABlasterPlayerState* PlayerLeaving);

	// 当前游戏逝魔是否为团队对抗
	bool bTeamsMatch = false;

protected:
	virtual void BeginPlay() override;
	
	/** Overridable virtual function to dispatch the appropriate transition functions before GameState and Blueprints get SetMatchState calls.
	 * 可重写的虚拟函数，以便在GameState和Blueprint获得SetMatchState调用之前调度适当的转换函数。
	 */
	virtual void OnMatchStateSet() override;
	
private:
	// 记录实时的倒计时。在游戏不同阶段，倒计时的含义也不同。
	float CountDownTime = 0.f;
public:
	FORCEINLINE float GetCountdownTime() const { return CountDownTime; }
};
