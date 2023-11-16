// Fill out your copyright notice in the Description page of Project Settings.


#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"

UMultiplayerSessionsSubsystem::UMultiplayerSessionsSubsystem():
	// 这里通过使用 初始化列表 来绑定委托和回调函数
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete)),
	FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionComplete)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete)),
	DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete)),
	StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionComplete))
{
	// 获取在线子系统
	IOnlineSubsystem* SubSystem = IOnlineSubsystem::Get();
	// 获取成功
	if(SubSystem)
	{
		// 获取在线子系统的会话接口，存储在SessionInterface中
		SessionInterface = SubSystem->GetSessionInterface();
	}
}

void UMultiplayerSessionsSubsystem::CreateSession(int32 NumPublicConnection, FString MatchType)
{
	DesiredNumPublicConnections = NumPublicConnection;
	DesiredMatchType = MatchType;
	// 多人会话接口不存在时，直接返回
	if(!SessionInterface.IsValid())
	{
		return;
	}
	/*
	 * 会话已经建立时，需要先销毁
	 */
	// 尝试获取会话，成功获取则需要销毁该会话.(NAME_GameSession是一个全局变量，会话建立后，会存储会话的名字？)
	auto ExistingSession = SessionInterface->GetSessionSettings(NAME_GameSession);
	if(ExistingSession != nullptr)
	{
		// 需要销毁已存在的会话，变更tag
		bCreateSessionOnDestroy = true;
		// 临时存储最大连接数
		LastNumPublicConnections = NumPublicConnection;
		// 临时存储MatchType
		LastMatchType = MatchType;
		// 调用销毁委托的函数
		DestroySession();
		// 疑惑：这里没有直接return返回，则后续创建会话的代码还会执行，加上销毁委托绑定的回调函数中创建的会话，那么岂不是进行了两次创建会话？虽然第一次可能会失败
		// 补上return后经过测试一切正常。不再出现预先写好的错误提示
		return;
	}
	// 将委托添加到委托列表。存储返回的句柄，用于后续操作已经添加的该委托
	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	/*
	 * 配置要创建的会话的属性
	 */
	// 实例化新的会话配置，转换为共享指针，因为头文件中LastSessionSettings已经声明为共享指针
	LastSessionSettings = MakeShareable(new FOnlineSessionSettings);
	// 获取在线子系统，如果是虚幻引擎默认的在线子系统则名为NULL，此时需要开启LAN局域网匹配。使用Steam平台在线子系统时，返回的是名字是steam
	LastSessionSettings->bIsLANMatch = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;
	// 最多连接数
	LastSessionSettings->NumPublicConnections = NumPublicConnection;
	// 允许会话进行中其他玩家加入
	LastSessionSettings->bAllowJoinInProgress = true;
	// 通过验证的才能加入？
	LastSessionSettings->bAllowJoinViaPresence = true;
	// 是否使用的平台（比如虚幻、steam等）展示会话，以供其他玩家连接
	LastSessionSettings->bShouldAdvertise = true;
	// 是否显示用户状态信息
	LastSessionSettings->bUsesPresence = true;
	// 设置键值对
	LastSessionSettings->Set(FName("MatchType"), MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	// 如果平台支持，是否启用大厅API
	LastSessionSettings->bUseLobbiesIfAvailable = true;
	// 用于防止不同的构建在搜索过程中相互查看 ?
	LastSessionSettings->BuildUniqueId = true;

	// 从控制器中获取玩家
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	// 正式创建会话。传入本地机器的玩家的首选的唯一网络Id、NAME_GameSession存储要创建的会话的名字、会话的配置（要求是一个引用，LastSessionSettings是一个共享指针，因此需要*号解引用）
	bool bCreateSessionSuccessful = SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings);
	// 会话创建失败，则需要清除已经添加到委托列表的委托
	if(!bCreateSessionSuccessful)
	{
		// 传入委托的句柄，清除已经添加的委托
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		// 自定义的委托发送广播，告知所有绑定了该委托的函数(其实就是菜单类)：会话创建失败bWasSuccessful=false
		MultiplayerOnCreateSessionsComplete.Broadcast(false);
		/*
		 *  因为绑定了函数OnCreateSessionComplete到原生的委托类FOnCreateSessionCompleteDelegate的对象CreateSessionCompleteDelegate上，
		 *  因此不管成功与否，OnCreateSessionComplete都会被执行，在OnCreateSessionComplete处理自定义广播的成功消息。
		 *  疑惑：OnCreateSessionComplete中已经广播了一次成功与否的结果，为什么这里还要广播一次失败的？
		 */
	}
	
}

void UMultiplayerSessionsSubsystem::FindSession(int32 MaxSearchResults)
{
	// 多人会话接口不存在时，直接返回
	if(!SessionInterface.IsValid())
	{
		return;
	}
	// 将查找会话的委托添加到委托列表，并将该操作返回的句柄存储
	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);
	// 实例化新的查找设置，并将其转换为共享指针
	LastSessionSearch = MakeShareable(new FOnlineSessionSearch);
	// 设置最大连接数
	LastSessionSearch->MaxSearchResults = MaxSearchResults;
	// 是否开启局域网连接。取决于当前使用的多人会话在线子系统是UE默认的NULL还是别的比如steam的
	LastSessionSearch->bIsLanQuery = IOnlineSubsystem::Get()->GetSubsystemName() == "Null" ? true : false;
	// 设置查找键值对。关系为Equals
	LastSessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
	// 获取本地机器的玩家
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	// 正式开始查找会话。注意FindSessions第二个参数要求是一个引用，LastSessionSearch是一个共享指针，需要转成共享引用
	bool bFindSessionSuccessful = SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef());
	// 查找失败时
	if(!bFindSessionSuccessful)
	{
		// 通过之前保存的查找委托的句柄，将查找委托从委托列表中移除
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		// 广播查找会话失败的消息。传入空的查找结果的数组以及失败tag
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
	}
}

void UMultiplayerSessionsSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult)
{
	// 确保会话接口存在
	if(!SessionInterface.IsValid())
	{
		// 需要广播失败的消息后再return返回
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}
	// 将加入会话委托添加到委托列表，存储其返回的句柄
	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);
	
	// 获取本地机器的玩家
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	// 正式加入会话.(疑惑：NAME_GameSession的作用和意义？)
	bool bJoinSuccessful = SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionResult);
	// 加入会话失败
	if(!bJoinSuccessful)
	{
		// 清除之前添加到委托列表的的委托
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		// 广播加入会话失败的消息
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
	}
}

void UMultiplayerSessionsSubsystem::DestroySession()
{
	// 确保会话接口存在,否则广播销毁失败的消息并直接返回
	if(!SessionInterface.IsValid())
	{
		MultiplayerOnDestroySessionComplete.Broadcast(false);
		return;
	}
	// 将销毁委托添加到委托列表
	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);
	// 正式销毁已存在的会话，并判断销毁结果
	if(!SessionInterface->DestroySession(NAME_GameSession))
	{
		// 销毁失败，需要清除添加到委托列表里面的销毁委托
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		// 广播销毁失败的消息
		MultiplayerOnDestroySessionComplete.Broadcast(false);
	}
	/* 疑惑思考
	 * 关于为什么在回调函数OnDestroySessionComplete中清除了添加到委托列表里面的销毁委托和 广播销毁失败的消息，在这里还需要做一遍。
	 * 本身DestroySession发送请求的行为可能是失败，因此需要在这里做一遍。
	 * 在回调函数OnDestroySessionComplete中的处理是对应DestroySession这一个请求的处理结果。
	 */
}

void UMultiplayerSessionsSubsystem::StartSession()
{
}

void UMultiplayerSessionsSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	// 确保会话接口存在
	if(SessionInterface)
	{
		// 将创建会话的委托从委托列表中移除
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}
	// 自定义委托广播创建会话的结果
	MultiplayerOnCreateSessionsComplete.Broadcast(bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnFindSessionComplete(bool bWasSuccessful)
{
	// 确保多人会话接口不存在时，使用句柄清除之前添加到委托列表里面的查找会话委托
	if(SessionInterface)
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	}
	// 获取的查找结果为0，广播查找失败的消息
	if(LastSessionSearch->SearchResults.Num() <= 0)
	{
		// 广播消息，参数指定为空数组和false，因为查找成功但结果为空bWasSuccessful也为true
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		// 本次处理结束，直接返回
		return;
	}
	// 上面的判断完毕，自定义的会话查找委托开始广播消息。传入查找结果（数组），和成功与否的tag
	MultiplayerOnFindSessionsComplete.Broadcast(LastSessionSearch->SearchResults, bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	// 确保多人会话接口存在
	if(SessionInterface.IsValid())
	{
		// 清除添加到委托列表的委托
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}
	
	// 广播加入会话的结果
	MultiplayerOnJoinSessionComplete.Broadcast(Result);
}

void UMultiplayerSessionsSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if(SessionInterface)
	{
		// 清除添加到委托列表的销毁委托
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}
	// 销毁成功且创建会话过程中需要先销毁会话的tag为真时
	if(bWasSuccessful && bCreateSessionOnDestroy)
	{
		// 重置tag为false
		bCreateSessionOnDestroy = false;
		// 再次创建会话
		CreateSession(LastNumPublicConnections, LastMatchType);
	}
	// 广播消息-会话销毁成功
	MultiplayerOnDestroySessionComplete.Broadcast(bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
}
