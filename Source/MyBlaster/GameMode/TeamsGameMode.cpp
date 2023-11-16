// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamsGameMode.h"

#include "Kismet/GameplayStatics.h"
#include "MyBlaster/GameState/BlasterGameState.h"
#include "MyBlaster/PlayerController/BlasterPlayerController.h"
#include "MyBlaster/PlayerState/BlasterPlayerState.h"

ATeamsGameMode::ATeamsGameMode()
{
	bTeamsMatch = true;
}

void ATeamsGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// 获取游戏状态并转换
	ABlasterGameState* BGameState = Cast<ABlasterGameState>(UGameplayStatics::GetGameState(this));
	// 转换成功
	if(BGameState)
	{
		// 通过新玩家的控制器获取其玩家状态，并转换为本项目的玩家状态类
		ABlasterPlayerState* BPState = NewPlayer->GetPlayerState<ABlasterPlayerState>();
		// 转换成功且该玩家未被分配队伍
		if(BPState && BPState->GetTeam() == ETeam::ET_NoTeam)
		{
			// 当蓝队大于等于红队人数时，将当前遍历到的玩家添加到红队，并设置其为红队队员
			if(BGameState->BlueTeam.Num() >= BGameState->RedTeam.Num())
			{
				BGameState->RedTeam.AddUnique(BPState);
				BPState->SetTeam(ETeam::ET_RedTeam);
			}
			// 否则，将当前遍历到的玩家添加到红队，并设置其为红队队员
			else
			{
				BGameState->BlueTeam.AddUnique(BPState);
				BPState->SetTeam(ETeam::ET_BlueTeam);
			}
		}
	}
}

void ATeamsGameMode::Logout(AController* Exiting)
{
	// 这里选择不执行父类的方法。否则直接登出游戏会话？
	// Super::Logout(Exiting);

	// 获取并转换游戏状态
	ABlasterGameState* BGameState = Cast<ABlasterGameState>(UGameplayStatics::GetGameState(this));
	// 获取并转换玩家状态
	ABlasterPlayerState* BPState = Exiting->GetPlayerState<ABlasterPlayerState>();
	// 两者都获取并转换成功
	if(BGameState && BPState)
	{
		// 蓝队中是否包含该玩家的玩家状态，包含则移除
		if(BGameState->RedTeam.Contains(BPState))
		{
			BGameState->RedTeam.Remove(BPState);
		}
		// 红队中是否包含该玩家的玩家状态，包含则移除
		if(BGameState->BlueTeam.Contains(BPState))
		{
			BGameState->BlueTeam.Remove(BPState);
		}
	}
}

void ATeamsGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	// 获取游戏状态并转换。注意需要传入世界上下文为this，即对象自身
	ABlasterGameState* BGameState = Cast<ABlasterGameState>(UGameplayStatics::GetGameState(this));
	// 转换成功
	if(BGameState)
	{
		// 遍历并转换所有玩家状态
        for(auto PState : BGameState->PlayerArray)
        {
        	// 将该玩家状态转换为本项目的玩家状态
        	ABlasterPlayerState* BPState = Cast<ABlasterPlayerState>(PState.Get());
        	// 转换成功且该玩家未被分配队伍
        	if(BPState && BPState->GetTeam() == ETeam::ET_NoTeam)
        	{
        		// 当蓝队大于等于红队人数时，将当前遍历到的玩家添加到红队，并设置其为红队队员
                if(BGameState->BlueTeam.Num() >= BGameState->RedTeam.Num())
                {
                    BGameState->RedTeam.AddUnique(BPState);
                    BPState->SetTeam(ETeam::ET_RedTeam);
                }
                // 否则，将当前遍历到的玩家添加到红队，并设置其为红队队员
        		else
        		{
        			BGameState->BlueTeam.AddUnique(BPState);
        			BPState->SetTeam(ETeam::ET_BlueTeam);
        		}
        	}
        }
	}
}

float ATeamsGameMode::CalculateDamage(AController* Attacker, AController* Victim, float BaseDamage)
{
	/*
	 * 父类方法不执行，因为不需要其功能。
	 * 通过玩家状态中的Team属性。进行敌我分辨，根据结果返回需要应用的伤害。
	 */ 
	// 转换攻击者和被攻击者的玩家状态
	ABlasterPlayerState* AttackerPState = Attacker->GetPlayerState<ABlasterPlayerState>();
	ABlasterPlayerState* VictimPState = Victim->GetPlayerState<ABlasterPlayerState>();
	// 任一为空，则不满足比较的条件，直接返回
	if(AttackerPState == nullptr || VictimPState == nullptr) return BaseDamage;
	// 进行比较并根据不同情况处理
	// 自伤
	if(AttackerPState == VictimPState)
	{
		// 直接返回
		return BaseDamage;
	}
	// 攻击者和被攻击者的队伍相同，则返回 0 伤害
	if(AttackerPState->GetTeam() == VictimPState->GetTeam())
	{
		return 0.f;
	}
	// 以上条件过滤完毕，则直接返回
	return BaseDamage;
}

void ATeamsGameMode::PlayerEliminated(ABlasterCharacter* ElimmedCharacter, ABlasterPlayerController* VictimController,
	ABlasterPlayerController* AttackerController)
{
	// 先执行父类的方法，处理必要的被击杀逻辑
	Super::PlayerEliminated(ElimmedCharacter, VictimController, AttackerController);

	// 获取游戏状态
	// ABlasterGameState* BGameState = Cast<ABlasterGameState>(GetWorld()->GetAuthGameMode());
	ABlasterGameState* BGameState = Cast<ABlasterGameState>(UGameplayStatics::GetGameState(this));
	// 通过控制器获取该玩家的玩家状态。玩家控制器不存在时，设置为空指针
	ABlasterPlayerState* AttackerPlayerState = AttackerController ? AttackerController->GetPlayerState<ABlasterPlayerState>() : nullptr;
	// 确保以上两者获取并转换成功，根据玩家状态里存储的队伍属性，增加对应队伍的得分
	if(BGameState && AttackerPlayerState)
	{
		if(AttackerPlayerState->GetTeam() == ETeam::ET_BlueTeam)
		{
			BGameState->BlueTeamScores();
		}
		if(AttackerPlayerState->GetTeam() == ETeam::ET_RedTeam)
		{
			BGameState->RedTeamScores();
		}
	}
}

