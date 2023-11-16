// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterPlayerController.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "MyBlaster/BlasterComponents/CombatComponent.h"
#include "MyBlaster/BlasterTypes/Announcement.h"
#include "MyBlaster/Character/BlasterCharacter.h"
#include "MyBlaster/HUD/BlasterHUD.h"
#include "MyBlaster/HUD/CharacterOverlay.h"
#include "Net/UnrealNetwork.h"
#include "MyBlaster/GameMode/BlasterGameMode.h"
#include "MyBlaster/GameState/BlasterGameState.h"
#include "MyBlaster/HUD/Announcement.h"
#include "MyBlaster/HUD/ReturnToMainMenu.h"
#include "MyBlaster/PlayerState/BlasterPlayerState.h"


void ABlasterPlayerController::BroadcastElim(APlayerState* Attacker, APlayerState* Victim)
{
	ClientElimAnnouncement(Attacker, Victim);
}

void ABlasterPlayerController::ClientElimAnnouncement_Implementation(APlayerState* Attacker, APlayerState* Victim)
{
	APlayerState* Self = GetPlayerState<APlayerState>();
	if(Self)
	{
		// 获取并转换HUD
        BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
		if(Attacker && Victim && BlasterHUD)
		{
			// 自己击杀别人时
			if(Attacker == Self && Victim != Self)
			{
				BlasterHUD->AddElimAnouncement(FString("You"), Victim->GetPlayerName());
				return;
			}
			// 自己被击杀时
			if(Victim == Self && Attacker != Self)
			{
				BlasterHUD->AddElimAnouncement(Attacker->GetPlayerName(), FString("You"));
				return;
			}
			// 自杀时
			if(Attacker == Victim && Attacker == Self)
			{
				BlasterHUD->AddElimAnouncement(FString("You"), FString("Yourself"));
				return;
			}
			// 别人自杀时
			if(Attacker == Victim && Attacker != Self)
			{
				BlasterHUD->AddElimAnouncement(Attacker->GetPlayerName(), FString("themself"));
				return;
			}
			// 最后只剩下最常见的情况，击杀者和被击杀者都不是自己时，击杀通告显示双方玩家名称
			BlasterHUD->AddElimAnouncement(Attacker->GetPlayerName(), Victim->GetPlayerName());
		}
	}
}

void ABlasterPlayerController::SetupInputComponent()
{
	// 先执行父类方法，实现必要的行为
	Super::SetupInputComponent();

	// 输入组件为空时，直接返回
	if(InputComponent == nullptr) return;
	/*
	 * 将响应的函数绑定到输入事件上。
	 * Quit			动作映射的名称
	 * IE_Pressed	按下按钮
	 * this			调用者？
	 * &ABlasterPlayerController::ShowReturnToMainMenu		绑定的函数
	 */ 
	InputComponent->BindAction("Quit", IE_Pressed, this, &ABlasterPlayerController::ShowReturnToMainMenu);
}

void ABlasterPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	BlasterHUD = Cast<ABlasterHUD>(GetHUD());
	
	// 向服务器发送RPC请求，要求服务器更新本玩家控制器上存储的比赛相关时间信息和比赛状态
	ServerCheckMatchState();

	// 停止高Ping图标的显示
	StopHighPingWarning();
}

void ABlasterPlayerController::ServerCheckMatchState_Implementation()
{
	// 获取当前游戏模式并转换为本项目的游戏模式类。
	ABlasterGameMode* GameMode = Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this));
	// 转换成功，将游戏模式的时间设置存储在玩家控制器的属性中
	if(GameMode)
	{
		// 热身时间的长度
		WarmupTime = GameMode->WarmupTime;
		// 一局比赛的时长
		MatchTime = GameMode->MatchTime;
		// 比赛结束后的休息时长
		CooldownTime = GameMode->CooldownTime;
		// 当前世界关卡的开始时间
		LevelStartingTime = GameMode->LevelStartingTime;
		// 更新比赛状态
		MatchState = GameMode->GetMatchState();
		// 上面只是赋值更新了服务器机器上的该玩家的控制器存储的变量属性，还需要向客户端发送RPC，更新客户端上的该玩家控制器存储的的相关属性
		ClientJoinMidgame(MatchState, WarmupTime, MatchTime, CooldownTime, LevelStartingTime);
	}
	/*
	 * 疑惑：本代码是在服务器上执行，当前机器是服务器机器，为什么在这里也需要绘制？
	 * 后续：此处如果也绘制，服务器上的玩家的屏幕会有两份相同内容的UI，且先绘制的UI后续无法根据比赛状态的改变而隐藏。（指针存储了后绘制的UI的地址？先绘制的UI的内存地址丢失了？）
	 * 而且服务器本身也是客户端，两次RPC请求都是自己发给自己，后面那一次已经进行了UI绘制的行为，因此前一次应该没有必要绘制。因此此处注释掉。注释后测试正常。
	 */ 
	// 当前比赛状态处于开始前的热身阶段时，才绘制更新玩家客户端上的公告UI。注意先判断是否不为空才执行操作，避免崩溃。
	// if(BlasterHUD && MatchState == MatchState::WaitingToStart)
	// {
	// 	BlasterHUD->AddAnnouncement();
	// }
}

void ABlasterPlayerController::ClientJoinMidgame_Implementation(FName StateOfMatch, float Warmup, float Match, float Cooldown, float StartingTime)
{
	// 本函数在服务器上调用，在客户端上执行。服务器传进来的参数，赋值更新到本地客户端的玩家控制器的属性上。
	WarmupTime = Warmup;
	MatchTime = Match;
	LevelStartingTime = StartingTime;
	MatchState = StateOfMatch;
	CooldownTime = Cooldown;
	// 根据比赛状态的变化进行处理。
	OnMatchStateSet(MatchState);
	// 当前比赛状态处于开始前的热身阶段时，才绘制更新玩家客户端上的公告UI。注意先判断是否不为空才执行操作，避免崩溃。
	if(BlasterHUD && MatchState == MatchState::WaitingToStart)
	{
		BlasterHUD->AddAnnouncement();
	}
}

void ABlasterPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	// 对传入的受控对象进行转换
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(InPawn);
	// 判断是否转换成功，成功则设置初始化属性
	if(BlasterCharacter)
	{
		// 设置血量的显示
		SetHUDHealth(BlasterCharacter->GetHealth(), BlasterCharacter->GetMaxHealth());
	}
}

void ABlasterPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// 处理是否要刷新游戏时间UI的逻辑
	SetHUDTime();
	// 客户端检查是否需要进行和服务器的时间同步
	CheckTimeSync(DeltaTime);
	// 确保某些重要的逻辑执行到位。
	PollInit();
	// 检查Ping的值并处理高Ping图标的显示和隐藏
	CheckPing(DeltaTime);
}

void ABlasterPlayerController::CheckPing(float DeltaTime)
{
	// 服务器上不需要进行Ping检查，直接返回即可
	if(HasAuthority()) return;
	// 累计记录高Ping图标的显示时间
	HighPingRunningTime += DeltaTime;
	// 如果超过预设的时长
	if(HighPingRunningTime >= CheckPingFrequency)
	{
		// 获取玩家状态并转换
		PlayerState = PlayerState == nullptr ? GetPlayerState<APlayerState>() : PlayerState;
		// 获取并转换成功
		if(PlayerState)
		{
			/*
			 * 查询该玩家的Ping值是否超过认为是高Ping的阈值。
			 * 注意：在网络通信的过程中，UE将Ping的大小缩小到了原本值的1/4，因此需要乘以4获得原本的Ping值
			 */
			// UE_LOG(LogTemp, Warning, TEXT("当前Ping值为： %d"), PlayerState->GetPing() * 4);
			if(PlayerState->GetPing() * 4 >= HighPingThreshold)
			{
				// 显示高Ping图标
				HighPingWarning();
				// 重置高Ping图标已显示的时长
				PingAnimationRunningTime = 0.f;
				/*
				 * 高ping情况下，需要向服务器报告当前机器处于高延迟状态.
				 * 疑惑：这里不直接将相关属性设置为高延迟状态下的应设的属性吗？比如武器的bUseSererSideRewind设置为false，高延迟下不使用服务器倒带
				 */ 
				ServerReportPingStatus(true);
				// UE_LOG(LogTemp, Warning, TEXT("已经发送ServerReportPingStatus，布尔值为true!!!"));
			}
			else
			{
				ServerReportPingStatus(false);
				// UE_LOG(LogTemp, Warning, TEXT("已经发送ServerReportPingStatus，布尔值为false!!!"));
			}
		}
		// 重置查询Ping频率的计时
		HighPingRunningTime = 0.f;
	}
	// 判断高Ping图标的对应的动画存在且正在播放中
	bool bHighPingAnimationPlaying = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
			BlasterHUD->CharacterOverlay->HighPingAnimation &&
				BlasterHUD->CharacterOverlay->IsAnimationPlaying(BlasterHUD->CharacterOverlay->HighPingAnimation);
	if(bHighPingAnimationPlaying)
	{
		// 累加高Ping图标已显示的时长
		PingAnimationRunningTime += DeltaTime;
		// 判断高Ping图标已显示的时长是否已经超过预设的时长
		if(PingAnimationRunningTime > HighPingDuration)
		{
			// 停止高Ping图标的显示
			StopHighPingWarning();
		}
	}
}

void ABlasterPlayerController::ShowReturnToMainMenu()
{
	// 菜单组件未设置时，直接返回
	if(ReturnToMainMenuWidget == nullptr) return;
	// 菜单组件未被实例化时，将其实例化
	if(ReturnToMainMenu == nullptr)
	{
		// 创建菜单组件并将其转换为UReturnToMainMenu类型
		ReturnToMainMenu = CreateWidget<UReturnToMainMenu>(this, ReturnToMainMenuWidget);
	}
	// 菜单组件已被实例化
	if(ReturnToMainMenu)
	{
		/*
		 * 将菜单打开状态取反。原本菜单关闭状态，执行本函数，则变为打开状态；原本打开，则执行本函数就会关闭
		 * 然后根据取反后的状态，显示或者销毁菜单
		 */
		bReturnToMainMenuOpen = !bReturnToMainMenuOpen;
		if(bReturnToMainMenuOpen)
		{
			// 显示菜单
			ReturnToMainMenu->MenuSetUp();
		}
		else
		{
			// 销毁菜单
			ReturnToMainMenu->MenuTearDown();
		}
	}
}

void ABlasterPlayerController::OnRep_ShowTeamScores()
{
	if(bShowTeamScores)
	{
		InitTeamScores();
		// UE_LOG(LogTemp, Warning, TEXT("已执行InitTeamScores()！！！"));
	}
	else
	{
		HideTeamScores();
		// UE_LOG(LogTemp, Warning, TEXT("已执行HideTeamScores()！！！"));
	}
}

void ABlasterPlayerController::ServerReportPingStatus_Implementation(bool bHighPing)
{
	// 调用委托的广播函数,需要传入该委托声明时的函数。则所有绑定了该委托的函数都会被执行。
	HighPingDelegate.Broadcast(bHighPing);
}

void ABlasterPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// DOREPLIFETIME注册的属性都会无条件网络更新复制
	DOREPLIFETIME(ABlasterPlayerController, MatchState);
	DOREPLIFETIME(ABlasterPlayerController, bShowTeamScores);
}

void ABlasterPlayerController::HideTeamScores()
{
	// 获取并转换HUD
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	// 判断控件是否都存在
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
			BlasterHUD->CharacterOverlay->RedTeamScore &&
				BlasterHUD->CharacterOverlay->BlueTeamScore  &&
					BlasterHUD->CharacterOverlay->ScoreSpacerText;
	// 将所有控件设置为空字符串，以达到视觉上隐藏即可
	if(bHUDValid)
	{
		BlasterHUD->CharacterOverlay->RedTeamScore->SetText(FText());
		BlasterHUD->CharacterOverlay->BlueTeamScore->SetText(FText());
		BlasterHUD->CharacterOverlay->ScoreSpacerText->SetText(FText());
	}
}

void ABlasterPlayerController::InitTeamScores()
{
	// 获取并转换HUD
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	// 判断控件是否都存在
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
			BlasterHUD->CharacterOverlay->RedTeamScore &&
				BlasterHUD->CharacterOverlay->BlueTeamScore  &&
					BlasterHUD->CharacterOverlay->ScoreSpacerText;
	if(bHUDValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("本机器上的bHUDValid为true！！！"));
		// 实例化新的SFtring初始化0
		FString Zero("0");
		// 初始化竖线
		FString Spacer("|");
		// 设置显示
		BlasterHUD->CharacterOverlay->RedTeamScore->SetText(FText::FromString(Zero));
		BlasterHUD->CharacterOverlay->BlueTeamScore->SetText(FText::FromString(Zero));
		BlasterHUD->CharacterOverlay->ScoreSpacerText->SetText(FText::FromString(Spacer));
	}
	else if(!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("非服务器上bHUDValid为false！！！"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("服务器上bHUDValid为false！！！"));
	}
}

void ABlasterPlayerController::SetHUDRedTeamScore(int32 RedScore)
{
	// 获取并转换HUD
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	// 判断控件是否都存在
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
			BlasterHUD->CharacterOverlay->RedTeamScore;
	// 将所有控件设置为空字符串，以达到视觉上隐藏即可
	if(bHUDValid)
	{
		// 转换得分为text然后格式化为fstring。疑惑：转换为text后能不能直接转FText
		FString ScoreText = FString::Printf(TEXT("%d"), RedScore);
		// 设置显示
		BlasterHUD->CharacterOverlay->RedTeamScore->SetText(FText::FromString(ScoreText));
	}
}

void ABlasterPlayerController::SetHUDBlueTeamScore(int32 BlueScore)
{
	// 获取并转换HUD
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	// 判断控件是否都存在
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
			BlasterHUD->CharacterOverlay->RedTeamScore;
	// 将所有控件设置为空字符串，以达到视觉上隐藏即可
	if(bHUDValid)
	{
		// 转换得分为text然后格式化为fstring。疑惑：转换为text后能不能直接转FText
		FString ScoreText = FString::Printf(TEXT("%d"), BlueScore);
		// 设置显示
		BlasterHUD->CharacterOverlay->BlueTeamScore->SetText(FText::FromString(ScoreText));
	}
}

void ABlasterPlayerController::CheckTimeSync(float DeltaTime)
{
	// 更新累计时间
	TimeSyncRunningTime += DeltaTime;
	// 判断本地机器是否为客户端且超过一定时间后才需要进行时间同步。
	if(IsLocalController() && TimeSyncRunningTime > TimeSyncFrequency)
	{
		// 进行时间同步
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
		// 重置累计的时间。
		TimeSyncRunningTime = 0.f;
	}
}

void ABlasterPlayerController::PollInit()
{
	/*
	 * 一些重要的逻辑为了确保执行到位，则放在本函数处理，本函数被Tick执行。
	 * 比如：游戏阶段从 开始前 过渡到 游戏中 时，HUD的绘制可能在Character生成之前就执行完毕了，因为当时没有Character，所以绘制失败。
	 * 因此在本函数中需要再绘制一次。
	 */
	// UI内容为空，则需要初始化
	if(CharacterOverlay == nullptr)
	{
		// 判断HUD是否存在且CharacterOverlay是否存在
		if(BlasterHUD && BlasterHUD->CharacterOverlay)
		{
			// 赋值存储
			CharacterOverlay = BlasterHUD->CharacterOverlay;
			// 存储成功
			if(CharacterOverlay)
			{
				// 血量HUD刷新失败时，刷新血量
				if(bInitializeHealth) SetHUDHealth(HUDHealth, HUDMaxHealth);
				// 护盾值HUD刷新失败时，刷新护盾值
				if(bInitializeShield) SetHUDShield(HUDShield, HUDMaxShield);
				// 分数HUD刷新失败时，刷新分数
				if(bInitializeScore) SetHUDScore(HUDScore);
				// 死亡次数HUD刷新失败时，刷新死亡次数
				if(bInitializeDefeats) SetHUDDefeats(HUDDefeats);
				// 后备弹药HUD刷新失败时，刷新后备弹药HUD
				if(bInitializeCarriedAmmo) SetHUDCarriedAmmo(HUDCarriedAmmo);
				// 武器弹匣弹药HUD刷新失败时，刷新武器弹匣弹药HUD
				if(bInitializeWeaponAmmo) SetHUDWeaponAmmo(HUDWeaponAmmo);
				
				// 获取并Cast转换玩家角色。虽然是开销大的Cast行为，且本函数在还Tick中执行，但仅在CharacterOverlay == nullptr时才会执行到此，后续CharacterOverlay初始化完毕就不会执行本代码了。
				ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn());
				if(BlasterCharacter)
				{
					// 手榴弹HUD刷新失败时，刷新玩家角色UI的手榴弹数
					if(bInitializeGrenades) SetHUDGrenades(BlasterCharacter->GetCombat()->GetGrenades());
				}
			}

			// 原教程没有的代码。客户端的团队得分没能正常刷新，因此在此处再调用一次。疑惑：这是个回调函数，正常情况下只有ShowTeamScores被网络复制时才会调用，这么直接调用是否恰当？
			OnRep_ShowTeamScores();
		}
	}
}

void ABlasterPlayerController::HighPingWarning()
{
	// 获取HUD并转换
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	// 确定UI控件以及里面的组件正常存在
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
			BlasterHUD->CharacterOverlay->HighPingImage &&
				BlasterHUD->CharacterOverlay->HighPingAnimation;
	if(bHUDValid)
	{
		// 修改该组件的透明度为1以此来达到将其显示的目的
		BlasterHUD->CharacterOverlay->HighPingImage->SetOpacity(1.f);
		// 播放组件的动画
		BlasterHUD->CharacterOverlay->PlayAnimation(
			// 要播放的动画
			BlasterHUD->CharacterOverlay->HighPingAnimation,
			// 从第几秒开始播放
			0.f,
			// 循环播放的次数
			5);
	}
}

void ABlasterPlayerController::StopHighPingWarning()
{
	// 获取HUD并转换
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	// 确定UI控件以及里面的组件正常存在
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
			BlasterHUD->CharacterOverlay->HighPingImage &&
				BlasterHUD->CharacterOverlay->HighPingAnimation;
	if(bHUDValid)
	{
		// 修改该组件的透明度为0以此来达到将其隐藏的目的
		BlasterHUD->CharacterOverlay->HighPingImage->SetOpacity(0.f);
		UE_LOG(LogTemp, Warning, TEXT("已经停止高Ping图标的显示！！！"));
		// 确保要停止控件播放的动画正在播放中
		if(BlasterHUD->CharacterOverlay->IsAnimationPlaying(BlasterHUD->CharacterOverlay->HighPingAnimation))
		{
			// 停止该动画的播放
			BlasterHUD->CharacterOverlay->StopAnimation(BlasterHUD->CharacterOverlay->HighPingAnimation);
		}
	}
}

void ABlasterPlayerController::SetHUDHealth(float Health, float MaxHealth)
{
	// 确保要操作的内容不为空指针。三元运算符，第一次执行本函数时，若为空指针，则Cast转换，后续再执行本函数时，不为空指针直接赋值旧的值即可，类似单例模式的思想
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	
	// 校验要访问的内容是否为空指针
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay && 
			BlasterHUD->CharacterOverlay->HealthBar &&
				BlasterHUD->CharacterOverlay->HealthText;
	if(bHUDValid)
	{
		// 未解决的疑惑，这里为什么要使用const修饰符？
		// 计算血量百分比
		const float HealthPercent = Health / MaxHealth;
		// 设置HUD的血量进度条
		BlasterHUD->CharacterOverlay->HealthBar->SetPercent(HealthPercent);
		// 准备HUD要绘制的血量文字显示。PrintF通过指定格式实例化FString类型，CeilToInt为向上取整到Int类型
		FString HealthText = FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(Health), FMath::CeilToInt(MaxHealth));
		// 设置HUD要绘制的文字。使用FromString传入FString类型转换成FText类型。
		BlasterHUD->CharacterOverlay->HealthText->SetText(FText::FromString(HealthText));

		FString CurrentHealthText = FString::Printf(TEXT("%d"), FMath::CeilToInt(Health));
		FString MaxHealthText = FString::Printf(TEXT("%d"), FMath::CeilToInt(MaxHealth));
		BlasterHUD->CharacterOverlay->CurrentHealthText->SetText(FText::FromString(CurrentHealthText));
		BlasterHUD->CharacterOverlay->MaxHealthText->SetText(FText::FromString(MaxHealthText));
	}
	else
	{
		// 设置刷新tag为真，表示UI需要绘制
		bInitializeHealth = true;
		// 暂存原本需要绘制的UI的显示的数值
		HUDHealth = Health;
		HUDMaxHealth = MaxHealth;
	}
}

void ABlasterPlayerController::SetHUDShield(float Shield, float MaxShield)
{
	// 确保要操作的内容不为空指针。三元运算符，第一次执行本函数时，若为空指针，则Cast转换，后续再执行本函数时，不为空指针直接赋值旧的值即可，类似单例模式的思想
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	
	// 校验要访问的内容是否为空指针
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
			BlasterHUD->CharacterOverlay->ShieldBar &&
				BlasterHUD->CharacterOverlay->ShieldText;
	if(bHUDValid)
	{
		// 未解决的疑惑，这里为什么要使用const修饰符？
		// 计算血量百分比
		const float ShieldPercent = Shield / MaxShield;
		// 设置HUD的血量进度条
		BlasterHUD->CharacterOverlay->ShieldBar->SetPercent(ShieldPercent);
		// 准备HUD要绘制的血量文字显示。PrintF通过指定格式实例化FString类型，CeilToInt为向上取整到Int类型
		FString ShieldText = FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(Shield), FMath::CeilToInt(MaxShield));
		// 设置HUD要绘制的文字。使用FromString传入FString类型转换成FText类型。
		BlasterHUD->CharacterOverlay->ShieldText->SetText(FText::FromString(ShieldText));

		
		FString CurrentShieldText = FString::Printf(TEXT("%d"), FMath::CeilToInt(Shield));
		FString MaxShieldText = FString::Printf(TEXT("%d"), FMath::CeilToInt(MaxShield));
		BlasterHUD->CharacterOverlay->CurrentShieldText->SetText(FText::FromString(CurrentShieldText));
		BlasterHUD->CharacterOverlay->MaxShieldText->SetText(FText::FromString(MaxShieldText));
	}
	else
	{
		// 设置刷新tag为真，表示UI需要绘制
		bInitializeShield = true;
		// 暂存原本需要绘制的UI的显示的数值
		HUDShield = Shield;
		HUDMaxShield = MaxShield;
	}
}

void ABlasterPlayerController::SetHUDScore(float Score)
{
	// 转换HUD对象
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	// 校验需要访问的对象属性是否存在
	bool bHUDValid = BlasterHUD && BlasterHUD->CharacterOverlay && BlasterHUD->CharacterOverlay->ScoreAmount;
	if(bHUDValid)
	{
		// FloorToInt向下取整
		FString ScoreText = FString::Printf(TEXT("%d"), FMath::FloorToInt(Score));
		// 设置分数显示
		BlasterHUD->CharacterOverlay->ScoreAmount->SetText(FText::FromString(ScoreText));
		UE_LOG(LogTemp, Warning, TEXT("HUD已经刷新，新的Score是：%d"), FMath::FloorToInt(Score));
	}
	else
	{
		// 设置刷新tag为真，表示UI需要绘制
		bInitializeScore = true;
		// 暂存原本需要绘制的UI的显示的数值
		HUDScore = Score;
	}
}

void ABlasterPlayerController::SetHUDDefeats(int32 Defeats)
{
	// 获取当前控制器的绘制UI的对象并转换
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	// 判断获取是否成功且将要绘制的UI的组件是否存在
	bool bHUDValid = BlasterHUD && BlasterHUD->CharacterOverlay && BlasterHUD->CharacterOverlay->DefeatsAmount;
	if(bHUDValid)
	{
		// 初始化要设置的字符串，Defeats本就是int类型，无需FMath的方法转化
        FString DefeatsText = FString::Printf(TEXT("%d"), Defeats);
		// 根据设置字符串的函数的要求，转换FString类型为FText
        BlasterHUD->CharacterOverlay->DefeatsAmount->SetText(FText::FromString(DefeatsText));
	}
	else
	{
		// 设置刷新tag为真，表示UI需要绘制
		bInitializeDefeats = true;
		// 暂存原本需要绘制的UI的显示的数值
		HUDDefeats = Defeats;
	}
}

void ABlasterPlayerController::SetHUDWeaponAmmo(int32 AmmoAmount)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD && BlasterHUD->CharacterOverlay && BlasterHUD->CharacterOverlay->WeaponAmmoAmount;
	if(bHUDValid)
	{
		FString AmmoText = FString::Printf(TEXT("%d"), AmmoAmount);
		BlasterHUD->CharacterOverlay->WeaponAmmoAmount->SetText(FText::FromString(AmmoText));
	}
	else
	{
		bInitializeWeaponAmmo = true;
		HUDWeaponAmmo = AmmoAmount;
	}
}

void ABlasterPlayerController::SetHUDCarriedAmmo(int32 Ammo)
{
	// 获取HUD
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD && BlasterHUD->CharacterOverlay && BlasterHUD->CharacterOverlay->CarriedAmmoAmount;
	if(bHUDValid)
	{
		FString CarriedAmmo = FString::Printf(TEXT("%d"), Ammo);
		BlasterHUD->CharacterOverlay->CarriedAmmoAmount->SetText(FText::FromString(CarriedAmmo));
	}
	else
	{
		bInitializeCarriedAmmo = true;
		HUDCarriedAmmo = Ammo;
	}
}

void ABlasterPlayerController::SetHUDGrenades(int32 Grenades)
{
	// 获取HUD
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	// 确定HUD获取成功，HUD里面的UI控件存在且控件里面要设置的组件存在
	bool bHUDValid = BlasterHUD && BlasterHUD->CharacterOverlay && BlasterHUD->CharacterOverlay->GrenadesText;
	if(bHUDValid)
	{
		// 使用数值类型的手榴弹数，初始化得到新的Fstring
		FString GrenadesText = FString::Printf(TEXT("%d"), Grenades);
		// 刷新HUD中手榴弹的数值
		BlasterHUD->CharacterOverlay->GrenadesText->SetText(FText::FromString(GrenadesText));
	}
	// 不满足更新的条件时，可能HUD组件尚未初始化完成，需要暂存要更新的数值
	else
	{
		// 设置刷新tag为真，表示UI需要绘制
		bInitializeGrenades = true;
		// 暂存手榴弹的数量
		HUDGrenades = Grenades;
	}
}

void ABlasterPlayerController::SetHUDMatchCountdown(float CountDownTime)
{
	// 获取HUD
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	// 判断是否有效
	bool bHUDvalid = BlasterHUD && BlasterHUD->CharacterOverlay && BlasterHUD->CharacterOverlay->MatchCountdownText;
	if(bHUDvalid)
	{
		/*
		 * 因为客户端上的玩家控制器存储的比赛各种时间属性是通过RPC向服务器获取的，因此存在一种情况，那就是CountDownTime还是初始值0.f，
		 * 因为RPC请求受网络情况影响，还没走完代码逻辑，这会导致算出来的时间是负值，为了视觉感官体验，当CountDownTime为负值时不必显示本函数处理的UI
		 */
		if(CountDownTime < 0.f)
		{
			// 传入空字符串即可，设置完毕直接return，不必再走后面的代码
			BlasterHUD->CharacterOverlay->MatchCountdownText->SetText(FText());
			return;
		}
		
		// 计算分钟位的显示
		int32 Minutes = FMath::FloorToInt(CountDownTime / 60.f);
		// 计算秒数位的显示
		int32 Seconds = CountDownTime - Minutes * 60;
		// 根据分钟位和秒数位的值创建FString类型
		FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		// 设置HUD对应组件的显示
		BlasterHUD->CharacterOverlay->MatchCountdownText->SetText(FText::FromString(CountdownText));
	}
}

void ABlasterPlayerController::SetHUDAnnouncementCountdown(float CountDownTime)
{
	// 获取HUD
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	// 校验要操作的对象是否都存在，避免崩溃
	bool bHUDValid = BlasterHUD && BlasterHUD->Announcement && BlasterHUD->Announcement->WarmupTime;
	if(bHUDValid)
	{
		/*
		 * 因为客户端上的玩家控制器存储的比赛各种时间属性是通过RPC向服务器获取的，因此存在一种情况，那就是CountDownTime还是初始值0.f，
		 * 因为RPC请求受网络情况影响，还没走完代码逻辑，这会导致算出来的时间是负值，为了视觉感官体验，当CountDownTime为负值时不必显示本函数处理的UI
		 */
		if(CountDownTime < 0.f)
		{
			// 传入空字符串即可，设置完毕直接return，不必再走后面的代码
			BlasterHUD->Announcement->WarmupTime->SetText(FText());
			return;
		}
		// 计算分钟和秒数
		int32 Minutes = FMath::FloorToInt(CountDownTime / 60);
		int32 Seconds = CountDownTime - Minutes * 60;
		FString WarmupTimeText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		BlasterHUD->Announcement->WarmupTime->SetText(FText::FromString(WarmupTimeText));
	}
}

void ABlasterPlayerController::SetHUDTime()
{
	/*
	 * 游戏流程： 热身阶段（等待其他玩家加入） > 正式开始
	 */
	float TimeLeft = 0.f;
	// 当前游戏阶段处于比赛开始前，则 要显示的剩余时间 = 热身总时长 - (当前服务器时间 - 当前世界关卡的生成时间)		, 注意计算中括号的化简
	if(MatchState == MatchState::WaitingToStart) TimeLeft = WarmupTime - GetServerTime() + LevelStartingTime;
	// 当前游戏阶段处于比赛中，则 要显示的剩余时间 = 一局比赛的时长 - (当前服务器时间 - 当前世界关卡的生成时间 - 热身的总时长)
	else if(MatchState == MatchState::InProgress) TimeLeft = MatchTime - GetServerTime() + LevelStartingTime + WarmupTime;
	// 当游戏阶段处于比赛结束后的休息阶段时，则 要显示的剩余时间 = 休息阶段的总时长 - (当前服务器时间 - 当前世界关卡的生成时间 - 比赛的总时长 - 热身阶段的时长)
	else if(MatchState == MatchState::Cooldown) TimeLeft = CooldownTime - GetServerTime() + LevelStartingTime + MatchTime + WarmupTime;
	
	// 转换为int，便于整数秒之间的比较
	uint32 SecondsLeft = FMath::CeilToInt(TimeLeft);

	/*
	 * 教程中的讲解：当前机器是服务器时，可以直接访问游戏模式GameMode获取要显示的倒计时，而不必涉及 GetServerTime() 计算，这过于耗费时间，
	 * 导致服务器上的玩家的倒计时与实际倒计时误差越来越大，因此服务器上的机器需要特别处理 TimeLeft
	 * 疑惑：教程中讲解，说法似乎有问题，服务器机器的ClientServerDelta始终为0且不会被更新，GetServerTime获取的直接就是服务器时间，真的有必要特别处理TImeLeft吗？
	 */
	// 当前机器是服务器时
	if(HasAuthority())
	{
		// 刷新服务器上的视口的游戏剩余开始时间。游戏模式为空时，表示代码第一次走到这里，此时开始计算LevelStartingTime
		if (BlasterGameMode == nullptr)
		{
			BlasterGameMode = Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this));
			LevelStartingTime = BlasterGameMode->LevelStartingTime;
		}
		// 获取游戏模式并转换为本项目的游戏模式
		BlasterGameMode = BlasterGameMode == nullptr ? Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this)) : BlasterGameMode;
		// 转换成功
		if(BlasterGameMode)
		{
			// ?
			SecondsLeft = FMath::CeilToInt(BlasterGameMode->GetCountdownTime() + LevelStartingTime);
		}
	}
	
	// 只有时间标记(它等值于UI显示的时间)和计算的时间不一致时，才刷新UI的显示。（即每过一秒才执行Set设置HUD的逻辑，这种做法可以节约性能）
	if(CountdownInt != SecondsLeft)
	{
		// 开始前和赛后休息阶段的时间显示使用的是同一块UI
		if(MatchState == MatchState::WaitingToStart || MatchState == MatchState::Cooldown)
		{
			// 刷新游戏剩余时间的UI显示。注意，参数要求的是float类型，因此不能直接使用SecondsLeft
			SetHUDAnnouncementCountdown(TimeLeft);
		}
		else if(MatchState == MatchState::InProgress)
		{
			// 剩余的游戏时间 = 一局游戏时间 - 当前世界关卡已经开始的时长(每个机器都不一样，需要在别的地方做同步处理)
			// 剩余的游戏时间 = 一局游戏时间 - 服务器时间(该时间是同步处理之后的时间)
			// 刷新游戏剩余时间的UI显示。注意，参数要求的是float类型，因此不能直接使用SecondsLeft
			SetHUDMatchCountdown(TimeLeft);
		}
	}
	// 更新用于判断的时间标记
	CountdownInt = SecondsLeft;
}

void ABlasterPlayerController::ServerRequestServerTime_Implementation(float TimeOfClientRequest)
{
	/*
	 * TimeOfClientRequest: 客户端在其机器上调用本函数(发出RPC请求时)传入当前的时间。
	 * 本函数在客户端上被调用，具体执行在服务器上。
	 */
	// 获取本地机器(服务器的关卡时间)
	float ServerTimeOfReceipt = GetWorld()->GetTimeSeconds();
	// 发送RPC请求给客户端。
	ClientReportServerTime(TimeOfClientRequest, ServerTimeOfReceipt);
}

void ABlasterPlayerController::ClientReportServerTime_Implementation(float TimeOfClientRequest,
	float TimeServerReceivedClientRequest)
{
	/*
	 * TimeOfClientRequest: 客户端在其机器上调用函数ServerRequestServerTime(即发出RPC请求时)传入的当时的时间。
	 * TimeServerReceivedClientRequest: 服务器机器调用本函数时(发出RPC请求时)传入当前的时间。
	 * 本函数在服务器上被调用，具体执行在客户端上。
	 * 现实的网络情况千变万化，因此RPC请求来回的时间差是会变动的，每次都可能不一样。
	 */
	// RPC请求来回的时间差 = 本地机器(客户端)当前关卡时间 - 上次发出查询服务器关卡时间请求时的时间
	float RoundTripTime = GetWorld()->GetTimeSeconds() - TimeOfClientRequest;
	// 计算并存储通信的单程时长
	SingleTripTime = 0.5f * RoundTripTime;
	// 在本地机器(客户端)上此时可推算，服务器此时的时间 = 服务器应答时间 + 1/2的RPC请求来回的时间差
	float CurrentServerTime = TimeServerReceivedClientRequest + SingleTripTime;
	// 客户端和服务器的时间差 = 服务器此时时间 - 本地机器(客户端)当前的关卡时间
	ClientServerDelta = CurrentServerTime - GetWorld()->GetTimeSeconds();
}

float ABlasterPlayerController::GetServerTime()
{
	/*
	 * ClientServerDelta不是一个网络复制的变量，它在客户端和服务器上初始值都为0，
	 * 但客户端上的ClientServerDelta会因为ClientReportServerTime_Implementation的执行而变化。
	 * 服务器上的ClientServerDelta没有变化仍旧为0，因此本函数在客户端和服务器上的执行结果是不一样的。
	 * 如果服务器或者客户端一方的机器卡了(世界关卡时间停止计算，但现实时间仍然在流逝)，那么它们的世界关卡的时间可能就会失去同步。
	 * 因此通常需要定期同步客户端和服务器的时间。
	 */
	return GetWorld()->GetTimeSeconds() + ClientServerDelta;
}

void ABlasterPlayerController::ReceivedPlayer()
{
	/*
	 * 本函数很早就会被调用，适合在此处做一些很早期就需要完成的工作，比如客户端和服务器的世界关卡的时间同步。
	 * 虽然很早就进行了时间同步，但受制于机器性能，客户端刚启动的时候可能会卡顿，
	 * 这就导致第一个周期(第一次和第二次时间同步之间)内客户端的世界关卡时间相较于服务器的世界关卡时间落后得比较明显。
	 */ 
	Super::ReceivedPlayer();
	// 本地机器为客户端时
	if(IsLocalController())
	{
		// 客户端与服务器进行时间同步
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
	}
}

void ABlasterPlayerController::OnMatchStateSet(FName State, bool bTeamsMatch)
{
	// 玩家控制器的比赛状态注册了网络复制更新，因此其被赋值变化后，每一个MatchState被网络更新了的玩家的机器上的回调函数OnRep_MatchState会被调用。
	MatchState = State;
	/*
	 * 游戏进入游戏中的阶段，则绘制玩家UI信息。注意，本函数只在服务器上执行，此处实际是给在服务器上游玩的玩家绘制UI。
	 * 每个客户端的玩家信息需要在回调函数OnRep_MatchState中绘制。
	 */
	if(MatchState == MatchState::InProgress)
	{
		// 游戏进入"游戏中"的阶段时需要处理的UI绘制显示
		HandleMatchHasStarted(bTeamsMatch);
	}
	/*
	 * 游戏进入比赛冷却阶段
	 */
	else if(MatchState == MatchState::Cooldown)
	{
		// 游戏进入"比赛冷却"阶段时需要处理的UI绘制显示。
		HandleCooldown();
	}
}

void ABlasterPlayerController::OnRep_MatchState()
{
	// 游戏进入游戏中的阶段，则绘制玩家UI信息。
	if(MatchState == MatchState::InProgress)
	{
		// 游戏进入"游戏中"的阶段时需要处理的UI绘制显示
		HandleMatchHasStarted();
	}
	else if(MatchState == MatchState::Cooldown)
	{
		// 游戏进入"比赛冷却"阶段时需要处理的UI绘制显示。
		HandleCooldown();
	}
}

void ABlasterPlayerController::HandleMatchHasStarted(bool bTeamsMatch)
{
	// 根据是否是团队对抗，变更是否显示团队得分的tag。需要确保当前是在服务器上
	if(HasAuthority()) bShowTeamScores = bTeamsMatch;
	// 获取HUD。使用三元运算符判断，已有则使用旧值赋值，否则获取并转换
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	// 转换成功
	if(BlasterHUD)
	{
		// 绘制玩家UI信息，为了避免重复绘制，应该在为空指针的情况下才绘制
		if(BlasterHUD->CharacterOverlay == nullptr) BlasterHUD->AddCharacterOverlay();
		// 判断公告信息UI组件是否存在
		if(BlasterHUD->Announcement)
		{
			// 隐藏公告UI信息
			BlasterHUD->Announcement->SetVisibility(ESlateVisibility::Hidden);
		}
		// 非服务器上时，直接返回，后续代码的工作已在网络复制的bShowTeamScores的回调函数中完成
		if(!HasAuthority()) return;
		// 当前游戏模式为团队对抗时，初始化团队得分的显示。注意：结合本函数执行的前后位置，这里仅达到在服务器上处理团队分数的显示，客户端需要通过额外的网络复制变量得知游戏模式，从而设置团队得分的显示
		if(bTeamsMatch)
		{
			InitTeamScores();
		}
		// 否则隐藏所有团队得分
		else
		{
			HideTeamScores();
		}
	}
}

void ABlasterPlayerController::HandleCooldown()
{
	// 获取BlasterHUD
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	// 成功获取
	if(BlasterHUD)
	{
		// 原教程没有条件判断，测试中可能会访问了空指针而崩溃，这里补上
		if(BlasterHUD->CharacterOverlay)
		{
			// 移除玩家UI信息，因为比赛结束，后续不再用得到
			BlasterHUD->CharacterOverlay->RemoveFromParent();
		}
		
		// 判断相关组件UI是否存在
		bool bHUDValid = BlasterHUD->Announcement && BlasterHUD->Announcement->AnnouncementText && BlasterHUD->Announcement->InfoText;
		if(bHUDValid)
		{
			// 显示之前被隐藏的信息公告区的UI
			BlasterHUD->Announcement->SetVisibility(ESlateVisibility::Visible);
			// 修改公告区的文字
			// FString AnnouncementText = FString(TEXT("New Match Starts In:"));
			// FString AnnouncementText("New Match Starts In:");	// 不再使用硬编码
			FString AnnouncementText = Announcement::NewMatchStartsIn;
			BlasterHUD->Announcement->AnnouncementText->SetText(FText::FromString(AnnouncementText));

			// 获取游戏状态并转换为本项目的游戏状态
			ABlasterGameState* BlasterGameState = Cast<ABlasterGameState>(UGameplayStatics::GetGameState(this));
			// 获取玩家状态并转换为本项目的玩家状态
			ABlasterPlayerState* BlasterPlayerState = GetPlayerState<ABlasterPlayerState>();
			// 成功转换时
			if(BlasterGameState && BlasterPlayerState)
			{
				// 初始化一个数组用于存储排行榜的内存地址
				TArray<ABlasterPlayerState*> TopPlayers = BlasterGameState->TopScoringPlayers;
				// 根据游戏模式，获取要显示的字符串
				FString InfoTextString = bShowTeamScores ? GetTeamInfoText(BlasterGameState) : GetInfoText(TopPlayers);
				// 设置要显示的字符内容
				BlasterHUD->Announcement->InfoText->SetText(FText::FromString(InfoTextString));
			}
		}
	}
	// 获取并转换本控制器控制的pawn
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn());
	// 转换成功
	if(BlasterCharacter)
	{
		// 禁用游戏操作的tag设置为true
		BlasterCharacter->bDisableGameplay = true;
		// 比赛结束时，玩家可能正处于开火状态，因此需要停止其开火行为。
		BlasterCharacter->GetCombat()->FireButtonPressed(false);
		// 禁用角色转身（注意区分角色转身和鼠标镜头旋转是两回事。）
		
	}
}

FString ABlasterPlayerController::GetInfoText(const TArray<ABlasterPlayerState*>& Players)
{
	// 获取玩家状态并转换为本项目的玩家状态
	ABlasterPlayerState* BlasterPlayerState = GetPlayerState<ABlasterPlayerState>();
	// 获取玩家状态为空，则直接返回空字符串。
	if(BlasterPlayerState == nullptr) return FString();
	// 初始化要显示的字符串
	FString InfoTextString;
	// 排行榜为空时的显示处理
	if(Players.Num() == 0)
	{
		InfoTextString = Announcement::ThereIsNoWinner;
	}
	// 胜利者唯一，且为本控制器的玩家，则个性化显示“您是唯一胜利者！”
	else if(Players.Num() == 1 && Players[0] == BlasterPlayerState)
	{
		InfoTextString = Announcement::YouAreTheWinner;
	}
	// 胜利者唯一，是本控制器的玩家以外的其他玩家
	else if(Players.Num() == 1)
	{
		// 显示该胜利者玩家的名字。疑惑：GetPlayerName()返回的FString其实是字符串的内存地址，%s要求一个字符串的值，因此需要*号解引用读取Fstring内存地址的值
		InfoTextString = FString::Printf(TEXT("Winner: \n%s"), *Players[0]->GetPlayerName());
	}
	// 有多位胜利者时，拼接并打印所有胜利者的名字
	else if(Players.Num() > 1)
	{
		InfoTextString = Announcement::PlayersTiedForTheWin;
		InfoTextString.Append(FString("\n"));
		// 遍历并拼接胜利者的名字。从数组TopPlayers中取出单个元素，auto自动识别元素的类型
		for (auto TiedPlayer : Players)
		{
			// 拼接
			InfoTextString.Append(FString::Printf(TEXT("%s\n"), *TiedPlayer->GetPlayerName()));
		}
	}

	return InfoTextString;
}

FString ABlasterPlayerController::GetTeamInfoText(ABlasterGameState* BlasterGameState)
{
	// 游戏状态为空时，直接返回空字符创
	if(BlasterGameState == nullptr) return FString();

	// 初始化要返回的字符串
	FString InfoTextString;
	/* 
	 * 根据比赛结果的团队得分情况，拼接合适的字符串
	 */
	// 获取两队得分
	const int32 RedTeamScore = BlasterGameState->RedTeamScore;
	const int32 BlueTeamScore = BlasterGameState->BlueTeamScore;
	// 两队得分均为0时
	if(RedTeamScore == 0 && BlueTeamScore == 0)
	{
		// 显示内容：没有赢家
		InfoTextString = Announcement::ThereIsNoWinner;
	}
	// 两队得分相同，同时显示两队队名
	else if(RedTeamScore == BlueTeamScore)
	{
		// Announcement::TeamsTiedForTheWin 的值为c风格的字符串，可以视作一个数组，它指向首个元素的内存地址，需要*号解引用获取其值
		InfoTextString = FString::Printf(TEXT("%s\n"), *Announcement::TeamsTiedForTheWin);
		InfoTextString.Append(Announcement::RedTeam);
		InfoTextString.Append(FString("\n"));
		InfoTextString.Append(Announcement::BlueTeam);
		InfoTextString.Append(FString("\n"));
	}
	// 红队分数大于蓝队
	if(RedTeamScore > BlueTeamScore)
	{
		InfoTextString = Announcement::RedTeamWins;
		InfoTextString.Append(TEXT("\n\n|"));
		InfoTextString.Append(FString::Printf(TEXT("%s: %d\n"), *Announcement::RedTeam, RedTeamScore));
		InfoTextString.Append(FString::Printf(TEXT("%s: %d\n"), *Announcement::BlueTeam, BlueTeamScore));
	}
	// 蓝队分数大于红队
	else if(BlueTeamScore > RedTeamScore)
	{
		InfoTextString = Announcement::BlueTeamWins;
		InfoTextString.Append(TEXT("\n|\n"));
		InfoTextString.Append(FString::Printf(TEXT("%s: %d\n"), *Announcement::BlueTeam, BlueTeamScore));
		InfoTextString.Append(FString::Printf(TEXT("%s: %d\n"), *Announcement::RedTeam, RedTeamScore));
	}
	
	return InfoTextString;
}

void ABlasterPlayerController::BroadcastDamageNumber(float DamageNumber, AActor* HitCharacter, bool bCriticalStrike)
{
	ClientHitedDamageNumber(DamageNumber, HitCharacter, bCriticalStrike);
}

void ABlasterPlayerController::ClientHitedDamageNumber_Implementation(float DamageNumber, AActor* HitCharacter,
	bool bCriticalStrike)
{
	ABlasterHUD* AttackerHUD = Cast<ABlasterHUD>(GetHUD());
	// 调用HUD绘制伤害数字
	AttackerHUD->ShowEnemyBeHitedDamageNumber(DamageNumber, HitCharacter, bCriticalStrike);
}