// Fill out your copyright notice in the Description page of Project Settings.


#include "AIBlasterCharacter.h"

#include "MyBlaster/BlasterComponents/AICombatComponent.h"

AAIBlasterCharacter::AAIBlasterCharacter()
{
	Combat->DestroyComponent();
	Combat = CreateDefaultSubobject<UAICombatComponent>(TEXT("AICombatComponent"));
	Combat->SetIsReplicated(true);
	Combat->Character = this;
}
