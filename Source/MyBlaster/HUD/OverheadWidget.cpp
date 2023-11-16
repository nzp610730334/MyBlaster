// Fill out your copyright notice in the Description page of Project Settings.


#include "OverheadWidget.h"

#include "Components/TextBlock.h"

void UOverheadWidget::SetDisplayText(FString TextToString)
{
	if(DisplayText)
	{
		DisplayText->SetText(FText::FromString(TextToString));
	}
}

void UOverheadWidget::ShowPlayerNetRole(APawn* InPawn)
{
	// 查询该Pawn在本地机器中的角色
	ENetRole LocalRole = InPawn->GetLocalRole();
	// 查询该Pawn在远程机器中的角色
	ENetRole RemoteRole = InPawn->GetRemoteRole();
	FString Role;
	// switch (LocalRole)
	switch (RemoteRole)
	{
	case ENetRole::ROLE_Authority:
		Role = FString("Authority");
		break;
	case ENetRole::ROLE_AutonomousProxy:
		Role = FString("Autonomous Proxy");
		break;
	case ENetRole::ROLE_SimulatedProxy:
		Role = FString("Simulated Proxy");
		break;
	case ENetRole::ROLE_None:
		Role = FString("None");
		break;
	default:
		Role = FString("DEFAULT");
	break;
	}
	// FString LocalRoleString = FString::Printf(TEXT("Local Role: %s"), *Role);
	FString RemoteRoleString = FString::Printf(TEXT("Remote Role: %s"), *Role);
	// 设置UI的文本组件的显示
	// SetDisplayText(LocalRoleString);
	SetDisplayText(RemoteRoleString);
}

void UOverheadWidget::OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld)
{
	// 从屏幕和当前世界关卡移除
	RemoveFromParent();
	Super::OnLevelRemovedFromWorld(InLevel, InWorld);
}
