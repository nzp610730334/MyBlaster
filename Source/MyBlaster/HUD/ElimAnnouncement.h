// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ElimAnnouncement.generated.h"

/**
 * 
 */
UCLASS()
class MYBLASTER_API UElimAnnouncement : public UUserWidget
{
	GENERATED_BODY()

public:
	// 设置Announcement控件的文本内容
	void SetAnnouncementText(FString AttackerName, FString VictimName);

	// 绑定水平盒子控件
	UPROPERTY(meta = (BindWidget))
	class UHorizontalBox* AnnouncementBox;

	// 绑定Announcement文本显示控件
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* AnnouncementText;
};
