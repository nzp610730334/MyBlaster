// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TeamsGameMode.h"
#include "MyBlaster/CaptureTheFlag/FlagZone.h"
#include "CaptureFlagGameMode.generated.h"

/**
 * 游戏模式-团队夺旗战
 */
UCLASS()
class MYBLASTER_API ACaptureFlagGameMode : public ATeamsGameMode
{
	GENERATED_BODY()
	// 夺旗战击杀对手不得分，因此需要重写父类的击杀玩家的逻辑
	virtual void PlayerEliminated(ABlasterCharacter* ElimmedCharacter, ABlasterPlayerController* VictimController, ABlasterPlayerController* AttackerController) override;

public:
	// 夺旗得分判定.传入旗子和得分判定区
	virtual void FlagCapture(class AFlag* Flag, AFlagZone* Zone);
};
