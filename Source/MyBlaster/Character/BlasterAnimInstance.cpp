// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterAnimInstance.h"

#include "BlasterCharacter.h"
// #include "BuildSettings/Public/BuildSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "MyBlaster/BlasterComponents/CombatComponent.h"
#include "MyBlaster/Weapon/Weapon.h"

void UBlasterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	BlasterCharacter = Cast<ABlasterCharacter>(TryGetPawnOwner());
}

void UBlasterAnimInstance::NativeUpdateAnimation(float DeltaTime)
{
	Super::NativeUpdateAnimation(DeltaTime);

	if(BlasterCharacter == nullptr)
	{
		BlasterCharacter = Cast<ABlasterCharacter>(TryGetPawnOwner());
	}
	if(BlasterCharacter == nullptr) return;		// 确保调用安全

	// 这些属性的设置是为了便于动画蓝图的访问
	// 获取角色速度
	FVector Velocity = BlasterCharacter->GetVelocity();
	// z轴的速度在这里不需要
	Velocity.Z = 0.f;
	Speed = Velocity.Size();
	// 角色是否在空中
	bIsInAir = BlasterCharacter->GetCharacterMovement()->IsFalling();
	// 角色是否加速中（按下了移动键）
	bIsAccelerating = BlasterCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f ? true : false;
	// 角色是否已装备武器
	bWeaponEquipped = BlasterCharacter->IsWeaponEquipped();
	// if(!bWeaponEquipped)
	// {
	// 	UE_LOG(LogTemp, Warning, TEXT("bWeaponEquipped为false，IsWeaponEquipped函数可能返回了false"));
	// }
	
	// 获取角色装备的武器
	EquippedWeapon = BlasterCharacter->GetEquippedWeapon();
	// 角色是否下蹲中
	bIsCrouched = BlasterCharacter->bIsCrouched;
	// 角色是否举枪瞄准中
	bAiming = BlasterCharacter->IsAiming();
	// 是否需要播放转身动画
	TurningInPlace = BlasterCharacter->GetTurningInPlace();
	// 是否需要应用根骨骼动画转身
	bRotateRootBone = BlasterCharacter->ShouldRotateRootBone();
	// 角色是否已死亡
	bElimmed = BlasterCharacter->IsElimmed();
	// 玩家角色当前是否举着旗子
	bHoldingTheFlag = BlasterCharacter->IsHoldingTheFlag();

	// 获取角色基本旋转
	FRotator AimRotation = BlasterCharacter->GetBaseAimRotation();
	// UE_LOG(LogTemp, Warning, TEXT("This Character AimRotator is %f: "), AimRotation.Yaw);
	// 获取角色速度
	FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(BlasterCharacter->GetVelocity());
	// UE_LOG(LogTemp, Warning, TEXT("This Character MovementRotation is %f: "), MovementRotation.Yaw);
	// 计算差值
	FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation);
	// 计算插值
	DeltaRotation = FMath::RInterpTo(DeltaRotation, DeltaRot, DeltaTime, 5.f);
	// 该种行为会导致玩家角色从-180°正向（即-180到0到180）一路混合动画到180°，可能导致动画抽搐。
	// YawOffset = UKismetMathLibrary::NormalizedDeltaRotator(AimRotation, MovementRotation).Yaw;
	// 使用插值混合。角色动画混合从-180°可以逆向直接到180°，不会导致一路的混合流程。备注：本范围(-180°，180°)
	YawOffset = DeltaRotation.Yaw;

	// 记录上一帧角色转向
	CharacterRotationLastFrame = CharacterRotation;
	// 更新存储的角色转向
	CharacterRotation = BlasterCharacter->GetActorRotation();
	// 计算差值
	const FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(CharacterRotation, CharacterRotationLastFrame);
	// 目标值
	const float Target = Delta.Yaw / DeltaTime;
	// 插值处理
	const float Interp = FMath::FInterpTo(Lean, Target, DeltaTime, 6.f);
	// 限制大小
	Lean = FMath::Clamp(Interp, -90.f, 90.f);
	// 备注：尽量使用GetBaseAimRotation()这种已经在配置了网络复制的属性而不是新增网络复制属性，有效节省网络带宽

	// 获取角色的瞄准偏移数值
	AO_Yaw = BlasterCharacter->GetAO_Yaw();
	AO_Pitch = BlasterCharacter->GetAO_Pitch();

	// 各种判断，避免使用了空指针导致崩溃
	if(bWeaponEquipped && EquippedWeapon && EquippedWeapon->GetWeaponMesh() && BlasterCharacter->GetMesh())
	{
		LeftHandTransform = EquippedWeapon->GetWeaponMesh()->GetSocketTransform(FName("LeftHandSocket"), ERelativeTransformSpace::RTS_World);
		FVector OutPosition;
		FRotator OutRotation;
		BlasterCharacter->GetMesh()->TransformToBoneSpace(FName("hand_r"), LeftHandTransform.GetLocation(), FRotator::ZeroRotator, OutPosition, OutRotation);
		LeftHandTransform.SetLocation(OutPosition);
		LeftHandTransform.SetRotation(FQuat(OutRotation));

		/*
		 * 枪口准确指向玩家准星方向的修正，只在玩家本地修正即可，因为对玩家本人观感影响最大，玩家之间不容易看出各自的枪口是否准确朝向准星，
		 * 因为A玩家看不到B玩家的准星，因此没必要在花费网络带宽做网络复制，从而在A玩家机器上修正B玩家(模拟代理)的枪口修正
		 *
		 * 后续思考：只修正枪口，角色动画看起来观感很怪，应该修正整个角色动画朝向，通过角色转身使得枪口朝向准星瞄准的目标方向。
		 */
		// 判断该实例是否本地控制
		if(BlasterCharacter->IsLocallyControlled())
		{
			// 存储该实例是否本地控制
			bLocallyControlled = true;
			// 获取右手插槽的转换
			FTransform RightHandTransform = EquippedWeapon->GetWeaponMesh()->GetSocketTransform(FName("Hand_R"), ERelativeTransformSpace::RTS_World);
			// 计算右手为起点到准星目标的方向
			// RightHandRotation = UKismetMathLibrary::FindLookAtRotation(RightHandTransform.GetLocation(), BlasterCharacter->GetHitTarget());
			// 直接使用FindLookAtRotation获得旋转方向会导致动画瞬间变化，抽搐，通过使用插值平滑过渡变化
			// RightHandRotation = UKismetMathLibrary::FindLookAtRotation(RightHandTransform.GetLocation(), RightHandTransform.GetLocation() + (RightHandTransform.GetLocation() - BlasterCharacter->GetHitTarget()));
			// 直接使用FindLookAtRotation获得旋转方向会导致动画瞬间变化，抽搐，通过使用插值平滑过渡变化
			FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(RightHandTransform.GetLocation(), RightHandTransform.GetLocation() + (RightHandTransform.GetLocation() - BlasterCharacter->GetHitTarget()));
			// 注意RInterpTo是插值Rotator的变化，而FInterpTo是插值Fvector的变化。
			RightHandRotation = FMath::RInterpTo(RightHandRotation, LookAtRotation, DeltaTime, 40.f);
		}
		
		/*
		 * 在世界关卡中绘制线条调试。
		 */
		// // 获取武器枪口插槽的转换
		// FTransform MuzzleTipTransform = EquippedWeapon->GetWeaponMesh()->GetSocketTransform(FName("MuzzleFlash"), ERelativeTransformSpace::RTS_World);
		// // 根据枪口插槽的转换获取枪口的x轴单位长度的方向，并生成新的Vector
		// FVector MuzzleX(FRotationMatrix(MuzzleTipTransform.GetRotation().Rotator()).GetUnitAxis(EAxis::X));
		// // 在世界关卡中绘制Debug线条查看枪口方向。枪口为起点，长度1000，红色
		// DrawDebugLine(GetWorld(), MuzzleTipTransform.GetLocation(), MuzzleTipTransform.GetLocation() + MuzzleX * 1000.f, FColor::Red);
		// // 绘制子弹实际飞行轨迹
		// DrawDebugLine(GetWorld(), MuzzleTipTransform.GetLocation(), BlasterCharacter->GetHitTarget(), FColor::Orange);
	}

	/*
	 * 根据玩家角色的战斗状态设置是否应该应用左手IK。
	 * 处于待机状态中时，则应用左手IK
	 */
	bUseFABRIK = BlasterCharacter->GetCombatState() == ECombatState::ECS_Unoccupied;
	/*
	 * 玩家处于本地换弹状态时，需要额外覆盖处理bUseFABRIK的值。
	 * 本地换弹状态是为了处理高延迟下使用了客户端预测带来的客户端回退的问题，客户端回退问题只出现在本地客户端。
	 * 需要额外条件不在投掷状态中，否则投掷时bLocallyReloading也为false，则bUseFABRIK为true，左手会贴到右手手榴弹上
	 * 需要额外条件玩家角色本地切换动画即将播放完毕
	 * 疑惑；这可能会导致服务器上看其他玩家切换武器时，因为其他玩家不是在服务器本地操作，导致bFABRIKOveride为false,因此会出现切换武器时bUseFABRIK为true左手依旧使用IK的情况
	 */
	// 条件整合
	bool bFABRIKOveride = BlasterCharacter->IsLocallyControlled() &&
		BlasterCharacter->GetCombatState() != ECombatState::ECS_ThrowingGrenade &&
			BlasterCharacter->bFinishedSwapping;
	if(bFABRIKOveride)
	{
		// 是否使用手部IK与是否处于本地换弹状态相反，处于本地换弹中则不应用手部IK,
		bUseFABRIK = !BlasterCharacter->IsLocallyReloading();
	}
	
	// 玩家的游戏操作没有被禁用且非换弹动作时才需要瞄准偏移，换弹过程中不需要瞄准偏移
	bUseAimOffsets = BlasterCharacter->GetCombatState() == ECombatState::ECS_Unoccupied && !BlasterCharacter->GetDisableGameplay();
	// 玩家的游戏操作没有被禁用且非换弹动作时才需要右手转换，使得武器一直瞄准准星所向，换弹过程中不需要瞄准偏移
	bTransformRightHand = BlasterCharacter->GetCombatState() == ECombatState::ECS_Unoccupied && !BlasterCharacter->GetDisableGameplay();
}

