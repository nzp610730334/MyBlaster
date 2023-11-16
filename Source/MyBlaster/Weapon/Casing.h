// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Casing.generated.h"

UCLASS()
class MYBLASTER_API ACasing : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ACasing();
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	// 碰撞
	UFUNCTION()
	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpilse, const FHitResult& Hit);

private:
	// 弹壳的静态网格体
	UPROPERTY(EditAnywhere)
	UStaticMeshComponent* CasingMesh;
	// 弹壳抛出的力度
	UPROPERTY(EditAnywhere)
	float ShellEjectionImpulse;
	// 弹壳碰撞声音
	UPROPERTY(EditAnywhere)
	class USoundCue* ShellSound;
	
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
