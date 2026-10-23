#include "GamePlay/PPPlayerBase.h"
#include "Component/AC_PCPlayer.h"
#include "Component/AC_VRPlayer.h"
#include "Component/AC_Task.h"
#include "GamePlay/HallPlayerController.h"
#include "GamePlay/KJPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Network/WSSubsystem.h"
#include "Tools/InfoSubsystem.h"
#include "Component/AC_CourseTransition.h"// 提供课程场景过渡组件
#include "Components/SceneCaptureComponent2D.h"// 提供像素流场景捕获组件和ShowOnlyActors
#include "Engine/GameInstance.h"// 提供GameInstance访问、提供当前客户端进程类型查询
#include "Engine/World.h"// 提供当前World访问、提供网络模式查询
#include "Camera/CameraComponent.h"// 提供BP_Player中的PCCamera2组件
#include "Camera/PlayerCameraManager.h"// 提供PC当前真正显示的相机位置
#include "GameFramework/PlayerController.h"// 提供玩家控制器
#include "GameFramework/CharacterMovementComponent.h"// 提供PC移动状态与朝向控制
#include "Animation/AnimInstance.h"// 提供Montage播放和结束回调
#include "Animation/AnimMontage.h"// 提供UAnimMontage
#include "Components/SkeletalMeshComponent.h"// 提供服务器骨骼动画Tick设置
#include "MotionWarpingComponent.h"// 提供Motion Warping组件
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"

// 必须与四个Montage中的Warp Target Name完全一致
const FName APPPlayerBase::PCTurnWarpTargetName(TEXT("PCTurnTarget"));

APPPlayerBase::APPPlayerBase()
{
	// 开启玩家Actor Tick
	PrimaryActorTick.bCanEverTick = true;

	// 为每个玩家创建课程场景过渡组件
	AC_CourseTransition = CreateDefaultSubobject<UAC_CourseTransition>(TEXT("AC_CourseTransition"));

	// 创建PC转身使用的Motion Warping组件
	PCTurnMotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("PCTurnMotionWarpingComponent"));
}

void APPPlayerBase::BeginPlay()
{
	Super::BeginPlay();

	// 当前玩家与同一场景中的其他学员玩家互相忽略移动碰撞
	if (UWorld* CurrentWorld = GetWorld())
	{
		UCapsuleComponent* CurrentCapsule = GetCapsuleComponent();

		if (IsValid(CurrentCapsule))
		{
			for (TActorIterator<APPPlayerBase> It(CurrentWorld); It; ++It)
			{
				APPPlayerBase* OtherPlayer = *It;

				if (!IsValid(OtherPlayer) || OtherPlayer == this)
				{
					continue;
				}

				// 当前玩家移动时忽略对方玩家
				CurrentCapsule->IgnoreActorWhenMoving(OtherPlayer, true);

				// 对方玩家移动时也忽略当前玩家
				if (UCapsuleComponent* OtherCapsule = OtherPlayer->GetCapsuleComponent())
				{
					OtherCapsule->IgnoreActorWhenMoving(this, true);
				}
			}
		}
	}

	// 所有网络端都持续更新玩家Montage，保证场景捕获中的远端玩家正常播放动画
	if (IsValid(GetMesh()))
	{
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	// 缓存BP_Player中用于像素流观察的PC相机组件
	CachedPCCameraComponent = FindComponentByClass<UCameraComponent>();

	// 重置PC相机上传时间
	PCCameraUploadElapsedTime = 0.0f;
}

void APPPlayerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 清理可能残留的PC转身Warp目标
	if (IsValid(PCTurnMotionWarpingComponent))
	{
		PCTurnMotionWarpingComponent->RemoveWarpTarget(PCTurnWarpTargetName);
	}

	// 清理活动Montage引用
	ActivePCTurnMontage = nullptr;

	// 执行父类结束逻辑
	Super::EndPlay(EndPlayReason);
}

void APPPlayerBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (ProcessType == EProcessType::PC_XYD)
	{
		// 更新PC原地转身检测、动画状态和网络同步
		UpdatePCTurn(DeltaTime);

		// 同步PC学员当前真正使用的相机，并在像素流端更新观察相机
		UpdatePCCameraSynchronization(DeltaTime);

	}else if (ProcessType == EProcessType::VR_XYD)
	{
		
	}else if (ProcessType == EProcessType::ViewPort)
	{
		
	}
}


void APPPlayerBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void APPPlayerBase::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(APPPlayerBase, GPlayerInfo);
	DOREPLIFETIME(APPPlayerBase, ProcessType);
	DOREPLIFETIME(APPPlayerBase, CameraT);
	DOREPLIFETIME(APPPlayerBase, VRRootT);
	DOREPLIFETIME(APPPlayerBase, RHandT);
	DOREPLIFETIME(APPPlayerBase, LHandT);
	DOREPLIFETIME(APPPlayerBase, AC_Task);
	DOREPLIFETIME(APPPlayerBase, AC_PCPlayer);
	DOREPLIFETIME(APPPlayerBase, AC_VRPlayer);
	DOREPLIFETIME(APPPlayerBase, bIsXunLianMode);
	DOREPLIFETIME(APPPlayerBase, bLeftHandIsGrab);
	DOREPLIFETIME(APPPlayerBase, bRightHandIsGrab);
	DOREPLIFETIME(APPPlayerBase, RightHandHoldType);
	DOREPLIFETIME(APPPlayerBase, bPCTurning);
	DOREPLIFETIME(APPPlayerBase, PCTurnAnimationAngle);
	DOREPLIFETIME(APPPlayerBase, bShowVRLineTrace);
}

void APPPlayerBase::OnRep_ProcessType()
{
	//TODO 待考虑拆分使用不同的玩家类
	
	UE_LOG(LogTemp, Log, TEXT("调用了onrep, 类型是 %s"), *UEnum::GetValueAsString(ProcessType));
	Start_BIE(ProcessType);
	
}

void APPPlayerBase::AttachAC_Task()
{
	if (AC_TaskClass && !AC_Task)
	{
		AC_Task = NewObject<UAC_Task>(this, AC_TaskClass, TEXT("AC_Task"));
		if (AC_Task)
		{
			AC_Task->SetIsReplicated(true);
			AC_Task->RegisterComponent();
			AC_Task->Activate();
			UE_LOG(LogTemp, Log, TEXT("动态挂载了任务AC_Task组件"));
		}
	}
}

void APPPlayerBase::RPC_ServerPCSetCameraTransform_Implementation(FTransform LPCameraTransform)
{
	// 只允许PC学员更新PC监控相机数据
	if (ProcessType != EProcessType::PC_XYD)
	{
		return;
	}

	// 拒绝包含非法数值的相机变换
	if (LPCameraTransform.ContainsNaN())
	{
		return;
	}

	// 相机同步不需要使用客户端传入的缩放
	LPCameraTransform.SetScale3D(FVector::OneVector);

	// 保存PC学员当前实际相机变换，现有复制代码会将它同步给像素流客户端
	CameraT = LPCameraTransform;
}

void APPPlayerBase::SetVRLineTraceVisible(bool bEnabled)
{
	// 只允许本地所属学员或服务器修改
	if (!IsLocallyControlled() && !HasAuthority())
	{
		return;
	}

	// 本地立即更新，避免VR端等待网络复制
	bShowVRLineTrace = bEnabled;

	// 服务器直接保存并立即复制
	if (HasAuthority())
	{
		ForceNetUpdate();
		return;
	}

	// VR客户端将状态上传服务器
	RPC_ServerSetVRLineTraceVisible(bEnabled);
}

void APPPlayerBase::RPC_ServerSetVRLineTraceVisible_Implementation(bool bEnabled)
{
	// 只有VR学员需要同步射线
	if (ProcessType != EProcessType::VR_XYD)
	{
		return;
	}

	bShowVRLineTrace = bEnabled;

	// 立即向像素流客户端复制
	ForceNetUpdate();
}

void APPPlayerBase::LocalSetHandState(bool LIsRightHand, bool LIsGrab)
{
	if (IsLocallyControlled())
	{
		RPC_ServerSetHandState(LIsRightHand, LIsGrab);
	}
}

void APPPlayerBase::LocalSetRightHoldType(EHandHoldType LType)
{
	if (IsLocallyControlled())
	{
		RPC_ServerSetRightHandHoldType(LType);
	}
}

void APPPlayerBase::RPC_ServerSetRightHandHoldType_Implementation(EHandHoldType LType)
{
	RightHandHoldType = LType;
	OnRep_RightHandHoldType();
}

void APPPlayerBase::OnRep_RightHandHoldType()
{
	BIE_RightHandHoldType(RightHandHoldType);
}

void APPPlayerBase::RPC_ServerSetHandState_Implementation(bool LIsRightHand, bool LIsGrab)
{
	if (LIsRightHand)
	{
		bRightHandIsGrab = LIsGrab;
	}
	else
	{
		bLeftHandIsGrab = LIsGrab;
	}
}


void APPPlayerBase::RPC_ServerVRSetLocationData_Implementation(FTransform LCameraT, FTransform LVRRootT,
                                                               FTransform LRHand, FTransform LLHand)
{
	CameraT = LCameraT;
	VRRootT = LVRRootT;
	RHandT = LRHand;
	LHandT = LLHand;
	
}

// 将指定Actor添加到当前学员的像素流场景捕获白名单
bool APPPlayerBase::RegisterActorToXSLShowOnly(AActor* ActorToShow)
{
	// Actor无效时不能加入捕获列表
	if (!IsValid(ActorToShow))
	{
		UE_LOG(LogTemp, Error, TEXT("像素流显示Actor注册失败：Actor无效，学员=%s"), *GPlayerInfo.SN);
		return false;
	}

	// 获取当前World
	UWorld* CurrentWorld = GetWorld();

	// World无效时不能判断当前客户端类型
	if (!IsValid(CurrentWorld))
	{
		return false;
	}

	// 获取当前客户端的GameInstance
	UGameInstance* CurrentGameInstance = CurrentWorld->GetGameInstance();

	// GameInstance无效时不能取得信息子系统
	if (!IsValid(CurrentGameInstance))
	{
		return false;
	}

	// 获取当前客户端的信息子系统
	UInfoSubsystem* InfoSubsystem = CurrentGameInstance->GetSubsystem<UInfoSubsystem>();

	// 只有像素流客户端需要维护SceneCapture的ShowOnlyActors
	if (!IsValid(InfoSubsystem) || InfoSubsystem->ProcessType != EProcessType::ViewPort)
	{
		return false;
	}

	// 从当前学员Pawn上取得用于生成该学员监控画面的场景捕获组件
	USceneCaptureComponent2D* XSLSceneCaptureComponent = FindComponentByClass<USceneCaptureComponent2D>();

	// 没有找到场景捕获组件时无法显示Actor
	if (!IsValid(XSLSceneCaptureComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("像素流显示Actor注册失败：学员没有SceneCaptureComponent2D，学员=%s，Actor=%s"), *GPlayerInfo.SN, *GetNameSafe(ActorToShow));
		return false;
	}

	// 避免同一个Actor被重复添加
	XSLSceneCaptureComponent->ShowOnlyActors.AddUnique(ActorToShow);

	// 通知渲染线程更新场景捕获状态
	XSLSceneCaptureComponent->MarkRenderStateDirty();

	// 输出注册成功日志
	UE_LOG(LogTemp, Warning, TEXT("像素流显示Actor注册成功：学员=%s，Actor=%s"), *GPlayerInfo.SN, *GetNameSafe(ActorToShow));

	// 返回注册成功
	return true;
}

// 将指定Actor从当前学员的像素流场景捕获白名单移除
void APPPlayerBase::UnregisterActorFromXSLShowOnly(AActor* ActorToRemove)
{
	// Actor指针为空时不执行
	if (ActorToRemove == nullptr)
	{
		return;
	}

	// 获取当前World
	UWorld* CurrentWorld = GetWorld();

	// World无效时不继续处理
	if (!IsValid(CurrentWorld))
	{
		return;
	}

	// 获取当前客户端的GameInstance
	UGameInstance* CurrentGameInstance = CurrentWorld->GetGameInstance();

	// GameInstance无效时不继续处理
	if (!IsValid(CurrentGameInstance))
	{
		return;
	}

	// 获取当前客户端的信息子系统
	UInfoSubsystem* InfoSubsystem = CurrentGameInstance->GetSubsystem<UInfoSubsystem>();

	// 非像素流客户端不需要维护ShowOnlyActors
	if (!IsValid(InfoSubsystem) || InfoSubsystem->ProcessType != EProcessType::ViewPort)
	{
		return;
	}

	// 取得当前学员Pawn上的场景捕获组件
	USceneCaptureComponent2D* XSLSceneCaptureComponent = FindComponentByClass<USceneCaptureComponent2D>();

	// 场景捕获组件无效时直接返回
	if (!IsValid(XSLSceneCaptureComponent))
	{
		return;
	}

	// 从像素流捕获白名单中移除指定Actor
	XSLSceneCaptureComponent->ShowOnlyActors.Remove(ActorToRemove);

	// 通知渲染线程更新场景捕获状态
	XSLSceneCaptureComponent->MarkRenderStateDirty();

	// 输出取消注册日志
	UE_LOG(LogTemp, Warning, TEXT("像素流显示Actor取消注册：学员=%s，Actor=%s"), *GPlayerInfo.SN, *GetNameSafe(ActorToRemove));
}

// 判断当前Pawn是否为允许驱动PC转身的本地学员
bool APPPlayerBase::ShouldRunLocalPCTurn() const
{
	// Dedicated Server不能作为本地玩家驱动动画
	if (GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	// 只有本地控制的PC学员Pawn允许检测控制器转身
	return ProcessType == EProcessType::PC_XYD && IsLocallyControlled();
}

// 判断PC玩家当前是否正在移动
bool APPPlayerBase::IsPCTurnMovementActive() const
{
	// 获取角色移动组件
	const UCharacterMovementComponent* PCTurnMovementComponent = GetCharacterMovement();

	// 移动组件无效时不认为玩家正在移动
	if (!IsValid(PCTurnMovementComponent))
	{
		return false;
	}

	// 判断角色当前是否具有实际平面移动速度
	const bool bHasMovementVelocity = PCTurnMovementComponent->Velocity.SizeSquared2D() > FMath::Square(PCTurnMovementSpeedThreshold);

	// 判断角色上一帧是否接收到移动输入
	const bool bHasMovementInput = GetLastMovementInputVector().SizeSquared2D() > KINDA_SMALL_NUMBER;

	// 具有速度或移动输入时都不能触发原地转身
	return bHasMovementVelocity || bHasMovementInput;
}

// 判断当前实例是否存在活动的PC转身
bool APPPlayerBase::IsPCTurnActive() const
{
	// 服务器转身状态或本地预测Montage任意一个有效都代表正在转身
	return bPCTurning || IsValid(ActivePCTurnMontage);
}

// 每帧更新PC原地转身
void APPPlayerBase::UpdatePCTurn(float DeltaTime)
{
	// 获取角色移动组件
	UCharacterMovementComponent* PCTurnMovementComponent = GetCharacterMovement();

	// 判断当前是否正在移动
	const bool bIsMoving = IsPCTurnMovementActive();

	// 判断当前是否正在执行转身Montage
	const bool bTurnActive = IsPCTurnActive();

	// 服务器负责产生最终权威朝向，所属客户端负责本地即时表现。
	// 像素流端等模拟代理只接收服务器同步结果，不在本地驱动角色旋转。
	const bool bCanDriveMovementRotation = HasAuthority() || IsLocallyControlled();

	// 正常移动时，让服务器和所属客户端都朝向控制器方向。
	// Root Motion转身期间关闭自动旋转，避免与Motion Warping争夺朝向。
	if (bCanDriveMovementRotation && IsValid(PCTurnMovementComponent))
	{
		PCTurnMovementComponent->bUseControllerDesiredRotation =
			bIsMoving && !bTurnActive;
	}

	// 原地转身的角度检测仍然只允许所属PC客户端执行。
	// 服务器不能在这里重复发起转身。
	if (!ShouldRunLocalPCTurn())
	{
		return;
	}

	// Montage正在播放或等待服务器结束时不重复开始
	if (bTurnActive)
	{
		return;
	}

	// 移动过程中不触发原地转身
	if (bIsMoving)
	{
		PCTurnDecisionElapsedTime = 0.0f;
		PCTurnDecisionDirection = 0.0f;
		return;
	}

	// 没有控制器时无法取得视角方向
	if (!IsValid(GetController()))
	{
		PCTurnDecisionElapsedTime = 0.0f;
		PCTurnDecisionDirection = 0.0f;
		return;
	}

	// 计算Actor朝向到控制器朝向之间的最短Yaw差值
	const float LRawYawDifference = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, GetControlRotation().Yaw);

	// 获取真实Yaw差值的绝对值
	const float LAbsoluteYawDifference = FMath::Abs(LRawYawDifference);

	// 未达到触发角度时重置判断状态
	if (LAbsoluteYawDifference < PCTurnTriggerAngle)
	{
		PCTurnDecisionElapsedTime = 0.0f;
		PCTurnDecisionDirection = 0.0f;
		return;
	}

	// 达到180度阈值时立即开始转身
	if (LAbsoluteYawDifference >= PCTurn180Threshold)
	{
		StartLocalPCTurn(LRawYawDifference);
		return;
	}

	// 保存当前左右转身方向
	const float LCurrentDirection = LRawYawDifference >= 0.0f ? 1.0f : -1.0f;

	// 判断期间改变左右方向时重新计时
	if (!FMath::IsNearlyEqual(PCTurnDecisionDirection, LCurrentDirection))
	{
		PCTurnDecisionDirection = LCurrentDirection;
		PCTurnDecisionElapsedTime = 0.0f;
	}

	// 累计视角稳定等待时间
	PCTurnDecisionElapsedTime += DeltaTime;

	// 尚未达到稳定等待时间
	if (PCTurnDecisionElapsedTime < PCTurnDecisionDelay)
	{
		return;
	}

	// 开始一次PC原地转身
	StartLocalPCTurn(LRawYawDifference);
}

// 本地开始一次PC原地转身
void APPPlayerBase::StartLocalPCTurn(float LRawYawDifference)
{
	// 只有本地PC学员能够主动开始转身
	if (!ShouldRunLocalPCTurn() || IsPCTurnActive())
	{
		return;
	}

	// 根据真实角度选择90度或180度动画
	const float LTurnMagnitude = FMath::Abs(LRawYawDifference) >= PCTurn180Threshold ? 180.0f : 90.0f;

	// 根据Yaw正负选择左右动画
	const float LTurnDirection = LRawYawDifference >= 0.0f ? 1.0f : -1.0f;

	// 生成固定的Montage选择角度
	const float LTurnAnimationAngle = LTurnMagnitude * LTurnDirection;

	// 查找对应的转身Montage
	UAnimMontage* LSelectedMontage = SelectPCTurnMontage(LTurnAnimationAngle);

	// 没有配置对应Montage时不能开始
	if (!IsValid(LSelectedMontage))
	{
		UE_LOG(LogTemp, Error, TEXT("PC原地转身开始失败：没有配置对应Montage，玩家=%s，动画角度=%.1f"), *GetNameSafe(this), LTurnAnimationAngle);
		return;
	}

	// 锁定转身开始时的准确控制器方向
	const float LTargetYaw = FRotator::NormalizeAxis(GetControlRotation().Yaw);

	// 重置判断状态
	PCTurnDecisionElapsedTime = 0.0f;
	PCTurnDecisionDirection = 0.0f;

	// ListenServer和单机直接开始服务器权威Montage
	if (HasAuthority())
	{
		ApplyServerPCTurnStart(LTurnAnimationAngle, LTargetYaw);
		return;
	}

	// 本地保存动画角度用于日志和显示
	PCTurnAnimationAngle = LTurnAnimationAngle;

	// 本地保存本次锁定的准确方向
	PCTurnExactTargetYaw = LTargetYaw;

	// 本地立即预测播放Montage，避免等待服务器往返
	if (!PlayPCTurnMontage(LSelectedMontage, LTargetYaw))
	{
		return;
	}

	// 请求服务器播放相同的权威Root Motion Montage
	RPC_ServerBeginPCTurn(LTurnAnimationAngle, LTargetYaw);

	UE_LOG(LogTemp, Warning, TEXT("PC开始Root Motion原地转身：玩家=%s，真实Yaw差值=%.1f，Montage角度=%.1f，锁定目标Yaw=%.1f，Montage=%s"), *GetNameSafe(this), LRawYawDifference, LTurnAnimationAngle, LTargetYaw, *GetNameSafe(LSelectedMontage));
}

// 根据固定动画角度取得对应的转身Montage
UAnimMontage* APPPlayerBase::SelectPCTurnMontage(float LTurnAnimationAngle) const
{
	// 判断是否使用180度Montage
	const bool bUse180Montage = FMath::Abs(LTurnAnimationAngle) >= 135.0f;

	// 判断是否向右转身
	const bool bTurnRight = LTurnAnimationAngle > 0.0f;

	// 返回右转180度Montage
	if (bUse180Montage && bTurnRight)
	{
		return PCTurnRight180Montage;
	}

	// 返回左转180度Montage
	if (bUse180Montage)
	{
		return PCTurnLeft180Montage;
	}

	// 返回右转90度Montage
	if (bTurnRight)
	{
		return PCTurnRight90Montage;
	}

	// 返回左转90度Montage
	return PCTurnLeft90Montage;
}

// 设置Warp目标并播放转身Montage
bool APPPlayerBase::PlayPCTurnMontage(UAnimMontage* LMontage, float LTargetYaw)
{
	// 检查Montage和Motion Warping组件
	if (!IsValid(LMontage) || !IsValid(PCTurnMotionWarpingComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("PC转身Montage播放失败：Montage或MotionWarping组件无效，玩家=%s"), *GetNameSafe(this));
		return false;
	}

	// 获取玩家骨骼网格体
	USkeletalMeshComponent* PlayerMesh = GetMesh();

	// 骨骼网格体无效时不能播放Montage
	if (!IsValid(PlayerMesh))
	{
		UE_LOG(LogTemp, Error, TEXT("PC转身Montage播放失败：玩家骨骼网格体无效，玩家=%s"), *GetNameSafe(this));
		return false;
	}

	// 获取玩家动画实例
	UAnimInstance* PlayerAnimInstance = PlayerMesh->GetAnimInstance();

	// 动画实例无效时不能播放Montage
	if (!IsValid(PlayerAnimInstance))
	{
		UE_LOG(LogTemp, Error, TEXT("PC转身Montage播放失败：动画实例无效，玩家=%s"), *GetNameSafe(this));
		return false;
	}

	// 创建只改变Yaw的目标旋转
	const FRotator LTargetRotation = FRotator(0.0f, FRotator::NormalizeAxis(LTargetYaw), 0.0f);

	// 使用角色当前位置和锁定方向创建Warp目标
	const FTransform LTargetTransform = FTransform(LTargetRotation, GetActorLocation(), FVector::OneVector);

	// 注册或更新Montage中名为PCTurnTarget的Warp目标
	PCTurnMotionWarpingComponent->AddOrUpdateWarpTargetFromTransform(PCTurnWarpTargetName, LTargetTransform);

	// 暂存转身开始前的控制器朝向设置
	UCharacterMovementComponent* PCTurnMovementComponent = GetCharacterMovement();

	// Root Motion转身期间关闭CharacterMovement自动旋转
	if (IsValid(PCTurnMovementComponent))
	{
		bSavedUseControllerDesiredRotation = PCTurnMovementComponent->bUseControllerDesiredRotation;
		PCTurnMovementComponent->bUseControllerDesiredRotation = false;
	}

	// 播放Montage但不停止其他组中的Montage
	const float LPlayedMontageLength = PlayerAnimInstance->Montage_Play(LMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, false);

	// Montage播放失败时恢复状态
	if (LPlayedMontageLength <= 0.0f)
	{
		PCTurnMotionWarpingComponent->RemoveWarpTarget(PCTurnWarpTargetName);

		if (IsValid(PCTurnMovementComponent))
		{
			PCTurnMovementComponent->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
		}

		UE_LOG(LogTemp, Error, TEXT("PC转身Montage播放失败：Montage_Play返回值无效，玩家=%s，Montage=%s"), *GetNameSafe(this), *GetNameSafe(LMontage));
		return false;
	}

	// 保存当前正在播放的转身Montage
	ActivePCTurnMontage = LMontage;

	// 创建Montage结束委托
	FOnMontageEnded LPCTurnMontageEndedDelegate;

	// 将结束委托绑定到当前玩家
	LPCTurnMontageEndedDelegate.BindUObject(this, &APPPlayerBase::HandlePCTurnMontageEnded);

	// 将结束委托设置到刚开始播放的Montage实例
	PlayerAnimInstance->Montage_SetEndDelegate(LPCTurnMontageEndedDelegate, LMontage);

	// 返回播放成功
	return true;
}

// 接收转身Montage播放结束回调
void APPPlayerBase::HandlePCTurnMontageEnded(UAnimMontage* LMontage, bool bInterrupted)
{
	// 忽略非当前转身Montage的结束回调
	if (!IsValid(LMontage) || LMontage != ActivePCTurnMontage)
	{
		return;
	}

	// 保存结束时的Actor实际Yaw
	const float LFinalActorYaw = FRotator::NormalizeAxis(GetActorRotation().Yaw);

	// 计算Root Motion最终方向与锁定目标之间的误差
	const float LFinalYawError = FMath::Abs(FMath::FindDeltaAngleDegrees(LFinalActorYaw, PCTurnExactTargetYaw));

	// 清理Motion Warping目标
	if (IsValid(PCTurnMotionWarpingComponent))
	{
		PCTurnMotionWarpingComponent->RemoveWarpTarget(PCTurnWarpTargetName);
	}

	// 清理当前活动Montage
	ActivePCTurnMontage = nullptr;

	// 恢复CharacterMovement原来的控制器朝向设置
	UCharacterMovementComponent* PCTurnMovementComponent = GetCharacterMovement();

	// 移动组件有效时恢复设置
	if (IsValid(PCTurnMovementComponent))
	{
		PCTurnMovementComponent->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
	}

	// 清理下一次转身的判断状态
	PCTurnDecisionElapsedTime = 0.0f;
	PCTurnDecisionDirection = 0.0f;

	// 服务器结束权威转身状态
	if (HasAuthority())
	{
		bPCTurning = false;
		ForceNetUpdate();
	}

	//UE_LOG(LogTemp, Warning, TEXT("PC Root Motion转身Montage结束：玩家=%s，Montage=%s，被打断=%s，最终Yaw=%.1f，目标Yaw=%.1f，Yaw误差=%.2f"), *GetNameSafe(this), *GetNameSafe(LMontage), bInterrupted ? TEXT("是") : TEXT("否"), LFinalActorYaw, PCTurnExactTargetYaw, LFinalYawError);
}

// 客户端请求服务器开始权威转身Montage
void APPPlayerBase::RPC_ServerBeginPCTurn_Implementation(float LTurnAnimationAngle, float LTargetYaw)
{
	// 服务器校验并开始转身
	ApplyServerPCTurnStart(LTurnAnimationAngle, LTargetYaw);
}

// 服务器通知远端客户端播放PC转身表现Montage
void APPPlayerBase::RPC_MulticastPlayPCTurnMontage_Implementation(float LTurnAnimationAngle, float LTargetYaw)
{
	// 服务器已经播放了权威Root Motion Montage，不重复播放
	if (HasAuthority())
	{
		return;
	}

	// 所属PC客户端已经进行了本地预测播放，不重复播放
	if (IsLocallyControlled())
	{
		return;
	}

	// 只处理PC学员，并拒绝非法数值
	if (ProcessType != EProcessType::PC_XYD || !FMath::IsFinite(LTurnAnimationAngle) || !FMath::IsFinite(LTargetYaw))
	{
		return;
	}

	// 已经通过本组播播放转身Montage时不重复开始
	if (IsValid(ActivePCTurnMontage))
	{
		return;
	}

	// 获取客户端请求的固定动画角度绝对值
	const float LRequestedMagnitude = FMath::Abs(LTurnAnimationAngle);

	// 只接受90度和180度转身Montage
	if (!FMath::IsNearlyEqual(LRequestedMagnitude, 90.0f, 1.0f) && !FMath::IsNearlyEqual(LRequestedMagnitude, 180.0f, 1.0f))
	{
		return;
	}

	// 生成经过校验的固定动画角度
	const float LValidatedAnimationAngle = LTurnAnimationAngle >= 0.0f ? LRequestedMagnitude : -LRequestedMagnitude;

	// 根据固定角度取得远端客户端需要播放的Montage
	UAnimMontage* LSelectedMontage = SelectPCTurnMontage(LValidatedAnimationAngle);

	// 远端客户端没有配置对应Montage时结束
	if (!IsValid(LSelectedMontage))
	{
		UE_LOG(LogTemp, Error, TEXT("远端PC转身播放失败：没有配置对应Montage，玩家=%s，动画角度=%.1f"), *GetNameSafe(this), LValidatedAnimationAngle);
		return;
	}

	// 保存远端客户端的转身动画角度
	PCTurnAnimationAngle = LValidatedAnimationAngle;

	// 保存远端客户端的Motion Warping目标方向
	PCTurnExactTargetYaw = FRotator::NormalizeAxis(LTargetYaw);

	// 在像素流端等远端客户端播放相同的转身Montage
	if (!PlayPCTurnMontage(LSelectedMontage, PCTurnExactTargetYaw))
	{
		UE_LOG(LogTemp, Error, TEXT("远端PC转身Montage播放失败：玩家=%s，Montage=%s"), *GetNameSafe(this), *GetNameSafe(LSelectedMontage));
		return;
	}

	// 输出远端播放成功日志
	UE_LOG(LogTemp, Warning, TEXT("远端客户端开始PC转身Montage：玩家=%s，角度=%.1f，目标Yaw=%.1f，Montage=%s"), *GetNameSafe(this), PCTurnAnimationAngle, PCTurnExactTargetYaw, *GetNameSafe(LSelectedMontage));
}

// 服务器校验并开始权威转身Montage
void APPPlayerBase::ApplyServerPCTurnStart(float LTurnAnimationAngle, float LTargetYaw)
{
	// 只有服务器能够执行权威转身
	if (!HasAuthority())
	{
		return;
	}

	// 拒绝非PC玩家和非法数值
	if (ProcessType != EProcessType::PC_XYD || !FMath::IsFinite(LTurnAnimationAngle) || !FMath::IsFinite(LTargetYaw))
	{
		return;
	}

	// 已有活动转身时拒绝重复开始
	if (IsPCTurnActive())
	{
		return;
	}

	// 获取客户端请求的动画角度绝对值
	const float LRequestedMagnitude = FMath::Abs(LTurnAnimationAngle);

	// 只允许固定的90度和180度Montage
	if (!FMath::IsNearlyEqual(LRequestedMagnitude, 90.0f, 1.0f) && !FMath::IsNearlyEqual(LRequestedMagnitude, 180.0f, 1.0f))
	{
		return;
	}

	// 生成经过服务器校验的固定动画角度
	const float LValidatedAnimationAngle = LTurnAnimationAngle >= 0.0f ? LRequestedMagnitude : -LRequestedMagnitude;

	// 获取服务器需要播放的转身Montage
	UAnimMontage* LSelectedMontage = SelectPCTurnMontage(LValidatedAnimationAngle);

	// 服务器没有配置Montage时拒绝开始
	if (!IsValid(LSelectedMontage))
	{
		UE_LOG(LogTemp, Error, TEXT("服务器开始PC转身失败：没有配置对应Montage，玩家=%s，动画角度=%.1f"), *GetNameSafe(this), LValidatedAnimationAngle);
		return;
	}

	// 保存经过校验的固定动画角度
	PCTurnAnimationAngle = LValidatedAnimationAngle;

	// 保存标准化后的准确目标方向
	PCTurnExactTargetYaw = FRotator::NormalizeAxis(LTargetYaw);

	// 设置服务器权威转身状态
	bPCTurning = true;

	// 在服务器播放会产生权威Root Motion的Montage
	if (!PlayPCTurnMontage(LSelectedMontage, PCTurnExactTargetYaw))
	{
		bPCTurning = false;
		ForceNetUpdate();
		return;
	}

	// 通知像素流端和其他远端客户端播放相同的转身Montage
	RPC_MulticastPlayPCTurnMontage(PCTurnAnimationAngle, PCTurnExactTargetYaw);

	// 立即复制转身状态
	ForceNetUpdate();

	//UE_LOG(LogTemp, Warning, TEXT("服务器开始PC Root Motion转身：玩家=%s，Montage角度=%.1f，目标Yaw=%.1f，Montage=%s"), *GetNameSafe(this), PCTurnAnimationAngle, PCTurnExactTargetYaw, *GetNameSafe(LSelectedMontage));
}

// 处理PC相机上传和像素流端相机追踪
void APPPlayerBase::UpdatePCCameraSynchronization(float DeltaTime)
{
	// 只有PC学员Pawn需要执行这套相机同步
	if (ProcessType != EProcessType::PC_XYD)
	{
		return;
	}

	// 获取当前World
	UWorld* CurrentWorld = GetWorld();

	// World无效时不能继续处理
	if (!IsValid(CurrentWorld))
	{
		return;
	}

	// 单机模式不需要通过服务器同步观察相机
	if (CurrentWorld->GetNetMode() == NM_Standalone)
	{
		return;
	}

	// PC学员自己的本地客户端负责上传当前实际相机
	if (IsLocallyControlled())
	{
		UploadLocalPCCameraTransform(DeltaTime);
		return;
	}

	// 获取当前客户端的GameInstance
	UGameInstance* CurrentGameInstance = CurrentWorld->GetGameInstance();

	// GameInstance无效时不能判断当前客户端类型
	if (!IsValid(CurrentGameInstance))
	{
		return;
	}

	// 获取当前客户端的信息子系统
	UInfoSubsystem* InfoSubsystem = CurrentGameInstance->GetSubsystem<UInfoSubsystem>();

	// 只有像素流客户端需要使用CameraT更新远端学员的观察相机
	if (!IsValid(InfoSubsystem) || InfoSubsystem->ProcessType != EProcessType::ViewPort)
	{
		return;
	}

	// 将像素流观察相机更新到服务器复制的位置
	ApplyReplicatedPCCameraTransform(DeltaTime);
}

// PC学员本地端上传当前真正用于显示画面的相机变换
void APPPlayerBase::UploadLocalPCCameraTransform(float DeltaTime)
{
	// 只有PC学员Pawn才允许进入相机上传逻辑
	if (ProcessType != EProcessType::PC_XYD)
	{
		return;
	}

	// 只有当前机器本地控制的Pawn才允许上传相机
	if (!IsLocallyControlled())
	{
		return;
	}

	// 判断当前Pawn是否为服务器分配给所属客户端的自主代理
	const bool bIsOwningClientPawn =
		GetNetMode() == NM_Client &&
		GetLocalRole() == ROLE_AutonomousProxy;

	// 判断当前Pawn是否为ListenServer本地直接控制的权威Pawn
	const bool bIsListenServerLocalPawn =
		GetNetMode() == NM_ListenServer &&
		HasAuthority();

	// 排除像素流客户端本地创建但没有服务器所有权的Pawn
	if (!bIsOwningClientPawn && !bIsListenServerLocalPawn)
	{
		return;
	}

	// 累加距离上一次上传经过的时间
	PCCameraUploadElapsedTime += DeltaTime;

	// 防止配置的上传间隔为零
	const float SafeUploadInterval = FMath::Max(PCCameraUploadInterval, 0.01f);

	// 尚未达到上传间隔时不发送RPC
	if (PCCameraUploadElapsedTime < SafeUploadInterval)
	{
		return;
	}

	// 重置上传累计时间
	PCCameraUploadElapsedTime = 0.0f;

	// 获取当前Pawn的本地玩家控制器
	APlayerController* PlayerController = Cast<APlayerController>(GetController());

	// 玩家控制器或相机管理器无效时不能取得真实画面位置
	if (!IsValid(PlayerController) || !IsValid(PlayerController->PlayerCameraManager))
	{
		return;
	}

	// 获取当前真正用于显示画面的相机位置
	const FVector CurrentCameraLocation =
		PlayerController->PlayerCameraManager->GetCameraLocation();

	// 获取当前真正用于显示画面的相机旋转
	const FRotator CurrentCameraRotation =
		PlayerController->PlayerCameraManager->GetCameraRotation();

	// 组合当前实际相机的世界变换
	const FTransform CurrentCameraTransform =
		FTransform(CurrentCameraRotation, CurrentCameraLocation, FVector::OneVector);

	// 只有拥有服务器网络连接的PC学员Pawn才能执行该Server RPC
	RPC_ServerPCSetCameraTransform(CurrentCameraTransform);
}

// 像素流端将观察相机更新到服务器同步的位置
void APPPlayerBase::ApplyReplicatedPCCameraTransform(float DeltaTime)
{
	// CameraT尚未收到有效数据时不能更新相机
	if (CameraT.Equals(FTransform::Identity) || CameraT.ContainsNaN())
	{
		return;
	}

	// 缓存失效时重新查找BP_Player中的PCCamera2
	if (!IsValid(CachedPCCameraComponent))
	{
		CachedPCCameraComponent = FindComponentByClass<UCameraComponent>();
		bPCCameraUsesAbsoluteTransform = false;
	}

	// 没有找到PCCamera2时不能更新观察相机
	if (!IsValid(CachedPCCameraComponent))
	{
		return;
	}

	// 像素流观察相机不能继续继承远端玩家Actor的位置和旋转
	if (!bPCCameraUsesAbsoluteTransform)
	{
		// 保存切换前的相机世界变换
		const FTransform LCurrentCameraWorldTransform = CachedPCCameraComponent->GetComponentTransform();

		// 位置和旋转改为绝对世界变换，但保留原有缩放继承
		CachedPCCameraComponent->SetAbsolute(true, true, false);

		// 恢复切换前的世界变换，避免切换当帧发生跳动
		CachedPCCameraComponent->SetWorldTransform(LCurrentCameraWorldTransform, false, nullptr, ETeleportType::TeleportPhysics);

		// 标记本次设置已经完成
		bPCCameraUsesAbsoluteTransform = true;

		// 输出一次诊断日志，确认实际找到的相机组件
		UE_LOG(LogTemp, Warning, TEXT("像素流PC观察相机已切换为绝对世界变换：学员=%s，相机=%s"), *GPlayerInfo.SN, *GetNameSafe(CachedPCCameraComponent));
	}

	// 获取服务器同步的目标相机位置
	const FVector TargetCameraLocation = CameraT.GetLocation();

	// 获取服务器同步的目标相机旋转
	const FRotator TargetCameraRotation = CameraT.Rotator();

	// 获取像素流观察相机当前的位置
	const FVector CurrentCameraLocation = CachedPCCameraComponent->GetComponentLocation();

	// 计算当前观察相机与目标相机的距离平方
	const float CameraDistanceSquared = FVector::DistSquared(CurrentCameraLocation, TargetCameraLocation);

	// 防止瞬移距离配置为负数
	const float SafeSnapDistance = FMath::Max(PCCameraSnapDistance, 0.0f);

	// 判断是否为上车、传送等大距离相机切换
	const bool bShouldSnapCamera = SafeSnapDistance <= KINDA_SMALL_NUMBER || CameraDistanceSquared > FMath::Square(SafeSnapDistance);

	// 大距离切换时立即设置目标位置，避免监控相机从警察局飞向警车
	if (bShouldSnapCamera)
	{
		CachedPCCameraComponent->SetWorldLocationAndRotation(TargetCameraLocation, TargetCameraRotation);
		return;
	}

	// 插值速度为零时直接设置服务器同步的相机变换
	if (PCCameraInterpolationSpeed <= KINDA_SMALL_NUMBER)
	{
		CachedPCCameraComponent->SetWorldLocationAndRotation(TargetCameraLocation, TargetCameraRotation);
		return;
	}

	// 平滑计算新的相机位置
	const FVector SmoothedCameraLocation = FMath::VInterpTo(CurrentCameraLocation, TargetCameraLocation, DeltaTime, PCCameraInterpolationSpeed);

	// 平滑计算新的相机旋转
	const FRotator SmoothedCameraRotation = FMath::RInterpTo(CachedPCCameraComponent->GetComponentRotation(), TargetCameraRotation, DeltaTime, PCCameraInterpolationSpeed);

	// 应用像素流观察相机的新位置和旋转
	CachedPCCameraComponent->SetWorldLocationAndRotation(SmoothedCameraLocation, SmoothedCameraRotation);
}

