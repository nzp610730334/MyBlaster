// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon.h"
#include "Flag.generated.h"

/**
 * 旗子类，本项目中将其视作一种特殊的武器。
 */
UCLASS()
class MYBLASTER_API AFlag : public AWeapon
{
	GENERATED_BODY()
public:
	AFlag();
	// 本子类作为旗子，使用的是静态网格体，因此父类对骨骼网格体的部分操作不适合本子类，这里将其重写
	virtual void Dropped() override;

	virtual void BeginPlay() override;

	// 重置旗子的状态
	void ResetFlag();
	
protected:
	// 旗子类使用的是静态网格体而不是骨骼网格体，因此需要添加新的静态网格体
	UPROPERTY(EditAnywhere)
	class UStaticMeshComponent* FlagMesh;
	
	// 本子类作为旗子，使用的是静态网格体，因此父类对骨骼网格体的部分操作不适合本子类，这里将其重写
	virtual void OnEquipped() override;
	virtual void OnDropped() override;

private:
	// 存储旗子的起始变换(位置、朝向、缩放)
	FTransform InitialTransform;

public:
	FORCEINLINE FTransform GetInitialTransform() const { return InitialTransform; };
};
