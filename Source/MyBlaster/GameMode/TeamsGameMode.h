// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BlasterGameMode.h"
#include "TeamsGameMode.generated.h"

/**
 * 游戏模式：团队对抗
 */
UCLASS()
class MYBLASTER_API ATeamsGameMode : public ABlasterGameMode
{
	GENERATED_BODY()

public:
	ATeamsGameMode();
	/*
	 * 当有玩家中途加入时，同样需要对该玩家进行队伍分配。因此重写父类PostLogin方法
	 */
	virtual void PostLogin(APlayerController* NewPlayer) override;
	/*
	 * 玩家中途离开了游戏时，也需要将其从队伍中移除、因此需要重写父类的Logout方法
	 */
	virtual void Logout(AController* Exiting) override;
	// 父类的伤害计算函数过于简单，不能满足需求，这里需要根据团队游戏模式的特点进行重写
	virtual float CalculateDamage(AController* Attacker, AController* Victim, float BaseDamage) override;

	// 团队对抗模式中，玩家被击杀后，需要增加击杀者所属队伍的分数。因此需要重写父类的玩家被击杀的方法
	virtual void PlayerEliminated(ABlasterCharacter* ElimmedCharacter, ABlasterPlayerController* VictimController, ABlasterPlayerController* AttackerController) override;
	
protected:
	/*
	 * 游戏开始时需要进行队伍分配。因此需要重写父类的游戏开始时的方法
	 */
	virtual void HandleMatchHasStarted() override;
};
