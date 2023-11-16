// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ElimAnnouncement.h"
#include "GameFramework/HUD.h"
#include "MyBlaster/Character/BlasterCharacter.h"
#include "BlasterHUD.generated.h"

// 准星结构体
USTRUCT(BlueprintType)
struct FHUDPackage
{
	GENERATED_BODY()

public:
	class UTexture2D* CrosshairsCenter;
	UTexture2D* CrosshairsLeft;
	UTexture2D* CrosshairsRight;
	UTexture2D* CrosshairsTop;
	UTexture2D* CrosshairsBottom;
	float CrosshairSpread;
	FLinearColor CrosshairsColor;
};

/**
 * 
 */
UCLASS()
class MYBLASTER_API ABlasterHUD : public AHUD
{
	GENERATED_BODY()

public:
	// 每帧执行的绘制图形的函数
	virtual void DrawHUD() override;

	// 玩家UI信息组件(在UE中创建的UserWidget)
	UPROPERTY(EditAnywhere, Category = "Player Stats")
	TSubclassOf<class UUserWidget> CharacterOverlayClass;
	// 角色信息组件类
	UPROPERTY()
	class UCharacterOverlay* CharacterOverlay;
	// 在屏幕上绘制玩家角色信息(血条等)
	void AddCharacterOverlay();

	// 信息公告区UI(在UE中创建的UserWidget)
	UPROPERTY(EditAnywhere, Category = "Announcement")
	TSubclassOf<class UUserWidget> AnnouncementClass;
	// 公告信息类
	UPROPERTY()
	class UAnnouncement* Announcement;
	// 在屏幕和是哪个绘制公告UI
	void AddAnnouncement();

	// 实例化通告控件并将其绘制到玩家的屏幕上
	void AddElimAnouncement(FString AttackerName, FString VictimName);
	
	// 显示命中敌人的伤害数字。需要的参数：命中的伤害数值、属于哪个敌人的受击数值、是否暴击
	UFUNCTION()
	void ShowEnemyBeHitedDamageNumber(float DamageNumber, AActor* HitCharacter, bool bCriticalStrike);
	
protected:
	// 重写父类的BeginPlay方法
	virtual void BeginPlay() override;

private:
	// 拥有本HUD的玩家控制器
	UPROPERTY()
	APlayerController* OwningPlayer;
	
	// 游戏模式
	UPROPERTY()
	ABlasterGameState* BlasterGameState;
	
	// 要绘制的准星结构体
	FHUDPackage HUDPackage;

	// 绘制单个纹理到窗口对应位置上
	void DrawCrosshair(UTexture2D* Texture, FVector2d ViewportCenter, FVector2d Spread, FLinearColor CrosshairsColor);

	// 准星最大偏移量
	UPROPERTY(EditAnywhere)
	float CrosshairSpreadMax = 16.f;

	// 原教程没有的功能
	// 是否需要绘制十字准星(比如：狙击枪开瞄准镜的时候瞄准镜UI有专门的准星，不需要绘制十字准星)
	bool bShouldDrawCrosshair = true;

	// 在蓝图中设置的击杀通告组件
	UPROPERTY(EditAnywhere)
	TSubclassOf<class UElimAnnouncement> ELimAnnouncementClass;

	// 击杀通告存在的时长
	UPROPERTY(EditAnywhere)
	float ElimAnnouncementTime = 2.5f;
	// 定时函数:定时处理已生成的击杀通告。传入要处理的击杀通告的指针
	UFUNCTION()
	void ElimAnnouncementTimerFinished(UElimAnnouncement* MsgToRemove);
	// 击杀通告数组。用于存储已生成的还未销毁的击杀通告
	UPROPERTY()
	TArray<UElimAnnouncement*> ElimMessages;

	
	// 在蓝图中设置的命中伤害数字
	UPROPERTY(EditAnywhere)
	TSubclassOf<class UHitedDamageNumber> HitedDamageNumberClass;
	// 定时函数，定时清除已生成的伤害数字，传入要清除的伤害数字的指针
	UFUNCTION()
	void RemoveHitedDamageNumber(UHitedDamageNumber* HitedDamageNumber);
	// 用于存储生成命中伤害数字的组件的数组
	UPROPERTY()
	TArray<UHitedDamageNumber*> TarrayHitedDamageNumber;
	
public:
	// 设置HUDPackage
	FORCEINLINE void SetHUDPackage(const FHUDPackage& package) { HUDPackage = package; }
	// 设置是否需要绘制十字准星的开关。BlueprintCallable表示暴露给蓝图调用
	UFUNCTION(BlueprintCallable)
	FORCEINLINE void SetShouldDrawCrosshair(bool bShouldDraw) { bShouldDrawCrosshair = bShouldDraw; };

	
};
