// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterOverlay.generated.h"

/**
 * 
 */
UCLASS()
class MYBLASTER_API UCharacterOverlay : public UUserWidget
{
	GENERATED_BODY()
public:
	// 血条
	UPROPERTY(meta=(BindWidget))
	class UProgressBar* HealthBar;
	// 血量文字提示
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* HealthText;
	// 当前血量
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* CurrentHealthText;
	// 最大血量
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* MaxHealthText;
	
	// 护盾
	UPROPERTY(meta=(BindWidget))
	class UProgressBar* ShieldBar;
	// 护盾文字提示
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* ShieldText;
	// 当前护盾
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* CurrentShieldText;
	// 最大护盾
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* MaxShieldText;
	
	// 分数说明
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* ScoreText;
	// 分数
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* ScoreAmount;
	// 红队分数
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* RedTeamScore;
	// 蓝队分数
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* BlueTeamScore;
	// 分数线区分符号
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* ScoreSpacerText;
	
	// 死亡次数说明
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* DefeatsText;
	// 死亡次数
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* DefeatsAmount;
	// 子弹数量说明
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* AmmoText;
	// 子弹数
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* WeaponAmmoAmount;
	// 玩家携带的后备子弹数
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* CarriedAmmoAmount;
	// 斜线/图标，用于分界弹匣子弹数量和后备子弹数量
	// UPROPERTY(meta=(BindWidget))
	// class UTextBlock* Slash;		// 斜线目前没有动态修改的打算，直接就是用UE用户组件绘制即可

	// 当前手榴弹的数量
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* GrenadesText;

	// 手榴弹的最大携带量
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* MaxGrenadesText;
	
	// 游戏时间
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* MatchCountdownText;

	// 当前Ping值
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* PingValue;
	
	// 高Ping警告用的图标
	UPROPERTY(meta=(BindWidget))
	class UImage* HighPingImage;

	// 高Ping图标闪烁的动画。Transient表示该属性没有序列化到硬盘，运行时加载
	UPROPERTY(meta=(BindWidgetAnim), Transient)
	class UWidgetAnimation* HighPingAnimation;
};
