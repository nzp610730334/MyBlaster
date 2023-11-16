// Fill out your copyright notice in the Description page of Project Settings.


#include "CaptureFlagGameMode.h"

#include "MyBlaster/GameState/BlasterGameState.h"
#include "MyBlaster/Weapon/Flag.h"

void ACaptureFlagGameMode::PlayerEliminated(ABlasterCharacter* ElimmedCharacter,
                                            ABlasterPlayerController* VictimController, ABlasterPlayerController* AttackerController)
{
	// 父类Super的PlayerEliminated不符合本子类的需求，不执行，但父类的父类ABlasterGamemode中的击杀玩家逻辑是本子类需要执行的。
	// Super::PlayerEliminated(ElimmedCharacter, VictimController, AttackerController);
	ABlasterGameMode::PlayerEliminated(ElimmedCharacter, VictimController, AttackerController);
}

void ACaptureFlagGameMode::FlagCapture(AFlag* Flag, AFlagZone* Zone)
{
	// 判断旗子与夺旗得分区的所属队伍不一致，不一致则得分
	bool bValidCapture = Flag->GetTeam() != Zone->Team;
	// (原教程没有的代码，漏了？)旗子与得分区队伍属性一致，则不得分，直接返回
	if(!bValidCapture) return;
	// 获取并转换游戏状态
	ABlasterGameState* BGameState = GetGameState<ABlasterGameState>();
	if(BGameState)
	{
		// 通过得分区的所属即可判断得分的是哪个队伍
		if(Zone->Team == ETeam::ET_RedTeam)
		{
			// 红队得分
			BGameState->RedTeamScores();
		}
		if(Zone->Team == ETeam::ET_BlueTeam)
		{
			// 蓝队得分
			BGameState->BlueTeamScores();
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("夺旗得分区判定完毕！"));
}
