#pragma once

UENUM(BlueprintType)
enum class ETurningInPlace : uint8
{
	// 左转
	ETIP_Left UMETA(DisplayName = "Turning Left"),
	// 右转
	ETIP_Right UMETA(DisplayName = "Turning Right"),
	// 原地不动
	ETIP_NotTurning UMETA(DisplayName = "Not Turning"),
	// 最大值
	ETIP_MAX UMETA(DisplayName = "DefaultMAX"),
};
