// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyBlaster/BlasterTypes/Team.h"
#include "FlagZone.generated.h"

/*
 * 本类是夺旗得分区.
 * 夺旗战：将敌人的旗子放回自己队伍的夺旗得分区，自己队伍得分。
 */

UCLASS()
class MYBLASTER_API AFlagZone : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AFlagZone();

	// 本得分区的所属队伍
	UPROPERTY(EditAnywhere)
	ETeam Team;
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 碰撞检测球体绑定的开始重叠的函数。
	UFUNCTION()
	virtual void OnSphereOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

private:
	// 碰撞检测区。
	UPROPERTY(EditAnywhere)
	class USphereComponent* ZoneSphere;
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
