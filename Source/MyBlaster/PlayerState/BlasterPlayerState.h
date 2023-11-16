// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "MyBlaster/BlasterTypes/Team.h"
#include "BlasterPlayerState.generated.h"

/**
 * 
 */
UCLASS()
class MYBLASTER_API ABlasterPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	// 属性网络复制的注册函数。(重写父类的方法)
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	// Score属性被网络复制更新时客户端机器调用的方法(重写父类的方法)，其父类的OnRep_Score已经使用UFUNCTION()作了修饰
	virtual void OnRep_Score() override;
	// Defeats属性被网络复制时客户端机器调用的方法。注意该方法父类并没有。无需override,但要UFUNCTION()修饰
	UFUNCTION()
	virtual void OnRep_Defeats();	
	// 服务器上用于更新玩家分数显示的函数
	void AddToScore(float ScoreAmount);
	// 服务器上用于更新玩家的死亡次数的显示
	void AddToDefeats(int32 DefeatsAmount);
private:
	/*
	 * 原生的代码中，玩家控制器可以直接访问玩家状态的属性，但玩家状态并不能直接访问玩家控制器的属性。
	 * 玩家状态可以访问玩家在控制的角色pawn的属性。
	 */
	// 存储本玩家状态对应的玩家角色。使用UPROPERTY()修饰，该属性未被初始化就被访问时，游戏不至于崩溃。
	UPROPERTY()
	class ABlasterCharacter* Character;
	// 存储本玩家状态对应的控制器
	UPROPERTY()
	class ABlasterPlayerController* Controller;
	// 死亡次数
	UPROPERTY(ReplicatedUsing = OnRep_Defeats)
	int32 Defeats;

	// 玩家所属队伍。默认无队伍。该变量需要被网络复制，需要在 GetLifetimeReplicatedProps函数 中注册
	UPROPERTY(Replicated)
	ETeam Team = ETeam::ET_NoTeam;

	// Team被网络复制后，需要执行的回调函数
	void OnRep_Team();

public:
	// 获取玩家队伍
	FORCEINLINE ETeam GetTeam() const { return Team; }
	// 设置玩家队伍
	void SetTeam(ETeam TeamToSet);
};
