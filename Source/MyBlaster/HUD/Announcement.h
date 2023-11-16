// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Announcement.generated.h"

/**
 * 
 */
UCLASS()
class MYBLASTER_API UAnnouncement : public UUserWidget
{
	GENERATED_BODY()
public:
	// 热身倒计时区UI
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* WarmupTime;
	// 公告区UI
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* AnnouncementText;
	// 信息文本区UI
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* InfoText;
	
};
