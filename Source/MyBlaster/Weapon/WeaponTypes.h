#pragma once

// 瞄准射线的检测长度
#define TRACE_LENGTH 80000.f

// 定义自定义深度颜色值
// 紫色
#define CUSTOM_DEPTH_PURPLE 250
#define CUSTOM_DEPTH_BLUE 251
// 棕褐色
#define CUSTOM_DEPTH_TAN 252


// 枚举-武器类型，需要暴露给蓝图
UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	// 步枪
	EWT_Assaulrifle UMETA(DisplayName = "Assault Rifle"),
	// 火箭发射器
	EWT_RocketLauncher UMETA(DisplayName = "Rocket Launcher"),
	// 手枪
	EWT_Pistol UMETA(DisplayName = "Pistol"),
	// 冲锋枪
	EWT_SubmachineGun UMETA(DisplayName = "Submachine Gun"),
	// 霰弹枪
	EWT_Shotgun UMETA(DisplayName = "Shotgun"),
	// 狙击枪
	EWT_SniperRifle UMETA(DisplayName = "Sniper Rifle"),
	// 榴弹枪
	EWT_GrenadeLauncher UMETA(DisplayName = "Grenade Launcher"),
	// 旗子
	EWT_Flag UMETA(DisplayName = "Flag"),
	
	EWT_Max UMETA(DisplayName = "DefaultMax")
};