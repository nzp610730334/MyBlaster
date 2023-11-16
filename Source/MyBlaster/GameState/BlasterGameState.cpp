// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterGameState.h"

#include "MyBlaster/HUD/BlasterHUD.h"
#include "MyBlaster/PlayerController/BlasterPlayerController.h"
#include "MyBlaster/PlayerState/BlasterPlayerState.h"
#include "Net/UnrealNetwork.h"

void ABlasterGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABlasterGameState, TopScoringPlayers);
	DOREPLIFETIME(ABlasterGameState, RedTeamScore);
	DOREPLIFETIME(ABlasterGameState, BlueTeamScore);
}

void ABlasterGameState::UpdateTopScore(ABlasterPlayerState* ScoringPlayer)
{
	// 当前排行榜最高分为空时，直接添加即可
	if(TopScoringPlayers.Num() == 0)
	{
		// 将该玩家的玩家状态的指针地址添加进数组
		TopScoringPlayers.Add(ScoringPlayer);
		// 记录当前最高分
		TopScore = ScoringPlayer->GetScore();
	}
	// 该玩家的分数与当前排行榜里的最高分相同，则应该并列第一
	else if(ScoringPlayer->GetScore() == TopScore)
	{
		// 并列第一时需要注意，使用唯一添加AddUnique()，已存在则不添加。即当已有的最高分的玩家也是该玩家时，则无需再添加。
		TopScoringPlayers.AddUnique(ScoringPlayer);
		// 疑惑：A玩家原本就100分第一进了排行榜，后续101分的时候无需添加进，但为什么不需要更新最高分？原教程没有对该情况处理
		UE_LOG(LogTemp, Warning, TEXT("该玩家的分数与当前排行榜里的最高分相同，则应该并列第一!"));
	}
	// 当该玩家的分数超越了最高分时，清空排行榜的所有并列玩家，添加该玩家进去
	else if(ScoringPlayer->GetScore() > TopScore)
	{
		// 清空排行榜里的所有玩家
		TopScoringPlayers.Empty();
		// 添加该玩家进去。疑惑：都已经清空了为什么还要使用AddUnique而不是使用Add就行。
		TopScoringPlayers.AddUnique(ScoringPlayer);
		// 更新最高分
		TopScore = ScoringPlayer->GetScore();
	}
}

void ABlasterGameState::OnRep_RedTeamScore()
{
	// 本函数在客户端上执行，从世界关卡获取的玩家控制器是客户端上的玩家的控制器
	ABlasterPlayerController* BlasterPlayerController = Cast<ABlasterPlayerController>(GetWorld()->GetFirstPlayerController());
	if(BlasterPlayerController)
	{
		// 更新团队得分
		BlasterPlayerController->SetHUDRedTeamScore(RedTeamScore);
	}
}

void ABlasterGameState::OnRep_BlueTeamScore()
{
	// 本函数在客户端上执行，从世界关卡获取的玩家控制器是客户端上的玩家的控制器
	ABlasterPlayerController* BlasterPlayerController = Cast<ABlasterPlayerController>(GetWorld()->GetFirstPlayerController());
	if(BlasterPlayerController)
	{
		// 更新团队得分
		BlasterPlayerController->SetHUDBlueTeamScore(BlueTeamScore);
	}
}

void ABlasterGameState::RedTeamScores()
{
	// 直接使用前自增处理即可
	++RedTeamScore;
	/*
	 * 更新服务器上的玩家的团队得分显示。注意：这里处理的是服务器上的显示，客户端的相关处理在回调函数OnRep_OnRep_队伍Score函数中
	 */
	// 获取玩家控制器(从世界关卡获取到的只有一个玩家。如果在服务器上通过游戏模式则能获取所有玩家)
	ABlasterPlayerController* BlasterPlayerController = Cast<ABlasterPlayerController>(GetWorld()->GetFirstPlayerController());
	if(BlasterPlayerController)
	{
		// 更新团队得分
		BlasterPlayerController->SetHUDRedTeamScore(RedTeamScore);
	}
	UE_LOG(LogTemp, Warning, TEXT("红队得了一分！"));
}

void ABlasterGameState::BlueTeamScores()
{
	++BlueTeamScore;
	/*
	 * 更新服务器上的玩家的团队得分显示。注意：这里处理的是服务器上的显示，客户端的相关处理在回调函数OnRep_OnRep_队伍Score函数中
	 */
	// 获取玩家控制器(从世界关卡获取到的只有一个玩家。如果在服务器上通过游戏模式则能获取所有玩家)
	ABlasterPlayerController* BlasterPlayerController = Cast<ABlasterPlayerController>(GetWorld()->GetFirstPlayerController());
	if(BlasterPlayerController)
	{
		// 更新团队得分
		BlasterPlayerController->SetHUDBlueTeamScore(BlueTeamScore);
	}
	UE_LOG(LogTemp, Warning, TEXT("蓝队得了一分！"));
}
