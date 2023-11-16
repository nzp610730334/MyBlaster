// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ReturnToMainMenu.generated.h"

/**
 * 
 */
UCLASS()
class MYBLASTER_API UReturnToMainMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	// 设置本菜单的行为和属性
	void MenuSetUp();
	// 销毁本菜单
	void MenuTearDown();

protected:
	// 重写父类的初始化方法
	virtual bool Initialize() override;

	// 玩家离开游戏。（断开客户端与多人会话的连接）
	UFUNCTION()
	void OnPlayerLeftGame();
	
private:
	UPROPERTY(meta = (BindWidget))
	class UButton* ReturnButton;

	// 本地机器(服务器或客户端)向服务器发送请求，请求断开多人会话
	UFUNCTION()
	void ReturnButtonClicked();

	// 多人会话子系统的对象的指针。本类通过该指针进行断开游戏等行为。
	UPROPERTY()
	class UMultiplayerSessionsSubsystem* MultiplayerSessionsSubsystem;
	
	// 玩家控制器。因为该属性需要Cast且将会被频繁访问，因此将其存储在本类中，可减少性能开销
	UPROPERTY()
	class APlayerController* PlayerController;

	// 多人会话被销毁后需要执行的行为。该函数绑定到多人会话子系统的销毁委托上。销毁委托发出广播，本函数会被执行。参数唯一，由多人会话子系统的委托传入，表示多人会话子系统的销毁成功与否。
	UFUNCTION()
	void OnDestroySession(bool bWasSuccessful);
};
