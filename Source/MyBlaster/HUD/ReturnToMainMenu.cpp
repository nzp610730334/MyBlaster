// Fill out your copyright notice in the Description page of Project Settings.


#include "ReturnToMainMenu.h"

#include "Components/Button.h"
#include "MultiplayerSessionsSubsystem.h"
#include "GameFramework/GameModeBase.h"
#include "MyBlaster/Character/BlasterCharacter.h"
#include "MyBlaster/PlayerController/BlasterPlayerController.h"

/*
 * 退出游戏的业务逻辑说明：
 * MenuSetUp()菜单显示到玩家视口上-退出多人会话的逻辑绑定到按钮上-玩家点击按钮触发ReturnButtonClicked-调用多人会话子系统销毁会话-客户端等待销毁结果-
 * 销毁成功，多人会话子系统的销毁完成的委托发送广播-先前绑定了多人会话子系统的销毁完成的委托的函数OnDestroySession开始执行，进行会话销毁后的收尾工作，如从视口中移除菜单MenuTearDown
 */

void UReturnToMainMenu::MenuSetUp()
{
	// 将菜单显示到玩家屏幕上
	AddToViewport();
	// 设置菜单显示的可见类型
	SetVisibility(ESlateVisibility::Visible);
	// 允许此小部件在单击或导航到时接受焦点。
	bIsFocusable = true;

	// 获取世界关卡
	UWorld* World = GetWorld();
	if(World)
	{
		/*
		 * 本函数将来仅在客户端上执行，每个客户端的世界关卡都只有该机器上的玩家的控制器，因此获取的都只是客户端自己的玩家。
		 */
		PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;
		// 获取成功
		if(PlayerController)
		{
			// 初始化新的输入模式(用于设置输入模式的数据结构，该模式允许用户界面对用户输入做出响应，如果用户界面无法处理，玩家输入/玩家控制器将获得机会。)
			FInputModeGameAndUI InputModeData;
			/*	TakeWidget()
			 * 获取底层的slate小部件，或者在它不存在的情况下构造它。如果您希望替换构建的slate小部件，请查找RebuildWidget。
			 * 对于非常特殊的情况，您实际上需要更改构建的用户小部件的GC根小部件-您需要使用
			 * TakeDerivedWidget-您还必须注意在调用TakeDeriedWidget之前不要调用TakeWidget，
			 * 因为这会在构建的小部件周围放置错误的预期包装器。
			 */
			InputModeData.SetWidgetToFocus(TakeWidget());
			// 设置玩家控制器的输入模式
			PlayerController->SetInputMode(InputModeData);
			// 显示鼠标光标？
			PlayerController->SetShowMouseCursor(true);
		}
	}

	/*
	 * 如果按钮组件存在，则绑定本类的按钮点击方法到该按钮组件的点击事件上。
	 * 重复绑定会报错，红色错误信息打印在日志上。因此需要判断未绑定时，才进行绑定。
	 * 菜单销毁时，也需要解除绑定
	 */ 
	if(ReturnButton && !ReturnButton->OnClicked.IsBound())
	{
		// 动态绑定
		ReturnButton->OnClicked.AddDynamic(this, &UReturnToMainMenu::ReturnButtonClicked);
	}
	
	/*
	 *	将按钮的方法绑定到子系统的完成销毁的委托上。
	 */
	// 获取游戏实例
	UGameInstance* GameInstance = GetGameInstance();
	if(GameInstance)
	{
		// 获取游戏实例的子系统并转换成多人会话子系统
		// MultiplayerSessionsSubsystem = Cast<UMultiplayerSessionsSubsystem>(GameInstance->GetSubsystem);
		MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
		if(MultiplayerSessionsSubsystem)
		{
			// 绑定函数到多人会话子系统的委托上
			MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.AddDynamic(this, &UReturnToMainMenu::OnDestroySession);
		}
	}
}

void UReturnToMainMenu::MenuTearDown()
{
	// 将菜单从玩家视口移除
	RemoveFromParent();
	/*
	 * 将玩家控制器的控制恢复为游戏时应有的状态，如隐藏鼠标等
	 */
	// 获取世界关卡
	UWorld* World = GetWorld();
	// 获取成功
	if(World)
	{
		// 获取玩家控制器
		PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;
		if(PlayerController)
		{
			// 初始化新的输入模式(用于设置仅允许玩家输入/玩家控制器响应用户输入的输入模式的数据结构。)
            FInputModeGameOnly InputMode;
            // 设置玩家控制器的输入模式
			PlayerController->SetInputMode(InputMode);
			// 隐藏鼠标光标
			PlayerController->SetShowMouseCursor(false);
		}
	}
	// 已绑定按键委托的情况下，需要解除绑定。后续再次打开菜单时，再绑定
	if(ReturnButton && ReturnButton->OnClicked.IsBound())
	{
		// 动态解除绑定
		ReturnButton->OnClicked.RemoveDynamic(this, &UReturnToMainMenu::ReturnButtonClicked);
	}

	// 已绑定退出多人会话的事件委托的情况下，需要解除绑定。后续再次打开菜单时，再绑定
	if(MultiplayerSessionsSubsystem && MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.IsBound())
	{
		// 动态解除绑定
		MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.RemoveDynamic(this, &UReturnToMainMenu::OnDestroySession);
	}
}

bool UReturnToMainMenu::Initialize()
{
	// 父类的初始化方法执行返回false时，这里也直接return false
	if(!Super::Initialize())
	{
		return false;
	}
	return true;
}

void UReturnToMainMenu::ReturnButtonClicked()
{
	/*
	 * 存在网络延迟，如果本地机器是客户端，则服务器不能马上响应客户端的请求。
	 * 为了防止客户端多次发送网络请求，需要将按钮暂时禁用，等待服务器的销毁结果，在OnDestroySession中再决定是否重新启用
	 */
	// 暂时禁用按钮
	ReturnButton->SetIsEnabled(false);
	// 获取世界关卡
	UWorld* World = GetWorld();
	// 通过世界关卡获取玩家控制器，并转换为ABlasterPlayerController
	// ABlasterPlayerController* BlasterPlayerController = World->GetFirstPlayerController<ABlasterPlayerController>();
	PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;		// 无需Cast成ABlasterPlayerController，因为在这里没用上，Cast浪费开销
	// 获取成功
	if(PlayerController)
	{
		// 通过玩家控制器获取控制的玩家角色
		// ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(BlasterPlayerController->GetCharacter());
		ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(PlayerController->GetPawn());
		// 转换成功
		if(BlasterCharacter)
		{
			// 调用玩家角色里面的退出游戏RPC请求
			BlasterCharacter->ServerLeaveGame();
			// 动态绑定玩家机器断开多人会话的函数到退出委托的实例上。
			BlasterCharacter->OnLeftGame.AddDynamic(this, &UReturnToMainMenu::OnPlayerLeftGame);
		}
		// 当玩家死亡，因为正在重生中时(因此不存在玩家角色)，会无法获取并转换玩家角色
		else
		{
			// 退出游戏失败，将按钮恢复启用
			ReturnButton->SetIsEnabled(true);
		}
	}
}

void UReturnToMainMenu::OnPlayerLeftGame()
{
	// 确保多人会话子系统的UC能在
	if(MultiplayerSessionsSubsystem)
	{
		// 发送网络请求，申请断开多人会话
		MultiplayerSessionsSubsystem->DestroySession();
	}
}

void UReturnToMainMenu::OnDestroySession(bool bWasSuccessful)
{
	// 断开多人会话如果失败，重新启用按钮，允许玩家再次点击
	if(!bWasSuccessful)
	{
		ReturnButton->SetIsEnabled(true);
		// 会话销毁失败，直接返回
		return;
	}
	
	/*
	 * 根据本地机器的不同，进行不同的收尾工作。
	 * 通过能否获取游戏模式来判断本地机器是服务器还是客户端。游戏模式仅存在于服务器上
	 */
	// 获取世界关卡
	UWorld* World = GetWorld();
	// 获取游戏模式。注意GetAuthGameMode和GetGameMode的区别。
	AGameModeBase* GameMode = World->GetAuthGameMode();
	// GetAuthGameMode能获取游戏模式，则说明本地机器是服务器
	if(GameMode)
	{
		// 返回主菜单，断开所有玩家的连接
		GameMode->ReturnToMainMenuHost();
	}
	// 否则说明本地机器是客户端
	else
	{
		// 获取玩家控制器
		PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;
		if(PlayerController)
		{
			// 将客户端优雅地返回到主菜单。FText()内可传入文本说明。
			PlayerController->ClientReturnToMainMenuWithTextReason(FText());
		}
	}
	
}
