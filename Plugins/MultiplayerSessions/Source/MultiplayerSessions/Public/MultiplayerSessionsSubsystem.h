// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"

#include "MultiplayerSessionsSubsystem.generated.h"

/*
 * 声明自定义的委托
 */
// 仅有一个参数的动态多播委托。指定委托的名字、参数类型、参数名称
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnCreateSessionsComplete, bool, bWasSuccessful);
// 多播委托。两个参数
DECLARE_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnFindSessionsComplete, const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerOnJoinSessionComplete, EOnJoinSessionCompleteResult::Type Resutl);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnDestroySessionComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnStartSessionComplete, bool, bWasSuccessful);

/**
 * 多人会话子系统
 */
UCLASS()
class MULTIPLAYERSESSIONS_API UMultiplayerSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UMultiplayerSessionsSubsystem();

	/*
	 * To Handle session functionality.The menu will call these.
	 * 为了处理会话的建立,菜单类将会以下方法
	 */
	// 创建会话。需要指定最大连接数、MatchType
	void CreateSession(int32 NumPublicConnection, FString MatchType);
	// 查找会话。传参：最多返回多少个查找结果
	void FindSession(int32 MaxSearchResults);
	// 加入会话。传参：查抄到的会话结果
	void JoinSession(const FOnlineSessionSearchResult& SessionResult);
	// 销毁会话
	void DestroySession();
	// 启动会话
	void StartSession();

	/*
	 * 创建已经声明的自定义委托。菜单类将会绑定回调函数到该委托上。
	 */
	// 会话创建成功或失败后，调用该创建的自定义委托发送广播，所有绑定了本委托的回调函数都会被执行
	FMultiplayerOnCreateSessionsComplete MultiplayerOnCreateSessionsComplete;
	// 查抄会话委托
	FMultiplayerOnFindSessionsComplete MultiplayerOnFindSessionsComplete;
	// 加入会话委托
	FMultiplayerOnJoinSessionComplete MultiplayerOnJoinSessionComplete;
	// 销毁会话委托
	FMultiplayerOnDestroySessionComplete MultiplayerOnDestroySessionComplete;
	// 启动会话委托
	FMultiplayerOnStartSessionComplete MultiplayerOnStartSessionComplete;

	// 主菜单处设置的最大连接数
	int32 DesiredNumPublicConnections{};
	// 主菜单处设置的MatchType
	FString DesiredMatchType{};
	
protected:
	/*
	 * 各种委托被触发时，需要执行的回调函数.
	 * 这些回调函数需要的参数要符合它们要绑定的委托的要求
	 */
	// 会话创建完毕的回调函数
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	// 会话查找完毕的回调函数
	void OnFindSessionComplete(bool bWasSuccessful);
	// 会话加入完毕的回调函数
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	// 会话销毁完毕的回调函数
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	// 会话启动完毕的回调函数
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);
	
private:
	IOnlineSessionPtr SessionInterface;
	// 声明一个名为LastSessionSettings的共享指针，类型为FOnlineSessionSettings
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings;
	// 声明一个会话查找配置，需要转为共享指针。根据该配置查找会话。
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch;
	/*
	 * 完整的会话建立过程中，需要绑定的委托
	 */
	// 创建会话委托
	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
	// 创建会话的委托句柄。可以通过句柄将委托从委托列表中删除。
	FDelegateHandle CreateSessionCompleteDelegateHandle;
	// 查找会话委托
	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	// 查找会话的委托句柄
	FDelegateHandle FindSessionsCompleteDelegateHandle;
	// 加入会话委托
	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
	// 加入会话的委托句柄
	FDelegateHandle JoinSessionCompleteDelegateHandle;
	// 销毁会话委托
	FOnDestroySessionCompleteDelegate DestroySessionCompleteDelegate;
	// 销毁会话的委托句柄
	FDelegateHandle DestroySessionCompleteDelegateHandle;
	// 启动会话委托
	FOnStartSessionCompleteDelegate StartSessionCompleteDelegate;
	// 启动会话的委托句柄
	FDelegateHandle StartSessionCompleteDelegateHandle;

	/*
	 * 因为会话只能存在一个，所以在黄建会话的逻辑中，当会话已存在，请求创建会话时，会先销毁已存在的会话，然后再创建新的会话。
	 * 然而会话销毁要网络传输需要时间，代码不可能卡在那里等销毁成功与否的结果，这种情况需要异步处理。
	 * 因此引入销毁委托。
	 * 创建会话步骤中，需要销毁已存在的会话时，先设置一个tag，然后临时存储要创建的会话的参数。会话被销毁后触发委托，调用绑定的函数的步骤中，
	 * 利用临时存储的参数正式创建会话。
	 */
	// 创建会话过程中需要先销毁会话的tag。疑惑：为什么用这种初始化方式
	bool bCreateSessionOnDestroy{false};
	// 需要临时存储的参数-最大连接数
	int32 LastNumPublicConnections;
	// 需要临时存储的参数-MatchType
	FString LastMatchType;
};
