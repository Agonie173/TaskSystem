#pragma once

#include "CoreMinimal.h"
#include "Settings/Data.h"
#include "GameFramework/Character.h"
#include "Component/AC_Task.h"
#include "PPPlayerBase.generated.h"

class UAC_VRPlayer;
class UAC_PCPlayer;

// 提前声明课程场景过渡组件
class UAC_CourseTransition;

// 提前声明相机组件
class UCameraComponent;

// 提前声明动画蒙太奇
class UAnimMontage;

// 提前声明Motion Warping组件
class UMotionWarpingComponent;

/**
 * 玩家基础类,继承自ACharacter,根据PC/像素流客户端/还是VR设备,执行不同的逻辑
 */
UCLASS()
class POLICEPLATFORM_API APPPlayerBase : public ACharacter
{
	GENERATED_BODY()
	
public:
	APPPlayerBase();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	
	UFUNCTION()
	void OnRep_ProcessType();

	//大厅学员执行安全入口，蓝图实现
	UFUNCTION(BlueprintImplementableEvent, Category = "警务|玩家")
	void Start_BIE(EProcessType LProcessType);
	
	
	UPROPERTY(Replicated, BlueprintReadWrite, Category = "警务|玩家")
	FPlayerInfo GPlayerInfo;
	
	// PC和 VR学员端分别动态挂载不同的组件
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "警务 | 动态组件配置")
	TSubclassOf<UAC_PCPlayer> AC_PCClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "警务 | 动态组件配置")
	TSubclassOf<UAC_VRPlayer> AC_VRClass = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "警务 | 动态组件配置")
	TSubclassOf<UAC_Task> AC_TaskClass = nullptr;
	
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "警务 | 动态组件")
	UAC_PCPlayer* AC_PCPlayer = nullptr;
	
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "警务 | 动态组件")
	UAC_VRPlayer* AC_VRPlayer = nullptr;
	
	
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "警务 | 动态组件")
	UAC_Task* AC_Task = nullptr;
	
	//客户端类型
	UPROPERTY(ReplicatedUsing=OnRep_ProcessType, BlueprintReadOnly, Category = "警务|玩家")
	EProcessType ProcessType = EProcessType::None;
	
	//相机位置
	UPROPERTY(Replicated, BlueprintReadWrite, Category = "警务|玩家")
	FTransform CameraT;
	
	//训练模式还是考核模式测试
	UPROPERTY(Replicated, BlueprintReadOnly, Category="警务|玩家")
	bool bIsXunLianMode = true; 
	
	//挂载任务组件
	UFUNCTION()
	void AttachAC_Task();
	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "警务|玩家")
	UTextureRenderTarget2D* Min2DTR = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "警务|玩家")
	UTextureRenderTarget2D* Current2DTR = nullptr;

	
	// 负责处理任务结束后的黑屏、玩家移动和PC相机移动
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "警务 | 动态组件")
	UAC_CourseTransition* AC_CourseTransition = nullptr;

	//VR玩家根位置
	UPROPERTY(Replicated, BlueprintReadWrite, Category = "警务|玩家")
	FTransform VRRootT;
	
	//VR玩家右手位置
	UPROPERTY(Replicated, BlueprintReadWrite, Category = "警务|玩家")
	FTransform RHandT;
	
	//VR玩家左手位置
	UPROPERTY(Replicated, BlueprintReadWrite, Category = "警务|玩家")
	FTransform LHandT;
	
	//左手是否是握住状态
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "警务|玩家")
	bool bLeftHandIsGrab = false;
	
	//右手是否是握住状态 
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "警务|玩家")
	bool bRightHandIsGrab = false;
	
	UPROPERTY(ReplicatedUsing=OnRep_RightHandHoldType, BlueprintReadOnly, Category = "警务|玩家")
	EHandHoldType RightHandHoldType = EHandHoldType::None;
	
	// 是否显示VR射线，由服务器同步给像素流客户端
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "警务|玩家")
	bool bShowVRLineTrace = false;

	// 本地学员修改射线显示状态
	UFUNCTION(BlueprintCallable, Category = "警务|玩家", meta = (DisplayName = "设置VR射线显示状态"))
	void SetVRLineTraceVisible(bool bEnabled);

	// 将射线状态上传服务器
	UFUNCTION(Server, Reliable)
	void RPC_ServerSetVRLineTraceVisible(bool bEnabled);
	
	UFUNCTION(Server, Unreliable, BlueprintCallable, Category = "警务|玩家")
	void RPC_ServerVRSetLocationData(FTransform LCameraT, FTransform LVRRootT, FTransform LRHand, FTransform LLHand);

	// 将PC学员当前真正用于显示画面的相机变换发送给服务器
	UFUNCTION(Server, Unreliable, BlueprintCallable, Category = "警务|玩家")
	void RPC_ServerPCSetCameraTransform(FTransform LPCameraTransform);

	// 当前PC玩家是否正在播放原地转身动画
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "警务 | PC转身", meta = (DisplayName = "PC转身中"))
	bool bPCTurning = false;

	// 当前锁定的转身动画角度，只会使用-180、-90、90、180
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "警务 | PC转身", meta = (DisplayName = "PC转身动画角度"))
	float PCTurnAnimationAngle = 0.0f;

	//本地设置手的姿势状态
	UFUNCTION(BlueprintCallable, Category = "警务|玩家")
	void LocalSetHandState(bool LIsRightHand, bool LIsGrab);
	
	//服务器上同步手的姿势状态
	UFUNCTION(Server, Unreliable)
	void RPC_ServerSetHandState(bool LIsRightHand, bool LIsGrab);
	
	//本地设置右手的抓取物品类型
	UFUNCTION(BlueprintCallable, Category = "警务|玩家")
	void LocalSetRightHoldType(EHandHoldType LType);
	
	//服务器上同步手的姿势状态
	UFUNCTION(Server, Reliable)
	void RPC_ServerSetRightHandHoldType(EHandHoldType LType);
	
	UFUNCTION()
	void OnRep_RightHandHoldType();
	
	//在蓝图里实现具体手抓取物品的表现
	UFUNCTION(BlueprintImplementableEvent)
	bool BIE_RightHandHoldType(EHandHoldType LType);
	
	// 通知玩家蓝图开启或关闭课件三维UI交互，具体射线逻辑由不同玩家蓝图实现
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "课件任务UI", meta = (DisplayName = "设置课件任务UI交互"))
	void SetCourseTaskUIInteractionEnabled_BIE(bool bEnabled);
	
	// 将指定Actor添加到当前学员的像素流场景捕获白名单
	UFUNCTION(BlueprintCallable, Category = "警务 | 像素流")
	bool RegisterActorToXSLShowOnly(AActor* ActorToShow);

	// 将指定Actor从当前学员的像素流场景捕获白名单移除
	UFUNCTION(BlueprintCallable, Category = "警务 | 像素流")
	void UnregisterActorFromXSLShowOnly(AActor* ActorToRemove);

protected:
	// 控制器与Actor朝向超过该角度后允许触发PC原地转身
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "警务 | PC转身", meta = (ClampMin = "1.0", ClampMax = "179.0"))
	float PCTurnTriggerAngle = 45.0f;

	// 偏差达到该角度时选择180度转身动画
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "警务 | PC转身", meta = (ClampMin = "90.0", ClampMax = "180.0"))
	float PCTurn180Threshold = 135.0f;

	// 选择90度动画前等待视角输入稳定的时间
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "警务 | PC转身", meta = (ClampMin = "0.0"))
	float PCTurnDecisionDelay = 0.12f;

	// 速度超过该数值时认为PC玩家正在移动，不触发原地转身
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "警务 | PC转身", meta = (ClampMin = "0.0"))
	float PCTurnMovementSpeedThreshold = 5.0f;

	// PC左转90度蒙太奇
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "警务 | PC转身")
	TObjectPtr<UAnimMontage> PCTurnLeft90Montage = nullptr;

	// PC右转90度蒙太奇
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "警务 | PC转身")
	TObjectPtr<UAnimMontage> PCTurnRight90Montage = nullptr;

	// PC左转180度蒙太奇
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "警务 | PC转身")
	TObjectPtr<UAnimMontage> PCTurnLeft180Montage = nullptr;

	// PC右转180度蒙太奇
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "警务 | PC转身")
	TObjectPtr<UAnimMontage> PCTurnRight180Montage = nullptr;

	// PC学员向服务器上传当前实际相机的时间间隔，0.05秒表示每秒20次
	UPROPERTY(EditDefaultsOnly, Category = "警务 | 像素流", meta = (ClampMin = "0.01"))
	float PCCameraUploadInterval = 0.05f;

	// 像素流端平滑追踪PC相机的插值速度
	UPROPERTY(EditDefaultsOnly, Category = "警务 | 像素流", meta = (ClampMin = "0.0"))
	float PCCameraInterpolationSpeed = 15.0f;

	// 相机位置相差超过该距离时直接瞬移，避免场景切换时相机缓慢飞行
	UPROPERTY(EditDefaultsOnly, Category = "警务 | 像素流", meta = (ClampMin = "0.0"))
	float PCCameraSnapDistance = 300.0f;

private:
	// 每帧更新PC原地转身检测、动画状态和结束修正
	void UpdatePCTurn(float DeltaTime);

	// 判断当前Pawn是否为允许驱动PC转身的本地学员
	bool ShouldRunLocalPCTurn() const;

	// 判断PC玩家当前是否正在移动
	bool IsPCTurnMovementActive() const;

	// 本地开始一次PC原地转身
	void StartLocalPCTurn(float LRawYawDifference);

	// 判断当前实例是否存在活动的PC转身
	bool IsPCTurnActive() const;

	// 根据固定动画角度取得对应的转身Montage
	UAnimMontage* SelectPCTurnMontage(float LTurnAnimationAngle) const;

	// 设置Warp目标并播放转身Montage
	bool PlayPCTurnMontage(UAnimMontage* LMontage, float LTargetYaw);

	// 接收转身Montage播放结束回调
	void HandlePCTurnMontageEnded(UAnimMontage* LMontage, bool bInterrupted);

	// 服务器校验并开始权威转身Montage
	void ApplyServerPCTurnStart(float LTurnAnimationAngle, float LTargetYaw);

	// 客户端请求服务器开始权威转身Montage
	UFUNCTION(Server, Reliable)
	void RPC_ServerBeginPCTurn(float LTurnAnimationAngle, float LTargetYaw);

	// 服务器通知所有远端客户端播放PC转身表现Montage
	UFUNCTION(NetMulticast, Reliable)
	void RPC_MulticastPlayPCTurnMontage(float LTurnAnimationAngle, float LTargetYaw);

	// 处理PC相机上传和像素流端相机追踪
	void UpdatePCCameraSynchronization(float DeltaTime);

	// PC学员本地端上传当前实际相机变换
	void UploadLocalPCCameraTransform(float DeltaTime);

	// 像素流端将观察相机更新到服务器同步的位置
	void ApplyReplicatedPCCameraTransform(float DeltaTime);

	// 缓存BP_Player中的PCCamera2组件
	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> CachedPCCameraComponent = nullptr;

	// 标记像素流观察相机是否已经使用绝对世界位置和旋转
	bool bPCCameraUsesAbsoluteTransform = false;

	// 累计PC相机上传时间，避免每帧发送RPC
	float PCCameraUploadElapsedTime = 0.0f;

	// 90度转身触发前累计的视角稳定等待时间
	float PCTurnDecisionElapsedTime = 0.0f;

	// 当前等待判定的转身方向，1为右转，-1为左转
	float PCTurnDecisionDirection = 0.0f;

	// 动画结束后需要对齐的准确控制器朝向
	float PCTurnExactTargetYaw = 0.0f;

	// 负责修改转身Montage Root Motion的组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "警务 | PC转身", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMotionWarpingComponent> PCTurnMotionWarpingComponent = nullptr;

	// 当前正在播放的转身Montage
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActivePCTurnMontage = nullptr;

	// 开始转身前CharacterMovement使用的控制器朝向状态
	bool bSavedUseControllerDesiredRotation = false;

	// Motion Warping中配置的目标名称
	static const FName PCTurnWarpTargetName;
};