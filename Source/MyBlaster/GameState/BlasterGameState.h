// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "BlasterGameState.generated.h"

/**
 * 
 */
UCLASS()
class MYBLASTER_API ABlasterGameState : public AGameState
{
	GENERATED_BODY()
public:
	// 注册要进行网络复制的属性的方法
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 更新排行榜的分数
	void UpdateTopScore(class ABlasterPlayerState* ScoringPlayer);
	
	// 得分最高的玩家。使用数组是因为可能存在并列第一。
	UPROPERTY(Replicated)
	TArray<ABlasterPlayerState*> TopScoringPlayers;
	/* 本类内第一次使用ABlasterPlayerState时，需要使用class修饰，或者include包含，如下所示
	 * 尖括号内不使用class修饰时，需要 #include "MyBlaster/PlayerState/BlasterPlayerState.h"
	 * 使用class修饰则不用，这种方式可避免在头文件中include其他文件。同一个名称只需要class修饰一次，后续再用ABlasterPlayerState无需再class修饰
	 */

	/*
	 * 队伍相关
	 * 疑惑：这样设计代码，以后一场比赛新增到三个四个甚至更多的队伍时，如何扩展？
	 */
	// 存储红队所有玩家的玩家状态的数组
	UPROPERTY()
	TArray<ABlasterPlayerState*> RedTeam;
	// 存储蓝队所有玩家的玩家状态的数组
	UPROPERTY()
	TArray<ABlasterPlayerState*> BlueTeam;
	
	// 红队得分。该变量需要注册网络复制，且被网络复制后回调函数是 OnRep_RedTeamScore
	UPROPERTY(ReplicatedUsing = OnRep_RedTeamScore)
	float RedTeamScore = 0.f;
	// 蓝队得分。该变量需要注册网络复制，且被网络复制后回调函数是 OnRep_BlueTeamScore
	UPROPERTY(ReplicatedUsing = OnRep_BlueTeamScore)
	float BlueTeamScore = 0.f;
	
	UFUNCTION()
	void OnRep_RedTeamScore();
	UFUNCTION()
	void OnRep_BlueTeamScore();

	// 增加红队分数
	void RedTeamScores();
	// 增加蓝队分数
	void BlueTeamScores();
	
private:
	// 当前所有玩家中的最高分
	float TopScore = 0.f;
};
