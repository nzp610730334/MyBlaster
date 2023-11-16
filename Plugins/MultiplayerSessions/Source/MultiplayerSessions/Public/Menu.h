// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Menu.generated.h"

/**
 * 
 */
UCLASS()
class MULTIPLAYERSESSIONS_API UMenu : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
	void MenuSetup(int32 NumberOfPublicConnections = 4, FString TypeOfMatch = FString(TEXT("FreeForAll")), FString LobbyPath = FString(TEXT("/Game/Maps/Lobby")));

protected:
	// 进行一系列初始化操作，比如绑定按钮事件。注意该函数在底层被调用，无需本类手动调用
	virtual bool Initialize() override;
	// 该函数将在本世界关卡被销毁时执行。ServerTravel切换新的地图关卡后，旧的地图关卡将被移除
	virtual void OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld) override;
	// 备注：ue5.1后可能没有OnLevelRemovedFromWorld了，使用virtual void NativeDestruct() override;代替

	/*
	 * 回调函数，要绑定到多人会话子系统中的自定义委托上。
	 * 参数要符合自定义的委托。作为动态多播委托的回调函数需要使用UFUNCTION()标记。普通的多播委托则不用
	 */
	UFUNCTION()
	void OnCreateSession(bool bWasSuccessful);

	void OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);
	void OnJoinSession(EOnJoinSessionCompleteResult::Type Result);
	UFUNCTION()
	void OnDestroySession(bool bWasSuccessful);
	UFUNCTION()
	void OnStartSession(bool bWasSuccessful);
	
private:
	// Host按钮，对应用户控件里的HostButton
	UPROPERTY(meta = (BindWidget))
	class UButton* HostButton;
	// Join按钮，对应用户控件里的JoinButton
	UPROPERTY(meta = (BindWidget))
	UButton* JoinButton;

	UFUNCTION()
	void HostButtonClicked();
	UFUNCTION()
	void JoinButtonClicked();
	
	// 关闭菜单然后恢复玩家的游戏控制
	void MenuTearDown();
	// 多人游戏会话在线子系统
	class UMultiplayerSessionsSubsystem* MultiplayerSessionsSubsystem;

	// 声明并初始化最大连接数
	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess))
	int32 NumPublicConnections{4};
	// 声明并初始化MatchType。因为是私有变量，因此需要设置私有可访问
	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess))
	FString MatchType{TEXT("FresForAll")};
	// 初始化新的FString作为大厅地图的路径
	FString PathToLobby{TEXT("")};
};
