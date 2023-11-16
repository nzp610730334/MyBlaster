// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterPlayerState.h"

#include "MyBlaster/Character/BlasterCharacter.h"
#include "MyBlaster/HUD/BlasterHUD.h"
#include "MyBlaster/PlayerController/BlasterPlayerController.h"
#include "Net/UnrealNetwork.h"

void ABlasterPlayerState::AddToScore(float ScoreAmount)
{
	/*
	 * ScoreAmount:变动的分数
	 */
	// 对分数作累加操作
	// Score += ScoreAmount;	// 直接访问Score修改可能导致其他副作用，应该使用UE提供的原生Set方法
	// SetScore(Score + ScoreAmount);		// 未来引擎版本Score可能会被私有化，应该改为使用Get获取
	SetScore(GetScore() + ScoreAmount);
	/*
	 * 更新玩家UI上的分数信息。这里更新的是服务器机器上的角色的信息，本函数只会被服务器机器调用，
	 * 因为函数OnRep_Score之类的函数需要对应的属性被服务器网络更新时客户端才会执行，
	 * 因此服务器上不会执行函数OnRep_Score更新玩家UI信息，涉及这些信息的更新操作需要额外在服务器上使用专门的函数即本函数处理.
	 */ 
	Character = Character == nullptr ? Cast<ABlasterCharacter>(GetPawn()): Character;
	if(Character)
	{
		Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
		if(Controller)
		{
			Controller->SetHUDScore(GetScore());
		}
	}
}

void ABlasterPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// 需要网络复制的属性都需要在此注册
	// DOREPLIFETIME无条件复制
	DOREPLIFETIME(ABlasterPlayerState, Defeats);
	DOREPLIFETIME(ABlasterPlayerState, Team);
}

void ABlasterPlayerState::OnRep_Score()
{
	Super::OnRep_Score();

	// 通过玩家状态获取受控角色pawn，转换成玩家角色。使用三元运算符转换，为空时转换，不为空则使用旧值，达到不用每次都转换的目的。
	Character = Character == nullptr ? Cast<ABlasterCharacter>(GetPawn()) : Character;
	// 转换成功时
	if(Character)
	{
		// 注释同上。无法获取(获取失败)是，说明本对象不是受控对象（即属于模拟代理，属于其他玩家，即不必更新模拟代理的HUD）
		Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
		if(Controller)
		{
			/*
			 * 更新玩家UI信息。注意：本函数OnRep_Score之类的函数需要对应的属性被服务器网络更新时客户端才会执行，
			 * 因此服务器上不会执行本函数，涉及这些属性的操作需要额外函数在服务器上处理.
			 */ 
			Controller->SetHUDScore(GetScore());
		}
	}
}


void ABlasterPlayerState::AddToDefeats(int32 DefeatsAmount)
{
	// 累加死亡次数
	Defeats += DefeatsAmount;
	// 获取当前玩家状态控制的pawn并转换为玩家角色
	Character = Character == nullptr ? Cast<ABlasterCharacter>(GetPawn()) : Character;
	// 转换成功
	if(Character)
	{
		// 获取玩家角色的控制器并转换为玩家控制器
		Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
		// 转换成功
		if(Controller)
		{
			// 设置角色死亡信息显示
			Controller->SetHUDDefeats(Defeats);
		}
	}
}

void ABlasterPlayerState::OnRep_Defeats()
{
	// 更新玩家客户端机器的角色的死亡次数信息
	Character = Character == nullptr ? Cast<ABlasterCharacter>(GetPawn()) : Character;
	if(Character)
	{
		Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
		if(Controller)
		{
			Controller->SetHUDDefeats(Defeats);
		}
	}
}

void ABlasterPlayerState::SetTeam(ETeam TeamToSet)
{
	// 设置队伍
	Team = TeamToSet;

	// 获取本控制器的pawn，将其转换为玩家角色
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn());
	// 转换成功
	if(BlasterCharacter)
	{
		// 设置该玩家的队伍材质
		BlasterCharacter->SetTeamColor(Team);
		UE_LOG(LogTemp, Warning, TEXT("玩家 %d 队伍颜色设置完毕！"), GetPlayerId());
	}
}

void ABlasterPlayerState::OnRep_Team()
{
	/*
	 * 通常情况下，客户端上的变量Team被网络更新后，本函数会被执行，
	 * 在本函数这里设置玩家队伍相关的材质
	 */
	// 获取本控制器的pawn，将其转换为玩家角色
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn());
	// 转换成功
	if(BlasterCharacter)
	{
		// 设置该玩家的队伍材质
		BlasterCharacter->SetTeamColor(Team);
	}
}



