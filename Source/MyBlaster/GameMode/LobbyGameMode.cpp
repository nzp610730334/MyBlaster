// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyGameMode.h"

#include <string>

#include "MultiplayerSessionsSubsystem.h"
#include "GameFramework/GameStateBase.h"

using namespace std;

void ALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	// 获取当前关卡的玩家数组的长度
	int32 NumberOfPlayers = GameState.Get()->PlayerArray.Num();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1,
			15.f,
			FColor::Blue,
			FString::Printf(TEXT("the number of Current Players is: %d"), NumberOfPlayers)); 
	}

	/*
	 * 从多人会话子系统中获取已建立的会话的MatchType
	 */
	// 获取游戏实例
	UGameInstance* GameInstance = GetGameInstance();
	if(GameInstance)
	{
		// 获取游戏实例子系统
        UMultiplayerSessionsSubsystem* SubSystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
		// 断言SubSystem必定存在，失败则停止执行
		check(SubSystem);
		// 当玩家数量到达最大连接数时
		if(NumberOfPlayers == SubSystem->DesiredNumPublicConnections)
		{
			UWorld* World = GetWorld();
			if(World)
			{
				// 使用无缝切换的方式切换关卡
				bUseSeamlessTravel = true;
				// 开始切换关卡
				UE_LOG(LogTemp,Log,TEXT("Start to Travel!Start to Travel!Start to Travel!Start to Travel!Start to Travel!Start to Travel!"));

				/*
				 * 该部分if...else 代码是否可以重构为策略模式
				 */
				// 获取MatchType
				FString MatchType = SubSystem->DesiredMatchType;
				// 指定一个关卡作为监听服务器打开，以供其他玩家客户端连接
				if(MatchType == "FreeForAll")
				{
					World->ServerTravel(FString("/Game/Maps/BlasterMap?listen"));
				}
				else if(MatchType == "Teams")
				{
					World->ServerTravel(FString("/Game/Maps/Teams?listen"));
				}
				else if(MatchType == "CaptureTheFlag")
				{
					World->ServerTravel(FString("/Game/Maps/CaptureTheFlag?listen"));
				}
			}
		}
	}
	
	
	/* P221 之前的切换地图代码
	if(NumberOfPlayers == 2)
	{
		UWorld* World = GetWorld();
		if(World)
		{
			// 使用无缝切换的方式切换关卡
			bUseSeamlessTravel = true;

			// 开始切换关卡
			UE_LOG(LogTemp,Log,TEXT("Start to Travel!Start to Travel!Start to Travel!Start to Travel!Start to Travel!Start to Travel!"));
			
			// 指定一个关卡作为监听服务器打开，以供其他玩家客户端连接
			World->ServerTravel(FString("/Game/Maps/BlasterMap?listen"));
			// World->ServerTravel(FString("/Game/Maps/ThirdPersonMap?listen"));
		}
	}
	*/
	// UWorld* World = GetWorld();
	// if(World)
	// {
	// 	// 使用无缝切换的方式切换关卡
	// 	bUseSeamlessTravel = true;
	// 	// 指定一个关卡作为监听服务器打开，以供其他玩家客户端连接
	// 	// World->ServerTravel(FString("/Game/Maps/BlasterMap?listen"));
	// 	World->ServerTravel(FString("/Game/Maps/BlasterMap?listen"));
	// }

	// if (GEngine)
	// {
	// 	GEngine->AddOnScreenDebugMessage(-1,
	// 		15.f,
	// 		FColor::Blue,
	// 		FString::Printf(TEXT("---------the number of Current Players is not enough!"), "")); 
	// }
}


