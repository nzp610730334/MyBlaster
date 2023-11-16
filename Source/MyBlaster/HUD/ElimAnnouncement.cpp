// Fill out your copyright notice in the Description page of Project Settings.


#include "ElimAnnouncement.h"

#include "Components/TextBlock.h"

void UElimAnnouncement::SetAnnouncementText(FString AttackerName, FString VictimName)
{
	// 拼接文本，玩家A击杀了玩家B。	*AttackerName表示取指针AttackerName的值
	FString ElimAnnouncementText = FString::Printf(TEXT("%s Elimed %s"), *AttackerName, *VictimName);
	// Announcement文本组件存在
	if(AnnouncementText)
	{
		// 将ElimAnnouncementText转换成FText类型，并将其设置为Announcement文本显示的值
		AnnouncementText->SetText(FText::FromString(ElimAnnouncementText));
	}
}
