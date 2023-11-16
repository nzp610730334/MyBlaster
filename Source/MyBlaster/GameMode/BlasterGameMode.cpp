// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterGameMode.h"

#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "MyBlaster/Character/BlasterCharacter.h"
#include "MyBlaster/GameState/BlasterGameState.h"
#include "MyBlaster/PlayerController/BlasterPlayerController.h"
#include "MyBlaster/PlayerState/BlasterPlayerState.h"

namespace MatchState
{
	// 初始化本项目自定义的游戏状态
	const FName Cooldown = FName("Cooldown");
}

ABlasterGameMode::ABlasterGameMode()
{
	bDelayedStart = true;
}

void ABlasterGameMode::BeginPlay()
{
	Super::BeginPlay();
	// 存储记录初始时间
	LevelStartingTime = GetWorld()->GetTimeSeconds();
}

void ABlasterGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	/*
	 * 游戏模式GameMode需要时刻监控游戏状态的变化并处理相应的逻辑。
	 */
	// 更新热身时间的倒计时的剩余时间，需要先判断当前是否是等待游戏开始的阶段。这是 从等待开始 阶段过渡到 游戏中 的阶段需要处理的逻辑。
	if(MatchState == MatchState::WaitingToStart)
	{
		// 热身倒计时的剩余时间 = 热身倒计时的总长度 - (当前的世界关卡时间 - 世界关卡生成时的时间)
		CountDownTime = WarmupTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
		// 当倒计时结束时
		if(CountDownTime <= 0.f)
		{
			// 游戏正式开始(将游戏状态设置为进行中)
			StartMatch();
		}
	}
	// 这是从 游戏中 的阶段过渡到 比赛冷却(赛后休息) 的阶段需要处理的逻辑。
	else if(MatchState == MatchState::InProgress)
	{
		// 计算一局比赛的剩余时间。 剩余时间 = 一局比赛总时长 - (当前服务器时间 - 当前世界关卡的生成时间 - 热身时长)
		CountDownTime = MatchTime - GetWorld()->GetTimeSeconds() + LevelStartingTime + WarmupTime;
		// 本局比赛时长已耗尽
		if(CountDownTime <= 0.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("SetMatchState(MatchState::Cooldown)开始调用之前！！！"));
			// 游戏状态设置为下一阶段。SetMatchState在被调用前，会先执行OnMatchStateSet()
			SetMatchState(MatchState::Cooldown);
			UE_LOG(LogTemp, Warning, TEXT("SetMatchState(MatchState::Cooldown)调用完毕之后！！！"));
		}
	}
	// 这是从 赛后休息 阶段过渡到 比赛重新开始 阶段。
	else if(MatchState == MatchState::Cooldown)
	{
		// 计算剩余时间。 剩余时间 = 赛后休息阶段总时长 - (当前服务器时间 - 当前世界关卡的生成时间 - 热身总时长 - 一局比赛总时长)
		CountDownTime = CooldownTime - GetWorld()->GetTimeSeconds() + LevelStartingTime + WarmupTime + MatchTime;
		// 剩余时间少于0，则重启游戏
		if(CountDownTime <= 0.f)
		{
			// 重启游戏。
			RestartGame();
		}
	}
} 

void ABlasterGameMode::OnMatchStateSet()
{
	UE_LOG(LogTemp, Warning, TEXT("即将开始调用Super::OnMatchStateSet()"));
	
	Super::OnMatchStateSet();

	/*
	 * 获取世界关卡里的所有玩家的控制器，循环遍历，并将他们的比赛状态MatchState设置为当前的比赛状态。
	 * GetWorld()->GetPlayerControllerIterator()获取的只是迭代器，需要使用*号解引用才能获得该玩家的控制器
	 */
	for(FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator();It;++It)
	{
		// 解引用并转换成本项目的玩家控制器
		ABlasterPlayerController* BlasterPlayer = Cast<ABlasterPlayerController>(*It);
		// 转换成功
		if(BlasterPlayer)
		{
			// 将该玩家的控制器的比赛状态设置为当前的比赛状态。
			BlasterPlayer->OnMatchStateSet(MatchState, bTeamsMatch);
		}
	}
}

void ABlasterGameMode::PlayerEliminated(ABlasterCharacter* ElimmedCharacter, ABlasterPlayerController* VictimController,
                                        ABlasterPlayerController* AttackerController)
{
	// 转换攻击者的玩家状态
	ABlasterPlayerState* AttackPlayerState = AttackerController ? Cast<ABlasterPlayerState>(AttackerController->PlayerState) : nullptr;
	// 转换被攻击者的玩家状态
	ABlasterPlayerState* VictimPlayerState = VictimController ? Cast<ABlasterPlayerState>(VictimController->PlayerState) : nullptr;
	// 直接获取游戏状态并转换
	ABlasterGameState* BlasterGameState = GetGameState<ABlasterGameState>();
	
	// 判断攻击者的AttackPlayerState存在且玩家不属于自杀才增加分数
	// if(AttackPlayerState && VictimPlayerState && AttackPlayerState != VictimPlayerState)
	if(AttackPlayerState && AttackPlayerState != VictimPlayerState && BlasterGameState)
	{
		/*
		 * 处理领先的玩家的皇冠特效的显示和销毁
		 */
		// 初始化新的数组，用于存储原本的所有领先者的玩家状态
		TArray<ABlasterPlayerState*> PlayersCurrentlyInTheLead;
		// 遍历当前所有领先者，将其add进新的数组中。auto表示变量自适应遍历的数组的元素的数据类型
		for(auto LeaderPlayer:BlasterGameState->TopScoringPlayers)
		{
			PlayersCurrentlyInTheLead.Add(LeaderPlayer);
		}
		
		// 累加攻击者的分数，这里硬编码击杀获取分值为1
		AttackPlayerState->AddToScore(1.f);
		// 更新最高分
		BlasterGameState->UpdateTopScore(AttackPlayerState);
		// 如果本函数此次处理的得分者属于领先者，则显示其皇冠特效
		if(BlasterGameState->TopScoringPlayers.Contains(AttackPlayerState))
		{
			ABlasterCharacter* Leader = Cast<ABlasterCharacter>(AttackPlayerState->GetPawn());
			if(Leader)
			{
				Leader->MulticastGainedTheLead();
			}
		}
		// 遍历本的所有领先者的玩家状态数组，数组元素不在新的领先者的玩家状态的数组中时，将玩家状态控制的角色的领先特效销毁
		for(int32 i = 0; i<PlayersCurrentlyInTheLead.Num(); i++)
		{
			// 新的领先者玩家状态数组中不包含该玩家状态
			if(!BlasterGameState->TopScoringPlayers.Contains(PlayersCurrentlyInTheLead[i]))
			{
				// 获取并转换成玩家角色
				ABlasterCharacter* Loser = Cast<ABlasterCharacter>(PlayersCurrentlyInTheLead[i]->GetPawn());
				// 转换成功
				if(Loser)
				{
					// 销毁该玩家角色的领先特效
					Loser->MulticastLostTheLead();
				}
			}
		}
	}
	
	// 判断被攻击者VictimPlayerState存在且玩家不属于自杀才累计玩家的死亡次数
	if(VictimPlayerState && AttackPlayerState != VictimPlayerState)
	{
		// 死亡次数Defeats类型为int32
		VictimPlayerState->AddToDefeats(1);
	}
	// 判断死亡的角色是否存在
	if(ElimmedCharacter)
	{
		// 执行该角色的死亡流程
		ElimmedCharacter->Elim(false);
	}

	// 通过迭代器IT遍历所有玩家控制器。注意读取迭代器的值需要使用型号*
	for(FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; It++)
	{
		// 转换该玩家控制器
		ABlasterPlayerController* BlasterPlayerController = Cast<ABlasterPlayerController>(*It);
		// 转换成功
		if(BlasterPlayerController && AttackPlayerState && VictimPlayerState)
		{
			// 调用该控制器，将对该控制器所在的客户端发送RPC请求通知，更新击杀通告。
			BlasterPlayerController->BroadcastElim(AttackPlayerState, VictimPlayerState);
		}
	}
}

void ABlasterGameMode::RequestRespawn(ACharacter* ElimmedCharacter, AController* ElimmedController)
{
	// 重生玩家角色之前，需要确保原来的角色被销毁
	// 判断参数是否有效
	if(ElimmedCharacter)
	{
		// 销毁之前，需要重置该pawn的属性(将其属性比如控制者指针指向空指针)
		ElimmedCharacter->Reset();
		// 销毁该pawn
		ElimmedCharacter->Destroy();
	}
	if(ElimmedController)
	{
		// 初始化新的数组，用于存放玩家出生点
		TArray<AActor*> PlayerStarts;
		// 获取当前世界关卡的所有玩家出生点，将它们的内存地址存入PlayerStarts
		UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);
		// 随机选取一个玩家出生点
		int32 Selection = FMath::RandRange(0, PlayerStarts.Num() - 1);
		// 在世界关卡中重新生成玩家角色。
		RestartPlayerAtPlayerStart(ElimmedController, PlayerStarts[Selection]);
		/*
		 *2023年6月2日13:20:36
		 * 注意：玩家角色之间如果有碰撞，那么有多个玩家短时间在同一个出生点生成，会导致后生成的玩家角色生成失败。
		 * 解决方法：
		 * 1.角色蓝图中Spawn Collision Handing Method设置为AdjustIfPossibleButAlwaysSpawn
		 * 2.或者在角色BlasterCharacter的构造函数中设置相应属性,代码如下
		 *	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		 */
	}
}

void ABlasterGameMode::PlayerLeftGame(class ABlasterPlayerState* PlayerLeaving)
{
	/*
	 * PlayerLeaving: 要离开游戏的玩家的玩家状态
	 * 将该玩家从分数排行榜中剔除（如果存在），然后销毁该玩家角色
	 */
	// 要离开游戏的玩家的状态为空，则直接返回
	if(PlayerLeaving == nullptr) return;
	// 获取游戏状态
	ABlasterGameState* BlasterGameState = GetGameState<ABlasterGameState>();
	// 如果要退出游戏的该玩家在分数排行榜中，则将其移除
	if(BlasterGameState && BlasterGameState->TopScoringPlayers.Contains(PlayerLeaving))
	{
		BlasterGameState->TopScoringPlayers.Remove(PlayerLeaving);
	}
	// 通过玩家状态获取该玩家控制的pawn，转换成玩家角色，
	ABlasterCharacter* CharacterLeaving = Cast<ABlasterCharacter>(PlayerLeaving->GetPawn());
	// 获取成功
	if(CharacterLeaving)
	{
		// 销毁该玩家角色。true表示该玩家是离开游戏，该销毁行为不会使该玩家角色重生。
		CharacterLeaving->Elim(true);
	}
}

float ABlasterGameMode::CalculateDamage(AController* Attacker, AController* Victim, float BaseDamage)
{
	// 当前游戏设计暂不处理此处逻辑，直接返回
	return BaseDamage;
}


