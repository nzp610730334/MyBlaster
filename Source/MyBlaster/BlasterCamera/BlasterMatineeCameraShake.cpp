// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterMatineeCameraShake.h"

UBlasterMatineeCameraShake::UBlasterMatineeCameraShake()
{
	OscillationDuration = 0.2f;
	FOVOscillation.Amplitude = 1.0f;
	RotOscillation.Pitch.Amplitude = 1.f;
}
