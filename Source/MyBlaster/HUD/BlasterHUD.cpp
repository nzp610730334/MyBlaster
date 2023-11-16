// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterHUD.h"

#include "Announcement.h"
#include "CharacterOverlay.h"
#include "HitedDamageNumber.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CapsuleComponent.h"
#include "Components/HorizontalBox.h"
#include "Kismet/GameplayStatics.h"
#include "MyBlaster/GameState/BlasterGameState.h"


void ABlasterHUD::BeginPlay()
{
	Super::BeginPlay();
	// 在屏幕上绘制玩家角色信息
	// AddCharacterOverlay();		// 添加了游戏阶段的功能后，只在进入游戏中的阶段后才绘制玩家角色信息，此处注释。

	// 测试完毕后注释掉
	// AddElimAnouncement(FString("Player 1"), FString("Player 2"));
	// 三元运算符判断并获取玩家控制器.GetOwningPlayerController返回此HUD播放器的PlayerController。
	// OwningPlayer = OwningPlayer == nullptr ? GetOwningPlayerController() : OwningPlayer;

	// 三元运算符判断并获取玩家控制器.GetOwningPlayerController返回此HUD播放器的PlayerController。
	OwningPlayer = OwningPlayer == nullptr ? GetOwningPlayerController() : OwningPlayer;
}

void ABlasterHUD::AddCharacterOverlay()
{
	// 获取玩家控制器
	APlayerController* PlayerController = GetOwningPlayerController();
	// 判断获取的控制器是否有效以及是否在UE编辑器中设置了UI组件
	if(PlayerController && CharacterOverlayClass)
	{
		// 实例化UI（理解为将UE中创建的UI组件CharacterOverlayClass和此处C++的变量CharacterOverlay建立了联系。）
		CharacterOverlay = CreateWidget<UCharacterOverlay>(PlayerController, CharacterOverlayClass);
		/*
		 * 绘制到屏幕上。（注意两种绘制方式的不同）
		 * AddToViewport() : 玩家的所有屏幕窗口上都会绘制。
		 * AddToPlayerScreen() : 只在玩家主屏幕窗口上绘制。比如有的上下分屏游戏机设备(上面是玩家主窗口显示游戏画面，下面是触屏操作菜单)
		 */ 
		CharacterOverlay->AddToViewport();
		// CharacterOverlay->AddToPlayerScreen();
	}
}

void ABlasterHUD::AddAnnouncement()
{
	// 获取本HUD的玩家的控制器
	APlayerController* PlayerController = GetOwningPlayerController();
	if(PlayerController && AnnouncementClass)
	{
		// 实例化UI（理解为将UE中创建的UI组件AnnouncementCLass和此处C++的变量Announcement建立了联系。）
		Announcement = CreateWidget<UAnnouncement>(PlayerController, AnnouncementClass);
		// 将UI组件绘制到屏幕上
		Announcement->AddToViewport();
		// Announcement->AddToPlayerScreen();
	}
}

void ABlasterHUD::AddElimAnouncement(FString AttackerName, FString VictimName)
{
	// 三元运算符判断并获取玩家控制器.GetOwningPlayerController返回此HUD播放器的PlayerController。
	OwningPlayer = OwningPlayer == nullptr ? GetOwningPlayerController() : OwningPlayer;
	// 玩家控制器获取成功且蓝图里设置了击杀通告组件
	if(OwningPlayer && ELimAnnouncementClass)
	{
		// 通过蓝图设置的击杀通告组件，创建击杀通告组件
		UElimAnnouncement* ElimAnnouncementWidget = CreateWidget<UElimAnnouncement>(OwningPlayer, ELimAnnouncementClass);
		// 创建成功
		if(ElimAnnouncementWidget)
		{
			// 设置击杀通告组件的文本显示内容
			ElimAnnouncementWidget->SetAnnouncementText(AttackerName, VictimName);
			// 将创建的击杀通告组件显示到玩家视口上
			ElimAnnouncementWidget->AddToViewport();

			/*
			 * 遍历击杀通告数组当前内的所有通告，将其位置一一上移。
			 * auto自适应ElimMessages的数据类型，或者直接指定对应类型，即 UElimAnnouncement*
			 */ 
			for(UElimAnnouncement* Msg : ElimMessages)
			{
				// 确保该数组元素存在,且box控件存在
				if(Msg && Msg->AnnouncementBox)
				{
					// 获取该击杀通告在Canvas Panel画布面板上的相关信息
					UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(Msg->AnnouncementBox);
					// 确保获取成功
					if(CanvasSlot)
					{
						// 记录Box的当前平面坐标(画布画板坐标？)
						FVector2d Position = CanvasSlot->GetPosition();
						// 变更后的坐标。（X轴不变，Y轴减少box自身高度。注意，画布画板中，左上角为(0,0),Y轴往下增加X轴往右增加）
						FVector2d NewPosistion(
							CanvasSlot->GetPosition().X,
							Position.Y - CanvasSlot->GetSize().Y
						);
						// 变更画布画板中box的坐标
						CanvasSlot->SetPosition(NewPosistion);
					}
				}
			}
			
			// 将新创建的击杀通告添加到用于存储击杀通告的数组中
			ElimMessages.Add(ElimAnnouncementWidget);
			
			/*
			 * 对已生成的击杀通告的后续处理
			 */
			// 实例化新的定时器
			FTimerHandle ElimMsgTimer;
			// 实例化新的定时器委托
			FTimerDelegate ElimMsgDelegate;
			/*
			 * 将要执行的函数绑定到定时器委托上。
			 * 疑惑：为什么要隔一个定时器委托？定时器直接计时执行目标函数不行吗？
			 */
			ElimMsgDelegate.BindUFunction(this, FName("ElimAnnouncementTimerFinished"), ElimAnnouncementWidget);
			// 设置定时器计时执行委托
			GetWorldTimerManager().SetTimer(
				// 定时器
				ElimMsgTimer,
				// 要执行的委托
				ElimMsgDelegate,
				// 执行间隔
				ElimAnnouncementTime,
				// 是否循环执行
				false
				);
		}
	}
}

void ABlasterHUD::ElimAnnouncementTimerFinished(UElimAnnouncement* MsgToRemove)
{
	// 确定要处理的击杀通告已存在
	if(MsgToRemove)
	{
		// 将其从父级对象移除。（从其父窗口小部件中删除窗口小部件。如果这个小部件被添加到玩家的屏幕或视口中，它也将从这些容器中删除。）
		MsgToRemove->RemoveFromParent();
	}
}

void ABlasterHUD::DrawHUD()
{
	Super::DrawHUD();

	/*
	 * 切换地图时HUDPackage不存在，此时绘制会报错奔溃，因此非正常游戏阶段，不绘制准星。
	 */
	// 获取游戏状态
	BlasterGameState = BlasterGameState == nullptr ? Cast<ABlasterGameState>(UGameplayStatics::GetGameState(this)) : BlasterGameState;
	if(BlasterGameState == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("BlasterGameState == nullptr!!!!!!!!!!!!!!！"));
		return;
	}
	// 不处于正式游戏阶段时，无需绘制
	if(BlasterGameState->GetMatchState() != MatchState::InProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetMatchState() != MatchState::InProgress,不绘制准星！"));
		return;
	}
	
	FVector2d ViewportSize;
	// if(GEngine)
	// 添加新功能，bShouldDrawCrosshair，狙击枪开启瞄准镜时无需绘制十字准星
	if(GEngine && bShouldDrawCrosshair)
	{
		// 传入前面初始化的ViewportSize接收窗口大小的值
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		// 计算窗口中心的坐标
		const FVector2d ViewportCenter(ViewportSize.X /2.f, ViewportSize.Y / 2.f);

		// 准星偏移。基础系数*从武器获得的偏移量
		float SpreadScaled = CrosshairSpreadMax * HUDPackage.CrosshairSpread;
		// 校验要绘制的内容是否存在
		if(HUDPackage.CrosshairsCenter)
		{
			FVector2d Spread(0.f, 0.f);
			// 绘制
			DrawCrosshair(HUDPackage.CrosshairsCenter, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if(HUDPackage.CrosshairsLeft)
		{
			FVector2d Spread(-SpreadScaled, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsLeft, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if(HUDPackage.CrosshairsRight)
		{
			FVector2d Spread(SpreadScaled, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsRight, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if(HUDPackage.CrosshairsTop)
		{
			FVector2d Spread(0.f, -SpreadScaled);
			DrawCrosshair(HUDPackage.CrosshairsTop, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if(HUDPackage.CrosshairsBottom)
		{
			FVector2d Spread(0.f, SpreadScaled);
			DrawCrosshair(HUDPackage.CrosshairsBottom, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
	}
}

void ABlasterHUD::DrawCrosshair(UTexture2D* Texture, FVector2d ViewportCenter, FVector2d Spread, FLinearColor CrosshairsColor)
{
	// 获取纹理的宽
	const float TextureWidth = Texture->GetSizeX();
	// 获取纹理的高
	const float TextureHeigh = Texture->GetSizeY();
	/*
	 * 计算绘制的起始点TextureDrawPoint.
	 * 备注：当原本的纹理符合元素内容居中时，即纹理的元素内容的XY位置都是纹理的长宽的一半。绘制准星时，绘制起点是纹理的左上角？
	 * 因此为了修正偏移，使得纹理的元素内容居中，即准星居中，需要把纹理绘制起点左偏移纹理的一半宽，上偏移纹理的一半高。
	 * 还需要加上Spread，这是受其他因素印象的偏移。（如角色移动和武器后坐力？）
	 */
	const FVector2d TextureDrawPoint(
		ViewportCenter.X - (TextureWidth / 2.f) + Spread.X,
		ViewportCenter.Y - (TextureHeigh / 2.f) + Spread.Y
	);
	// 开始绘制。参数含义？
	DrawTexture(
		Texture,
		TextureDrawPoint.X,
		TextureDrawPoint.Y,
		TextureWidth,
		TextureHeigh,
		0.f,
		0.f,
		1.f,
		1.f,
		CrosshairsColor
	);
}

void ABlasterHUD::ShowEnemyBeHitedDamageNumber(float DamageNumber, AActor* HitCharacter, bool bCriticalStrike)
{
	if(HitCharacter == nullptr) return;
	// 获取受击者的位置作为伤害数字的默认出现位置
	FVector HitCharacterLocation = HitCharacter->GetActorLocation();
	// 转换受击者
	ABlasterCharacter* HitBlasterCharacter = Cast<ABlasterCharacter>(HitCharacter);
	if(HitBlasterCharacter)
	{
		// 初始化新的float类型，用于后续存储角色胶囊体半径。
		float OutRadius;
		// 初始化新的float类型，用于后续存储角色胶囊体半高
		float OutHalfHeight;
		// 获取玩家角色的胶囊体半径和半高
		HitBlasterCharacter->GetCapsuleComponent()->GetScaledCapsuleSize(OutRadius, OutHalfHeight);
		// 变更伤害数字显示位置
		HitCharacterLocation = HitCharacterLocation + FVector(0.f, 0.f, OutHalfHeight);
	}

	// 初始化新的FVector，存储受击者在玩家屏幕上的位置
	FVector2d HitCharacter2DLocation;
	// 三元运算符判断并获取玩家控制器.GetOwningPlayerController返回此HUD播放器的PlayerController。
	OwningPlayer = OwningPlayer == nullptr ? GetOwningPlayerController() : OwningPlayer;
	// 转换受击者的位置为攻击者的屏幕上的对应位置
	UGameplayStatics::ProjectWorldToScreen(
		OwningPlayer,
		HitCharacterLocation,
		HitCharacter2DLocation,
		// 这是否应该相对于玩家视口子区域（在分屏中使用玩家附加的小部件时很有用）。设置为true，开镜时也能显示合适的位置
		true
	);
	
	// 生成伤害组件（传入数值、位置、是否暴击）
	UHitedDamageNumber* HitedDamageNumber = CreateWidget<UHitedDamageNumber>(OwningPlayer, HitedDamageNumberClass);
	// HitedDamageNumber->DamageValue = DamageNumber;
	// HitedDamageNumber->bCriticalStrike = bCriticalStrike;
	HitedDamageNumber->SetDamageText(DamageNumber);
	TarrayHitedDamageNumber.Add(HitedDamageNumber);
	// 添加到攻击者的屏幕上
	HitedDamageNumber->AddToViewport();
	// 设置位置。bRemoveDPSScale设置为true，避免偏移
	HitedDamageNumber->SetPositionInViewport(HitCharacter2DLocation, true);
	// 根据暴击与否，播放不同的动画
	HitedDamageNumber->PlayAnimation(bCriticalStrike?HitedDamageNumber->CriticalStrikeDamage:HitedDamageNumber->NormalDamage);

	/*
	 * 设置定时器，一定时间(2s)后将其从父级移除
	 */
	// 实例化新的定时器
	FTimerHandle RemoveHitDamageNumberTimer;
	// 实例化新的定时器委托
	FTimerDelegate RemoveHitDamageNumberDelegate;
	/*
	 * 将要执行的函数绑定到定时器委托上。
	 * 疑惑：为什么要隔一个定时器委托？定时器直接计时执行目标函数不行吗？
	 */
	RemoveHitDamageNumberDelegate.BindUFunction(this, FName("RemoveHitedDamageNumber"), HitedDamageNumber);
	// 设置定时器计时执行委托
	GetWorldTimerManager().SetTimer(
		// 定时器
		RemoveHitDamageNumberTimer,
		// 要执行的委托
		RemoveHitDamageNumberDelegate,
		// 执行间隔
		2.f,
		// 是否循环执行
		false
		);
}

void ABlasterHUD::RemoveHitedDamageNumber(UHitedDamageNumber* HitedDamageNumber)
{
	// 确定要清除的伤害数字对象存在
	if(HitedDamageNumber)
	{
		// 将其从父级对象移除。（从其父窗口小部件中删除窗口小部件。如果这个小部件被添加到玩家的屏幕或视口中，它也将从这些容器中删除。）
		HitedDamageNumber->RemoveFromParent();
	}
	
	UE_LOG(LogTemp, Warning, TEXT("清楚了已经生成的伤害数字！"));
}
