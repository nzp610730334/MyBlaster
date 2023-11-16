#pragma once

UENUM(BlueprintType)
enum class ECombatState : uint8
{
	/*
	 * 玩家角色的战斗状态
	 */
	// 待机状态
	ECS_Unoccupied UMETA(DisplayName = "Unoccupied"),
	// 重新装弹中
	ECS_Reloading UMETA(DisplayName = "Reloading"),
	// 投掷手榴弹状态
	ECS_ThrowingGrenade UMETA(DisplayName = "Throwing Grenade"),
	// 切换武器中
	ECS_SwappingWeapons UMETA(DisplayName = "Swapping Weapons"),
	
	ECS_MAX UMETA(DisPlayName = "DefaultMAX")
};
