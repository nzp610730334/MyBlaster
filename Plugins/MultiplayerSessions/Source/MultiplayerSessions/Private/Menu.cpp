// Fill out your copyright notice in the Description page of Project Settings.


#include "Menu.h"

#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "Components/Button.h"

void UMenu::MenuSetup(int32 NumberOfPublicConnections, FString TypeOfMatch, FString LobbyPath)
{
	// 拼接路径。LobbyPath是C风格的字符串(数组)，是数组首个元素的指针，需要星号*解引用
	PathToLobby = FString::Printf(TEXT("%s?listen"), *LobbyPath);
	// 存储参数内容
	NumPublicConnections = NumberOfPublicConnections;
	MatchType = TypeOfMatch;
	
	// 将菜单添加到视口上
	AddToViewport();
	// 设置可见
	SetVisibility(ESlateVisibility::Visible);
	// 将此标志设置为true，允许此小部件在单击或导航到时接受焦点。(鼠标光脚移动到菜单上？)
	bIsFocusable = true;

	// 获取世界关卡
	UWorld* World = GetWorld();
	// 获取成功
	if(World)
	{
		// 获取玩家控制器
		APlayerController* PlayerController = World->GetFirstPlayerController();
		// 成功获取。则变更该玩家控制器的输入方式。
		if(PlayerController)
		{
			// 初始化新的输入模式
			FInputModeUIOnly InputModeData;
			// 该输入模式聚焦的控件。TakeWidget():获取底层的slate小部件，或者在它不存在的情况下构造它。
			InputModeData.SetWidgetToFocus(TakeWidget());
			// 设置鼠标在视口中的行为，DoNotLock允许移到视口之外
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			// 配置玩家控制器的输入模式
			PlayerController->SetInputMode(InputModeData);
			// 显示鼠标光标
			PlayerController->SetShowMouseCursor(true);
		}
	}
	
	/*
	 * 游戏实例从游戏开始存续到游戏结束，游戏实例被创建后，游戏实例子系统也被创建。
	 * 多人会话子系统的父类即是游戏实例子系统
	 */
	// 获取游戏实例
	UGameInstance* GameInstance = GetGameInstance();
	if(GameInstance)
	{
		// 获取游戏实例子系统,并转换为多人会话子系统
		MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
	}

	// 确保多人在线子系统存在，将各阶段的回调函数绑定到各阶段的委托上。注意区分动态多播委托和多播委托的区别
	if(MultiplayerSessionsSubsystem)
	{
		// 绑定创建会话完毕的回调函数到多人会话子系统中自定义的动态多播委托上
		MultiplayerSessionsSubsystem->MultiplayerOnCreateSessionsComplete.AddDynamic(this, &ThisClass::OnCreateSession);
		// 绑定查找会话完毕的回调函数到多人会话子系统中自定义的多播委托上
		MultiplayerSessionsSubsystem->MultiplayerOnFindSessionsComplete.AddUObject(this, &ThisClass::OnFindSessions);
		// 绑定回调函数加入会话委托
		MultiplayerSessionsSubsystem->MultiplayerOnJoinSessionComplete.AddUObject(this, &ThisClass::OnJoinSession);
		// 绑定回调函数销毁会话委托
		MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.AddDynamic(this, &ThisClass::OnDestroySession);
		// 绑定回调函数气筒会话委托
		MultiplayerSessionsSubsystem->MultiplayerOnStartSessionComplete.AddDynamic(this, &ThisClass::OnStartSession);
	}
}

bool UMenu::Initialize()
{
	// 父类返回了false的话，本类也返回false
	if(!Super::Initialize())
	{
		return false;
	}

	/*
	 * 绑定按钮
	 */
	// 确保按钮存在
	if(HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &UMenu::HostButtonClicked);
	}
	if(JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &UMenu::JoinButtonClicked);
	}
	
	return true;
}

void UMenu::OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld)
{
	// 关闭菜单然后恢复玩家的游戏控制
	MenuTearDown();
	// 涉及销毁的操作，父类的方法应该在后面执行，否则可能导致要操作的变量已经被父类方法销毁了
	Super::OnLevelRemovedFromWorld(InLevel, InWorld);
}

void UMenu::OnCreateSession(bool bWasSuccessful)
{
	// 会话创建成功，则打印消息并切换地图
	if(bWasSuccessful)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Yellow,
			FString(TEXT("Session Created Successfully!"))
			);
		
		UWorld* World = GetWorld();
		if(World)
		{
			// 地图关卡的实际路径 J:/Unrealprojects5/MenuSystem/Content/Maps/Lobby.umap
			// 以listen监听服务器的身份切换指定路径下的地图关卡Lobby
			// World->ServerTravel(FString("/Game/Maps/Lobby?listen"));
			World->ServerTravel(FString(PathToLobby));
		}
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Red,
			FString(TEXT("Session Created Failed!"))
			);
		// 创建会话失败，重新启用按钮
		HostButton->SetIsEnabled(true);
	}
}

void UMenu::OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful)
{
	// 多人在线子系统不存在时，直接返回
	if(MultiplayerSessionsSubsystem == nullptr)
	{
		return;
	}
	// 遍历会话查找结果，选择符合条件的调用多人在线子系统的加入会话函数
	for(auto& Result : SessionResults)
	{
		// 初始化空的FString
		FString StringValue;
		// 获取查找结果的键MatchType的值,存储在StringValue中
		Result.Session.SessionSettings.Get(FName("MatchType"), StringValue);
		if(GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				60.f,
				FColor::Black,
				FString::Printf(TEXT("已选择的Match Type: %s"), *MatchType)
				);
		}
		// 结果与预设结果一致
		if(StringValue == MatchType)
		{
			// 调用多人子系统的函数加入会话
			MultiplayerSessionsSubsystem->JoinSession(Result);
			// 中断遍历，直接返回
			return;
		}
	}
	// 查找会话失败或者查找到的会话为0时，重新启用按钮
	if(!bWasSuccessful || SessionResults.Num() <= 0)
	{
		JoinButton->SetIsEnabled(true);
	}
}

void UMenu::OnJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
	// 获取在线子系统
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	// 获取成功
	if(Subsystem)
	{
		// 获取会话接口
		IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
		// 初始化新的FString用于接收已建立的会话的主机的地址
		FString Address;
		// 返回用于加入比赛的特定于平台的连接信息。(从联接完成的委托调用GetResolvedConnectString)
		SessionInterface->GetResolvedConnectString(NAME_GameSession, Address);
		// 获取本地机器的玩家控制器。因为本类是菜单不是pawn或character，所以从游戏实例中获取
		APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
		if(PlayerController)
		{
			// 正式切换地图
			PlayerController->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
		}
	}
	// 会话存在，但有用户在没有销毁会话的前提下就退出了会话，那么会话存在但后续用户会加入失败，这种情况也需要重新启用按钮
	if(Result != EOnJoinSessionCompleteResult::Success)
	{
		JoinButton->SetIsEnabled(true);
	}
}

void UMenu::OnDestroySession(bool bWasSuccessful)
{
}

void UMenu::OnStartSession(bool bWasSuccessful)
{
}

void UMenu::HostButtonClicked()
{
	/*
	 * 防止用户多次点击，禁用按钮。需要在回调函数处重新启用按钮。
	 * 疑惑：是否应该禁用全部业务逻辑冲突按钮，比如避免用户在点击创建会话的时候马上去点击加入会话按钮？或许应该整合成一个函数，哪些按钮需要一起禁用，一起重新启用
	 * JoinButton->SetIsEnabled(false);
	 */ 
	HostButton->SetIsEnabled(false);
	if(GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Yellow,
			// FString::Printf(TEXT("Host Button Clicked!"))
			FString(TEXT("Host Button Clicked!"))	// 没有使用占位符时，无需使用Printf格式化
			);
	}
	if(GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			60.f,
			FColor::Black,
			FString::Printf(TEXT("Host按钮已按下，已选择的Match Type: %s"), *MatchType)
			);
	}
	
	// 确保多人会话子系统存在
	if(MultiplayerSessionsSubsystem)
	{
		// 调用多人会话子系统创建会话。（这里不该使用硬编码，后续需要优化，MenuSetup接收参数传给CreateSession）
		MultiplayerSessionsSubsystem->CreateSession(NumPublicConnections, MatchType);
	}
}

void UMenu::JoinButtonClicked()
{
	if(GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			60.f,
			FColor::Black,
			FString::Printf(TEXT("加入按钮已按下，已选择的Match Type: %s"), *MatchType)
			);
	}
	
	/*
	 * 防止用户多次点击，禁用按钮。需要在回调函数处重新启用按钮。
	 */ 
	// HostButton->SetIsEnabled(false);
	JoinButton->SetIsEnabled(false);
	if(GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Yellow,
			FString(TEXT("Join Button Clicked!"))
			);
	}
	// 确保多人会话在线子系统的存在
	if(MultiplayerSessionsSubsystem)
	{
		// 调用其查找会话的函数。传入最大查找数
		MultiplayerSessionsSubsystem->FindSession(10000);
	}
}

void UMenu::MenuTearDown()
{
	// 将菜单控件从父级移除
	RemoveFromParent();
	/*
	 * 恢复玩家的输入模式为菜单打开前的状态
	 */
	// 获取玩家控制器
	UWorld* World = GetWorld();
	if(World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if(PlayerController)
		{
			// 创建新的输入模式。FInputModeGameOnly仅游戏，注意和FInputModeUIOnly的区别
			FInputModeGameOnly InputModeData;
			// 设置输入模式
			PlayerController->SetInputMode(InputModeData);
			// 不要忘了重新隐藏鼠标光标
			PlayerController->SetShowMouseCursor(false);
		}
	}
}
