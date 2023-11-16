// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// 声明宏常量（UE编辑器的项目设置中自定义了碰撞通道类型SkeletaMesh，需要将其与预留的ECC_GameTraceChannel1设置对应）
#define ECC_SkeletaMesh ECollisionChannel::ECC_GameTraceChannel1
// 声明宏常量（UE编辑器的项目设置中自定义了碰撞通道类型HitBox，需要将其与预留的ECC_GameTraceChannel2设置对应）
#define ECC_HitBox ECollisionChannel::ECC_GameTraceChannel2
