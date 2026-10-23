#include "Component/AC_CourseTransition.h"
#include "GamePlay/PPPlayerBase.h"// 提供项目玩家基类
#include "GameFramework/PlayerController.h"// 提供玩家控制器和相机管理器
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"// 提供临时相机Actor
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"// 提供角色移动组件
#include "Kismet/GameplayStatics.h"// 提供根据Tag查找Actor的功能
#include "Engine/World.h"// 提供世界和计时器功能
#include "TimerManager.h"
#include "Engine/GameInstance.h"// 提供GameInstance访问
#include "Tools/InfoSubsystem.h"// 提供当前运行进程类型判断

// 设置组件默认属性
UAC_CourseTransition::UAC_CourseTransition()
{
	// 当前组件不需要每帧执行Tick
	PrimaryComponentTick.bCanEverTick = false;
}

// 组件开始运行时缓存所属玩家
void UAC_CourseTransition::BeginPlay()
{
	// 执行父类开始逻辑
	Super::BeginPlay();

	// 将组件所属Actor转换成项目玩家基类
	OwnerPlayer = Cast<APPPlayerBase>(GetOwner());

	// 检查组件是否正确挂载在项目玩家身上
	if (!IsValid(OwnerPlayer))
	{
		// 输出错误日志
		UE_LOG(LogTemp, Error, TEXT("课程场景过渡组件初始化失败：Owner不是APPPlayerBase"));
	}
}

// 组件结束运行时清理资源
void UAC_CourseTransition::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 判断当前世界是否有效
	if (IsValid(GetWorld()))
	{
		// 清除尚未执行的场景过渡计时器
		GetWorld()->GetTimerManager().ClearTimer(TransitionTimerHandle);
	}

	// 判断是否存在PC临时相机
	if (IsValid(ActivePCCameraActor))
	{
		// 销毁PC临时相机
		ActivePCCameraActor->Destroy();

		// 清空PC临时相机引用
		ActivePCCameraActor = nullptr;
	}

	// 执行父类结束逻辑
	Super::EndPlay(EndPlayReason);
}

// 根据任务配置开始一次课程场景过渡
void UAC_CourseTransition::StartCourseTransition(const FCoursePlayerTransitionDefinition& TransitionDefinition)
{
	// 没有开启过渡时不执行
	if (!TransitionDefinition.bEnabled)
	{
		return;
	}

	// OwnerPlayer无效时重新获取一次
	if (!IsValid(OwnerPlayer))
	{
		// 将组件所属Actor转换成项目玩家基类
		OwnerPlayer = Cast<APPPlayerBase>(GetOwner());
	}

	// 判断所属玩家是否有效
	if (!IsValid(OwnerPlayer))
	{
		// 输出错误日志
		UE_LOG(LogTemp, Error, TEXT("课程场景过渡开始失败：OwnerPlayer无效"));

		// 结束当前函数
		return;
	}

	// 判断当前运行进程是否为像素流客户端
	const bool bIsViewPortClient = IsViewPortClient();

	// 只允许服务器、本地学员和像素流客户端执行场景过渡
	if (!OwnerPlayer->HasAuthority() && !OwnerPlayer->IsLocallyControlled() && !bIsViewPortClient)
	{
		return;
	}

	// 像素流观察端和其他非学员端不执行黑屏与移动
	if (OwnerPlayer->ProcessType != EProcessType::PC_XYD && OwnerPlayer->ProcessType != EProcessType::VR_XYD)
	{
		return;
	}

	// 判断目标锚点ID是否有效
	if (TransitionDefinition.TargetAnchorId.IsNone())
	{
		// 输出错误日志
		UE_LOG(LogTemp, Error, TEXT("课程场景过渡开始失败：TargetAnchorId为空"));
		// 结束当前函数
		return;
	}

	// 根据中文Tag查找目标锚点
	if (!FindTargetAnchor(TransitionDefinition.TargetAnchorId))
	{
		// 找不到锚点时结束当前函数
		return;
	}

	// 保存当前过渡配置
	ActiveTransition = TransitionDefinition;

	// 清理上一轮场景Actor过渡数据
	MovingSceneActor = nullptr;

	// 重置场景Actor原始变换
	OriginalMovingSceneActorTransform = FTransform::Identity;

	// 重置乘员相对变换
	PreservedOccupantRelativeTransform = FTransform::Identity;

	// 重置乘员相对变换有效状态
	bHasPreservedOccupantRelativeTransform = false;

	// 判断当前是否需要移动警车等场景Actor
	if (ActiveTransition.TransitionTargetMode == ECourseTransitionTargetMode::MoveSceneActorWithOccupant)
	{
		// 检查场景Actor标签是否有效
		if (ActiveTransition.MovingSceneActorId.IsNone())
		{
			UE_LOG(LogTemp, Error, TEXT("课程场景Actor过渡失败：MovingSceneActorId为空"));
			return;
		}

		// 根据配置查找需要移动的场景Actor
		if (!FindMovingSceneActor(ActiveTransition.MovingSceneActorId))
		{
			return;
		}

		// 保存玩家或PC相机在警车中的相对位置
		if (!CaptureOccupantRelativeTransform())
		{
			return;
		}
	}

	// 判断世界是否有效
	if (!IsValid(GetWorld()))
	{
		// 输出错误日志
		UE_LOG(LogTemp, Error, TEXT("课程场景过渡开始失败：World无效"));

		// 结束当前函数
		return;
	}

	// 清除可能残留的上一次过渡计时器
	GetWorld()->GetTimerManager().ClearTimer(TransitionTimerHandle);

	// 只在所属学员自己的本地端开始黑屏
	StartLocalFadeOut();

	// 保证玩家完全黑屏之后再执行移动
	const float ActualTransitionDelay = FMath::Max(ActiveTransition.TeleportDelay, ActiveTransition.FadeOutDuration);

	// 判断是否需要延时
	if (ActualTransitionDelay <= 0.0f)
	{
		// 不需要延时时立即执行移动
		ExecuteTransitionAtBlackScreen();
	}
	else
	{
		// 等待画面全黑后执行移动
		GetWorld()->GetTimerManager().SetTimer(TransitionTimerHandle, this, &UAC_CourseTransition::ExecuteTransitionAtBlackScreen, ActualTransitionDelay, false);
	}

	// 输出过渡开始日志
	//UE_LOG(LogTemp, Warning, TEXT("课程场景过渡开始：锚点=%s，玩家类型=%s，等待时间=%.2f"), *ActiveTransition.TargetAnchorId.ToString(), *UEnum::GetValueAsString(OwnerPlayer->ProcessType), ActualTransitionDelay);
}

bool UAC_CourseTransition::FindMovingSceneActor(FName MovingSceneActorId)
{
	// 清空上一轮查找结果
	MovingSceneActor = nullptr;

	// World或所属学员无效时不能查找
	if (!IsValid(GetWorld()) || !IsValid(OwnerPlayer))
	{
		return false;
	}

	// 保存拥有目标Tag的全部场景Actor
	TArray<AActor*> FoundSceneActors;

	// 根据配置Tag查找全部候选场景Actor
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), MovingSceneActorId, FoundSceneActors);

	// 没有找到对应Actor时返回失败
	if (FoundSceneActors.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("课程场景Actor过渡失败：没有找到场景Actor，Tag=%s，学员SN=%s"), *MovingSceneActorId.ToString(), *OwnerPlayer->GPlayerInfo.SN);
		return false;
	}

	// 单学员场景只有一个候选Actor时直接使用
	if (FoundSceneActors.Num() == 1)
	{
		MovingSceneActor = FoundSceneActors[0];
		return IsValid(MovingSceneActor);
	}

	// 多学员场景按照Actor Owner匹配当前任务所属学员
	for (AActor* FoundSceneActor : FoundSceneActors)
	{
		// 跳过无效Actor
		if (!IsValid(FoundSceneActor))
		{
			continue;
		}

		// 只选择归属于当前学员的场景Actor
		if (FoundSceneActor->GetOwner() == OwnerPlayer)
		{
			MovingSceneActor = FoundSceneActor;
			UE_LOG(LogTemp, Warning, TEXT("课程场景Actor按学员匹配成功：学员SN=%s，Actor=%s，Tag=%s"), *OwnerPlayer->GPlayerInfo.SN, *MovingSceneActor->GetName(), *MovingSceneActorId.ToString());
			return true;
		}
	}

	// 输出每个候选Actor当前的Owner
	for (AActor* FoundSceneActor : FoundSceneActors)
	{
		if (IsValid(FoundSceneActor))
		{
			UE_LOG(LogTemp, Error, TEXT("课程场景Actor候选归属不匹配：学员SN=%s，Actor=%s，ActorOwner=%s"), *OwnerPlayer->GPlayerInfo.SN, *FoundSceneActor->GetName(), *GetNameSafe(FoundSceneActor->GetOwner()));
		}
	}

	// 找不到所属警车时禁止移动，避免错误移动其他学员的警车
	UE_LOG(LogTemp, Error, TEXT("课程场景Actor过渡失败：没有找到当前学员所属场景Actor，Tag=%s，学员SN=%s，候选数量=%d"), *MovingSceneActorId.ToString(), *OwnerPlayer->GPlayerInfo.SN, FoundSceneActors.Num());
	return false;
}

bool UAC_CourseTransition::CaptureOccupantRelativeTransform()
{
	// 检查所属玩家和场景Actor
	if (!IsValid(OwnerPlayer) || !IsValid(MovingSceneActor))
	{
		return false;
	}

	// 保存警车移动前的世界变换
	OriginalMovingSceneActorTransform = MovingSceneActor->GetActorTransform();

	// VR端始终保存玩家Actor相对警车的位置
	if (OwnerPlayer->ProcessType == EProcessType::VR_XYD)
	{
		// 计算VR玩家相对警车的变换
		PreservedOccupantRelativeTransform = OwnerPlayer->GetActorTransform().GetRelativeTransform(OriginalMovingSceneActorTransform);

		// 标记乘员相对变换有效
		bHasPreservedOccupantRelativeTransform = true;

		// 输出保存成功日志
		UE_LOG(LogTemp, Warning, TEXT("已保存VR玩家相对警车的位置：玩家=%s，警车=%s"), *OwnerPlayer->GetName(), *MovingSceneActor->GetName());
		return true;
	}

	// PC普通玩家移动模式保存玩家Actor位置
	if (OwnerPlayer->ProcessType == EProcessType::PC_XYD && ActiveTransition.PCTransitionMode == ECoursePCTransitionMode::MovePlayer)
	{
		// 计算PC玩家相对警车的变换
		PreservedOccupantRelativeTransform = OwnerPlayer->GetActorTransform().GetRelativeTransform(OriginalMovingSceneActorTransform);

		// 标记乘员相对变换有效
		bHasPreservedOccupantRelativeTransform = true;
		return true;
	}

	// PC相机模式在服务器上只需要移动警车
	if (OwnerPlayer->ProcessType == EProcessType::PC_XYD && !OwnerPlayer->IsLocallyControlled())
	{
		return true;
	}

	// PC相机模式必须存在上车时创建的临时相机
	if (OwnerPlayer->ProcessType == EProcessType::PC_XYD && !IsValid(ActivePCCameraActor))
	{
		UE_LOG(LogTemp, Error, TEXT("保存PC相机相对警车位置失败：ActivePCCameraActor无效"));
		return false;
	}

	// 保存PC临时相机相对警车的位置
	if (OwnerPlayer->ProcessType == EProcessType::PC_XYD)
	{
		// 计算PC临时相机相对警车的变换
		PreservedOccupantRelativeTransform = ActivePCCameraActor->GetActorTransform().GetRelativeTransform(OriginalMovingSceneActorTransform);

		// 标记乘员相对变换有效
		bHasPreservedOccupantRelativeTransform = true;

		// 输出保存成功日志
		//UE_LOG(LogTemp, Warning, TEXT("已保存PC相机相对警车的位置：警车=%s"), *MovingSceneActor->GetName());
		return true;
	}

	// 当前运行端不属于支持的学员端
	return false;
}

void UAC_CourseTransition::ApplySceneActorWithOccupantTransition()
{
	// 检查本次过渡需要的对象
	if (!IsValid(OwnerPlayer) || !IsValid(MovingSceneActor) || !IsValid(TargetAnchorActor))
	{
		UE_LOG(LogTemp, Error, TEXT("课程载具整体移动失败：玩家、场景Actor或目标锚点无效"));
		return;
	}

	// 判断当前运行进程是否为像素流客户端
	const bool bIsViewPortClient = IsViewPortClient();

	// 使用现场锚点作为警车的目标变换
	FTransform TargetSceneActorTransform = TargetAnchorActor->GetActorTransform();

	// 保持警车原来的缩放
	TargetSceneActorTransform.SetScale3D(OriginalMovingSceneActorTransform.GetScale3D());

	// 不应用目标旋转时保留警车原来的旋转
	if (!ActiveTransition.bApplyTargetRotation)
	{
		TargetSceneActorTransform.SetRotation(OriginalMovingSceneActorTransform.GetRotation());
	}

	// 计算警车本次移动产生的Yaw变化
	const float SceneActorYawDelta = FRotator::NormalizeAxis(TargetSceneActorTransform.Rotator().Yaw - OriginalMovingSceneActorTransform.Rotator().Yaw);

	// 根据原相对位置计算乘员在现场的目标世界变换
	const FTransform TargetOccupantTransform = PreservedOccupantRelativeTransform * TargetSceneActorTransform;

	// 服务器、本地学员端和像素流端分别移动各自世界中的本地警车
	if (OwnerPlayer->HasAuthority() || OwnerPlayer->IsLocallyControlled() || bIsViewPortClient)
	{
		MovingSceneActor->SetActorTransform(TargetSceneActorTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}

	// 像素流端只移动该学员所属的本地警车
	// 远端学员Pawn的位置继续由服务器复制，不能在像素流端直接修改
	if (bIsViewPortClient && !OwnerPlayer->IsLocallyControlled())
	{
		UE_LOG(LogTemp, Warning, TEXT("像素流课程载具本地移动完成：学员SN=%s，场景Actor=%s，目标锚点=%s"), *OwnerPlayer->GPlayerInfo.SN, *MovingSceneActor->GetName(), *ActiveTransition.TargetAnchorId.ToString());
		return;
	}

	// 判断当前是否需要直接移动玩家Actor
	const bool bMovePlayerActor = OwnerPlayer->ProcessType == EProcessType::VR_XYD || ActiveTransition.PCTransitionMode == ECoursePCTransitionMode::MovePlayer;

	// VR或PC玩家移动模式恢复玩家相对警车的位置
	if (bMovePlayerActor && bHasPreservedOccupantRelativeTransform)
	{
		// 清除角色移动组件残留速度
		if (UCharacterMovementComponent* CharacterMovement = OwnerPlayer->GetCharacterMovement())
		{
			CharacterMovement->StopMovementImmediately();
		}

		// 取得玩家控制器
		AController* PlayerController = OwnerPlayer->GetController();

		// 将控制器朝向增加与警车相同的Yaw变化
		if (IsValid(PlayerController))
		{
			// 获取原控制旋转
			FRotator TargetControlRotation = PlayerController->GetControlRotation();

			// 保持玩家相对警车的观察朝向
			TargetControlRotation.Yaw = FRotator::NormalizeAxis(TargetControlRotation.Yaw + SceneActorYawDelta);

			// 保存新的控制旋转
			PlayerController->SetControlRotation(TargetControlRotation);
		}

		// 将玩家恢复到警车中的原相对位置
		OwnerPlayer->SetActorTransform(TargetOccupantTransform, false, nullptr, ETeleportType::TeleportPhysics);

		// 服务器强制同步玩家新位置
		if (OwnerPlayer->HasAuthority())
		{
			OwnerPlayer->ForceNetUpdate();
		}
	}
	// PC相机模式恢复临时相机相对警车的位置
	else if (OwnerPlayer->ProcessType == EProcessType::PC_XYD && OwnerPlayer->IsLocallyControlled())
	{
		// 检查PC临时相机
		if (!IsValid(ActivePCCameraActor) || !bHasPreservedOccupantRelativeTransform)
		{
			UE_LOG(LogTemp, Error, TEXT("课程载具整体移动失败：PC临时相机或相对变换无效"));
			return;
		}

		// 将PC临时相机恢复到警车中的原相对位置
		ActivePCCameraActor->SetActorTransform(TargetOccupantTransform);
	}

	// 输出整体移动结果
	UE_LOG(LogTemp, Warning, TEXT("课程载具整体移动完成：场景Actor=%s，目标锚点=%s，Yaw变化=%.2f"), *MovingSceneActor->GetName(), *ActiveTransition.TargetAnchorId.ToString(), SceneActorYawDelta);
}

// 根据中文Tag查找目标锚点
bool UAC_CourseTransition::FindTargetAnchor(FName TargetAnchorId)
{
	// 清空上一次找到的锚点
	TargetAnchorActor = nullptr;

	// 判断世界是否有效
	if (!IsValid(GetWorld()))
	{
		// 返回查找失败
		return false;
	}

	// 创建数组接收拥有指定Tag的Actor
	TArray<AActor*> FoundAnchorActors;

	// 根据中文Tag查找场景Actor
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), TargetAnchorId, FoundAnchorActors);

	// 判断是否找到目标锚点
	if (FoundAnchorActors.IsEmpty())
	{
		// 输出错误日志
		UE_LOG(LogTemp, Error, TEXT("课程场景过渡失败：没有找到锚点，Tag=%s"), *TargetAnchorId.ToString());

		// 返回查找失败
		return false;
	}

	// 判断是否存在多个同名锚点
	if (FoundAnchorActors.Num() > 1)
	{
		// 输出警告并使用第一个锚点
		UE_LOG(LogTemp, Warning, TEXT("课程场景过渡找到多个同名锚点：Tag=%s，数量=%d，将使用第一个"), *TargetAnchorId.ToString(), FoundAnchorActors.Num());
	}

	// 保存找到的第一个锚点
	TargetAnchorActor = FoundAnchorActors[0];

	// 返回锚点是否有效
	return IsValid(TargetAnchorActor);
}

// 画面变黑后执行实际移动
void UAC_CourseTransition::ExecuteTransitionAtBlackScreen()
{
	// 判断所属玩家和目标锚点是否有效
	if (!IsValid(OwnerPlayer) || !IsValid(TargetAnchorActor))
	{
		// 输出错误日志
		UE_LOG(LogTemp, Error, TEXT("课程场景过渡执行失败：玩家或锚点无效"));

		// 防止画面一直保持黑屏
		StartLocalFadeIn();

		// 结束当前函数
		return;
	}

	// 判断当前是否需要移动场景Actor和乘员
	if (ActiveTransition.TransitionTargetMode == ECourseTransitionTargetMode::MoveSceneActorWithOccupant)
	{
		// 移动警车并保持玩家或相机在车内的位置
		ApplySceneActorWithOccupantTransition();
	}
	// VR普通过渡继续直接移动玩家
	else if (OwnerPlayer->ProcessType == EProcessType::VR_XYD)
	{
		ApplyPlayerTransition();
	}
	// PC普通过渡根据配置移动玩家或相机
	else if (OwnerPlayer->ProcessType == EProcessType::PC_XYD)
	{
		// PC上车时只移动临时相机
		if (ActiveTransition.PCTransitionMode == ECoursePCTransitionMode::MoveCamera)
		{
			ApplyPCCameraTransition();
		}
		else
		{
			// 恢复玩家自身相机
			RestorePCPlayerView();

			// 直接移动PC玩家
			ApplyPlayerTransition();
		}
	}

	// 取得传送完成后需要继续保持全黑的时间
	const float SafeBlackHoldDuration = FMath::Max(ActiveTransition.BlackHoldDuration, 0.0f);

	// 不需要保持全黑时立即恢复画面
	if (SafeBlackHoldDuration <= KINDA_SMALL_NUMBER)
	{
		StartLocalFadeIn();
	}
	// World有效时等待配置时间，让玩家位置、VR相机和网络同步先稳定
	else if (UWorld* CurrentWorld = GetWorld())
	{
		CurrentWorld->GetTimerManager().SetTimer(TransitionTimerHandle, this, &UAC_CourseTransition::StartLocalFadeIn, SafeBlackHoldDuration, false);
	}
	// World无效时直接恢复画面，避免一直黑屏
	else
	{
		StartLocalFadeIn();
	}
}

// 将VR玩家或普通PC玩家移动到目标锚点
void UAC_CourseTransition::ApplyPlayerTransition()
{
	// 判断玩家和锚点是否有效
	if (!IsValid(OwnerPlayer) || !IsValid(TargetAnchorActor))
	{
		return;
	}

	// 服务器和所属玩家本地端执行移动
	if (!OwnerPlayer->HasAuthority() && !OwnerPlayer->IsLocallyControlled())
	{
		return;
	}

	// 默认直接使用锚点位置
	FVector TargetPlayerLocation = TargetAnchorActor->GetActorLocation();

	// 默认保持玩家当前旋转
	FRotator TargetPlayerRotation = OwnerPlayer->GetActorRotation();

	// 保存当前VR相机的世界变换
	FTransform CurrentVRCameraTransform = FTransform::Identity;

	// 标记是否取得了有效的VR相机变换
	bool bFoundVRCameraTransform = false;

	// 判断当前是否为VR玩家
	if (OwnerPlayer->ProcessType == EProcessType::VR_XYD)
	{
		// 所属VR玩家本地端直接读取真实相机组件
		if (OwnerPlayer->IsLocallyControlled())
		{
			// 获取玩家身上的全部相机组件
			TArray<UCameraComponent*> CameraComponents;

			// 查找玩家身上的相机组件
			OwnerPlayer->GetComponents<UCameraComponent>(CameraComponents);

			// 保存当前活动相机组件
			UCameraComponent* ActiveCameraComponent = nullptr;

			// 遍历所有相机组件
			for (UCameraComponent* CameraComponent : CameraComponents)
			{
				// 判断当前相机组件是否有效并且正在工作
				if (IsValid(CameraComponent) && CameraComponent->IsActive())
				{
					// 保存当前活动相机
					ActiveCameraComponent = CameraComponent;

					// 找到后停止遍历
					break;
				}
			}

			// 没有找到活动相机时使用第一个有效相机
			if (!IsValid(ActiveCameraComponent) && CameraComponents.Num() > 0)
			{
				// 保存第一个相机组件
				ActiveCameraComponent = CameraComponents[0];
			}

			// 判断是否取得有效相机
			if (IsValid(ActiveCameraComponent))
			{
				// 保存当前VR相机的世界变换
				CurrentVRCameraTransform = ActiveCameraComponent->GetComponentTransform();

				// 标记相机变换获取成功
				bFoundVRCameraTransform = true;
			}
		}
		else if (OwnerPlayer->HasAuthority())
		{
			// 判断DS是否已经收到VR客户端上报的相机变换
			if (!OwnerPlayer->CameraT.Equals(FTransform::Identity))
			{
				// 使用VR客户端上报给服务器的相机世界变换
				CurrentVRCameraTransform = OwnerPlayer->CameraT;

				// 标记相机变换获取成功
				bFoundVRCameraTransform = true;
			}
		}
	}

	// 判断VR端是否需要补偿头显相对玩家的高度
	if (OwnerPlayer->ProcessType == EProcessType::VR_XYD && ActiveTransition.bCompensateVRCameraHeight)
	{
		// 判断是否取得了有效的VR相机变换
		if (bFoundVRCameraTransform)
		{
			// 计算头显相对玩家Actor的Z轴高度
			const float CameraHeightOffset = CurrentVRCameraTransform.GetLocation().Z - OwnerPlayer->GetActorLocation().Z;

			// 从锚点高度中减去头显相对玩家的高度
			TargetPlayerLocation.Z = TargetPlayerLocation.Z - CameraHeightOffset;

			// 输出VR高度补偿日志
			UE_LOG(LogTemp, Warning, TEXT("VR传送高度补偿：锚点Z=%.2f，相机相对高度=%.2f，玩家目标Z=%.2f"), TargetAnchorActor->GetActorLocation().Z, CameraHeightOffset, TargetPlayerLocation.Z);
		}
		else
		{
			// 输出相机变换获取失败日志
			UE_LOG(LogTemp, Warning, TEXT("VR传送高度补偿失败：没有取得有效的VR相机变换"));
		}
	}

	// 判断是否需要应用目标锚点旋转
	if (ActiveTransition.bApplyTargetRotation)
	{
		// 判断当前是否为VR玩家并且已经取得相机变换
		if (OwnerPlayer->ProcessType == EProcessType::VR_XYD && bFoundVRCameraTransform)
		{
			// 获取玩家当前Actor的Yaw
			const float CurrentPlayerYaw = OwnerPlayer->GetActorRotation().Yaw;

			// 获取VR相机当前的世界Yaw
			const float CurrentCameraYaw = CurrentVRCameraTransform.Rotator().Yaw;

			// 计算头显相对玩家Actor的Yaw偏移
			const float CameraRelativeYaw = FRotator::NormalizeAxis(CurrentCameraYaw - CurrentPlayerYaw);

			// 使用锚点Yaw减去头显相对Yaw，计算玩家Actor真正需要的Yaw
			TargetPlayerRotation.Yaw = FRotator::NormalizeAxis(TargetAnchorActor->GetActorRotation().Yaw - CameraRelativeYaw);

			// VR玩家Actor不应用锚点的俯仰旋转
			TargetPlayerRotation.Pitch = 0.0f;

			// VR玩家Actor不应用锚点的翻滚旋转
			TargetPlayerRotation.Roll = 0.0f;

			// 输出VR朝向补偿日志
			UE_LOG(LogTemp, Warning, TEXT("VR传送朝向补偿：锚点Yaw=%.2f，相机相对Yaw=%.2f，玩家目标Yaw=%.2f"), TargetAnchorActor->GetActorRotation().Yaw, CameraRelativeYaw, TargetPlayerRotation.Yaw);
		}
		else
		{
			// PC普通传送直接使用锚点旋转
			TargetPlayerRotation = TargetAnchorActor->GetActorRotation();
		}
	}

	// 获取玩家角色移动组件
	UCharacterMovementComponent* CharacterMovement = OwnerPlayer->GetCharacterMovement();

	// 判断角色移动组件是否有效
	if (IsValid(CharacterMovement))
	{
		// 清除传送前残留的移动速度
		CharacterMovement->StopMovementImmediately();
	}

	// 判断VR端是否需要应用目标旋转
	if (OwnerPlayer->ProcessType == EProcessType::VR_XYD && ActiveTransition.bApplyTargetRotation)
	{
		// 获取当前玩家控制器
		AController* PlayerController = OwnerPlayer->GetController();

		// 判断玩家控制器是否有效
		if (IsValid(PlayerController))
		{
			// 获取当前控制旋转
			FRotator TargetControlRotation = PlayerController->GetControlRotation();

			// 将控制器Yaw同步为玩家Actor的目标Yaw
			TargetControlRotation.Yaw = TargetPlayerRotation.Yaw;

			// 设置新的控制旋转，防止下一帧把玩家朝向改回去
			PlayerController->SetControlRotation(TargetControlRotation);
		}
	}

	// 将玩家移动到补偿后的目标位置和旋转
	OwnerPlayer->SetActorLocationAndRotation(TargetPlayerLocation, TargetPlayerRotation, false, nullptr, ETeleportType::TeleportPhysics);

	// 判断当前是否为服务器
	if (OwnerPlayer->HasAuthority())
	{
		// 强制尽快将新位置和旋转同步给其他客户端
		OwnerPlayer->ForceNetUpdate();
	}
}

// 将PC玩家画面切换到目标相机位置
void UAC_CourseTransition::ApplyPCCameraTransition()
{
	// 只有PC学员自己的本地端需要创建相机
	if (!IsValid(OwnerPlayer) || OwnerPlayer->ProcessType != EProcessType::PC_XYD || !OwnerPlayer->IsLocallyControlled())
	{
		return;
	}

	// 获取PC本地玩家控制器
	APlayerController* PlayerController = GetLocalPlayerController();

	// 判断玩家控制器是否有效
	if (!IsValid(PlayerController))
	{
		// 输出错误日志
		UE_LOG(LogTemp, Error, TEXT("PC课程相机移动失败：本地PlayerController无效"));

		// 结束当前函数
		return;
	}

	// 获取目标相机位置
	const FVector TargetLocation = TargetAnchorActor->GetActorLocation();

	// 默认使用锚点旋转
	FRotator TargetRotation = TargetAnchorActor->GetActorRotation();

	// 判断是否不应用锚点旋转
	if (!ActiveTransition.bApplyTargetRotation && IsValid(PlayerController->PlayerCameraManager))
	{
		// 保持PC当前相机旋转
		TargetRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
	}

	// 判断是否已经创建过临时相机
	if (!IsValid(ActivePCCameraActor))
	{
		// 设置临时相机生成参数
		FActorSpawnParameters SpawnParameters;

		// 设置临时相机所属玩家
		SpawnParameters.Owner = OwnerPlayer;

		// 忽略生成位置的碰撞
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// 在目标位置生成一个只存在于本地的临时相机
		ActivePCCameraActor = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), TargetLocation, TargetRotation, SpawnParameters);
	}
	else
	{
		// 将已有的临时相机移动到新的目标位置
		ActivePCCameraActor->SetActorLocationAndRotation(TargetLocation, TargetRotation);
	}

	// 判断临时相机是否生成成功
	if (!IsValid(ActivePCCameraActor))
	{
		// 输出错误日志
		UE_LOG(LogTemp, Error, TEXT("PC课程相机移动失败：无法创建临时相机"));

		// 结束当前函数
		return;
	}

	//// 判断玩家相机管理器是否有效
	//if (IsValid(PlayerController->PlayerCameraManager) && IsValid(ActivePCCameraActor->GetCameraComponent()))
	//{
	//	// 保持玩家原本的相机视野角度
	//	ActivePCCameraActor->GetCameraComponent()->SetFieldOfView(PlayerController->PlayerCameraManager->GetFOVAngle());
	//}

	// 获取PC临时相机组件
	UCameraComponent* TemporaryCameraComponent = ActivePCCameraActor->GetCameraComponent();

	// 判断临时相机组件是否有效
	if (IsValid(TemporaryCameraComponent))
	{
		// 关闭固定宽高比，让相机画面自动填满当前游戏视口
		TemporaryCameraComponent->SetConstraintAspectRatio(false);

		// 玩家相机管理器有效时保持玩家原本的视野角度
		if (IsValid(PlayerController->PlayerCameraManager))
		{
			TemporaryCameraComponent->SetFieldOfView(PlayerController->PlayerCameraManager->GetFOVAngle());
		}
	}

	// 将PC画面切换到警车位置的临时相机
	PlayerController->SetViewTargetWithBlend(ActivePCCameraActor, 0.0f);
}

// 恢复PC玩家原本的玩家相机
void UAC_CourseTransition::RestorePCPlayerView()
{
	// 获取PC本地玩家控制器
	APlayerController* PlayerController = GetLocalPlayerController();

	// 判断本地玩家控制器是否有效
	if (IsValid(PlayerController) && IsValid(OwnerPlayer))
	{
		// 将画面切回玩家自身相机
		PlayerController->SetViewTargetWithBlend(OwnerPlayer, 0.0f);
	}

	// 判断是否存在临时相机
	if (IsValid(ActivePCCameraActor))
	{
		// 销毁上警车时创建的临时相机
		ActivePCCameraActor->Destroy();

		// 清空临时相机引用
		ActivePCCameraActor = nullptr;
	}
}

// 开始所属学员本地端的黑屏
void UAC_CourseTransition::StartLocalFadeOut()
{
	// 获取所属学员的本地玩家控制器
	APlayerController* PlayerController = GetLocalPlayerController();

	// 判断玩家控制器和相机管理器是否有效
	if (!IsValid(PlayerController) || !IsValid(PlayerController->PlayerCameraManager))
	{
		return;
	}

	// 保证渐黑时间不会为零
	const float FadeDuration = FMath::Max(ActiveTransition.FadeOutDuration, 0.01f);

	// 将当前画面逐渐变成全黑并保持
	PlayerController->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, FadeDuration, FLinearColor::Black, false, true);
}

// 恢复所属学员本地端画面
void UAC_CourseTransition::StartLocalFadeIn()
{
	// 获取所属学员的本地玩家控制器
	APlayerController* PlayerController = GetLocalPlayerController();

	// 判断玩家控制器和相机管理器是否有效
	if (!IsValid(PlayerController) || !IsValid(PlayerController->PlayerCameraManager))
	{
		return;
	}

	// 保证渐亮时间不会为零
	const float FadeDuration = FMath::Max(ActiveTransition.FadeInDuration, 0.01f);

	// 将当前画面从全黑逐渐恢复
	PlayerController->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, FadeDuration, FLinearColor::Black, false, false);
}

// 获取当前玩家对应的本地玩家控制器
APlayerController* UAC_CourseTransition::GetLocalPlayerController() const
{
	// 判断所属玩家是否有效
	if (!IsValid(OwnerPlayer))
	{
		// 返回空控制器
		return nullptr;
	}

	// 只有所属玩家自己的本地端才允许操作画面
	if (!OwnerPlayer->IsLocallyControlled())
	{
		// DS和像素流观察端不会取得该控制器
		return nullptr;
	}

	// 返回当前玩家控制器
	return Cast<APlayerController>(OwnerPlayer->GetController());
}

// 判断当前运行进程是否为像素流客户端
bool UAC_CourseTransition::IsViewPortClient() const
{
	// 获取当前World
	UWorld* CurrentWorld = GetWorld();

	// World无效时不能判断进程类型
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

	// 只有像素流客户端返回true
	return IsValid(InfoSubsystem) && InfoSubsystem->ProcessType == EProcessType::ViewPort;
}
