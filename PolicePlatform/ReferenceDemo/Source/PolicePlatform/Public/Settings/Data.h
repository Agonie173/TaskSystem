// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundBase.h"// 提供音频资产类型
#include "Engine/StaticMesh.h"// 提供课程角色临时道具使用的静态网格体类型
#include "Data.generated.h"

class AHallPlayerController;

// 声明UMG界面类型，供任务定义保存Widget类
class UUserWidget;

// 声明音频资源类型，供任务定义保存旁白
class USoundBase;

class AActor;

//枚举定义地方
#pragma region 枚举定义

//进程类型枚举 
UENUM(BlueprintType)
enum class EProcessType : uint8
{
	None			UMETA(DisplayName = "未定义"),
	JLD				UMETA(DisplayName = "教练客户端"),
	PC_XYD			UMETA(DisplayName = "PC学员客户端"),
	VR_XYD			UMETA(DisplayName = "VR学员客户端"),
	ViewPort		UMETA(DisplayName = "像素流客户端"),
	CenterDS		UMETA(DisplayName = "中心DS"),
	RoomDS			UMETA(DisplayName = "房间DS"),
};


//任务结果类型枚举 
UENUM(BlueprintType, meta = (ScriptName = "ETaskResultEnum"))
enum class ETaskResult : uint8
{
	None			UMETA(DisplayName = "未定义"),
	Succeed			UMETA(DisplayName = "成功"),
	Fail			UMETA(DisplayName = "失败"),
	TimeOut			UMETA(DisplayName = "超时"),
	IsGetOutOfLine	UMETA(DisplayName = "违规"),
};


//任务类型枚举 
UENUM(BlueprintType)
enum class ETaskType : uint8
{
	None					UMETA(DisplayName = "未定义"),
	Interaction				UMETA(DisplayName = "交互任务"),
	
};

//任务所属阶段枚举 
UENUM(BlueprintType)
enum class ETaskPhase : uint8
{
	None						UMETA(DisplayName = "未定义"),
	SiteDisposal				UMETA(DisplayName = "现场处置阶段"),
	
};

//任务考核维度枚举 
UENUM(BlueprintType)
enum class ETaskAssess : uint8
{
	None						UMETA(DisplayName = "未定义"),
	ZFGF						UMETA(DisplayName = "执法规范"),
	GTBD						UMETA(DisplayName = "沟通表达"),
	CXYS						UMETA(DisplayName = "程序意识"),
};

//课件类型枚举 
UENUM(BlueprintType)
enum class EKJType : uint8
{
	None						UMETA(DisplayName = "未定义"),
	JQCZ						UMETA(DisplayName = "警情处置"),
	SJXL						UMETA(DisplayName = "射击训练"),
};

//手抓取物品类型
UENUM(BlueprintType)
enum class EHandHoldType : uint8
{
	None						UMETA(DisplayName = "未定义"),
	JingGun						UMETA(DisplayName = "警棍"),
	ShouQiang					UMETA(DisplayName = "手枪"),
	LaJiaoShui					UMETA(DisplayName = "辣椒水"),
	JingGuanZheng				UMETA(DisplayName = "警官证"),
	ShouJi						UMETA(DisplayName = "手机"),
};

//课程任务触发类型
UENUM(BlueprintType)
enum class ECourseTaskTriggerType : uint8
{
	// 没有定义触发方式
	None UMETA(DisplayName = "未定义"),

	// 等待场景或UI上报指定事件
	WaitEvent UMETA(DisplayName = "等待事件"),

	// 在限时内识别全部配置关键词
	SpeechKeywords UMETA(DisplayName = "语音关键词"),

	// 任务开始后立即检查服务器保存的课程装备状态
	EquipmentState UMETA(DisplayName = "检查装备状态"),

	// 按照配置的阶段顺序依次等待事件并执行本地Actor命令
	StageSequence UMETA(DisplayName = "顺序阶段")
};

// 定义课程本地Actor可以执行的通用命令类型
UENUM(BlueprintType)
enum class ECourseLocalActorCommandType : uint8
{
	// 没有配置命令
	None UMETA(DisplayName = "未定义"),

	// 销毁指定Actor并从任务组件的Actor记录中移除
	DestroyActor UMETA(DisplayName = "销毁Actor"),

	// 根据场景锚点设置Actor的位置、旋转或缩放
	SetTransformFromAnchor UMETA(DisplayName = "根据锚点设置变换"),

	// 将自定义命令发送给Actor蓝图处理
	Custom UMETA(DisplayName = "执行自定义命令")
};

// 定义PC端执行场景过渡时的移动方式
UENUM(BlueprintType)
enum class ECoursePCTransitionMode : uint8
{
	// 直接移动玩家Actor，适用于普通地点传送
	MovePlayer UMETA(DisplayName = "移动玩家"),

	// 只设置本地相机位置，适用于坐上警车等观察视角
	MoveCamera UMETA(DisplayName = "移动相机")
};

// 定义课程过渡需要移动的目标类型
UENUM(BlueprintType)
enum class ECourseTransitionTargetMode : uint8
{
	// 保持现有逻辑，直接把玩家或PC相机移动到目标锚点
	MovePlayerToAnchor UMETA(DisplayName = "移动玩家到锚点"),

	// 移动警车等场景Actor，并保持玩家或相机在Actor中的相对位置
	MoveSceneActorWithOccupant UMETA(DisplayName = "移动场景Actor并保持乘员位置")
};

// 定义一个课程功能在哪些训练模式中允许使用
UENUM(BlueprintType)
enum class ECourseModeAvailability : uint8
{
	BothModes UMETA(DisplayName = "训练和考核模式"),
	TrainingOnly UMETA(DisplayName = "仅训练模式"),
	AssessmentOnly UMETA(DisplayName = "仅考核模式")
};
#pragma endregion


//结构体定义地方
#pragma region 结构体定义

// 保存UI或场景交互上报给任务系统的课程事件数据
USTRUCT(BlueprintType)
struct FCourseEventData
{
	GENERATED_BODY()

	// 表示玩家完成了什么操作，例如出警人数确认
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课件事件")
	FName EventId = NAME_None;

	// 表示玩家操作了哪个对象，例如出警人数界面
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课件事件")
	FName TargetId = NAME_None;

	// 表示当前事件是否携带一个有效的整数，数值为0时也能正确区分
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课件事件")
	bool bHasIntegerValue = false;

	// 保存事件携带的整数，本任务中表示玩家选择的协同警员数量
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课件事件")
	int32 IntegerValue = 0;
};

//玩家的一些低频变化的数据
USTRUCT(BlueprintType)
struct FPlayerInfo
{
	GENERATED_BODY()

	//玩家编号，用于玩家唯一标识
	UPROPERTY(BlueprintReadOnly)
	FString SN = TEXT("");
	
	// 玩家名字
	UPROPERTY(BlueprintReadOnly)
	FString PlayerName = TEXT("");

	// 玩家控制器，服务器上才有效
	UPROPERTY(BlueprintReadOnly)
	AHallPlayerController* PC = nullptr;
	
	//课件，对应地图
	UPROPERTY(BlueprintReadOnly)
	int32 MapID = -2;
	
	//标识唯一的房间DS，因为同一个课件可能同时存在多个DS
	UPROPERTY(BlueprintReadOnly)
	int32 RoomID = -2;
};

//玩家一些高频变化的数据
USTRUCT(BlueprintType)
struct FPlayerStatus
{
	GENERATED_BODY()
	
	//玩家编号，用于玩家唯一标识
	UPROPERTY(BlueprintReadWrite)
	FString SN = TEXT("");
	
	// 玩家名字
	UPROPERTY(BlueprintReadOnly)
	FString PlayerName = TEXT("");
	
	//头显电量
	UPROPERTY(BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "100"))
	int32 HeadsetBattery = 101;
	
	//手枪电量
	UPROPERTY(BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "100"))
	int32 GunBattery = 101;
	
	//手柄电量
	/*
	5 = 满电
	4 = 偏高
	3 = 中等
	2 = 偏低
	1 = 极低（建议低电量弹窗提醒）
	 */
	UPROPERTY(BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "100"))
	int32 TouchBattery = 101;
	
	
};


//房间DS信息
USTRUCT(BlueprintType)
struct FRoomDSInfo
{
	GENERATED_BODY()
	
	
	UPROPERTY(BlueprintReadWrite)
	int32 MapID = -1;
	
	UPROPERTY(BlueprintReadWrite)
	int32 RoomID = -1;
	
	UPROPERTY(BlueprintReadWrite)
	int32 Port = -1;
	
	//单人训练还是受控训练
	UPROPERTY(BlueprintReadWrite)
	bool bControlledMode = true;
};



//课件信息
USTRUCT(BlueprintType)
struct FCoursewareInfo
{
    GENERATED_BODY()

    //课件ID
    UPROPERTY(BlueprintReadWrite)
    int32 MapId = 0;

    //课件名称
    UPROPERTY(BlueprintReadWrite)
    FString Name = TEXT("");

    //课件说明
    UPROPERTY(BlueprintReadWrite)
    FString Description = TEXT("");
	
	//图片引用路径
	UPROPERTY(BlueprintReadWrite)
	FString Image = TEXT("");
	
	//课件类型
	UPROPERTY(BlueprintReadWrite)
	EKJType Type = EKJType::None;
};

//任务数据结构
USTRUCT(BlueprintType)
struct FTaskData
{
    GENERATED_BODY()

	//任务名称
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "任务数据")
    FString TaskName = TEXT("任务xx");

	//任务类型
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "任务数据")
    ETaskType TaskType = ETaskType::None;

	//任务所属阶段
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "任务数据")
	ETaskPhase TaskPhase = ETaskPhase::None;
	
	//任务考核维度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "任务数据")
	ETaskAssess TaskAssess = ETaskAssess::None;
	
	//任务限时时间
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "任务数据")
    float TimeLimit = 20.0;

	//任务标准话术
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "任务数据")
    FString StandardScript = TEXT("");

	//任务关键词
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "任务数据")
    TArray<FString> Keywords;

	//满分得分
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "任务数据")
    float SucceedScore = 10.0;

	//失败得分
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "任务数据")
    float FailScore = 0.0;

	//超时得分
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "任务数据")
    float TimeoutScore = 0.0;
	
	//训练模式UI提示文本
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "任务数据")
	FString XunLianUIText = TEXT("");
	
	//考核模式UI提示文本
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "任务数据")
	FString KaoHeUIText = TEXT("");
};


//成绩 任务执行结果
USTRUCT(BlueprintType)
struct FTaskResult
{
	GENERATED_BODY()
	
	//学员的SN
	UPROPERTY(BlueprintReadOnly)
	FString SN = TEXT("");
	
	//任务序号
	UPROPERTY(BlueprintReadOnly)
	int32 TaskIndex = 0;
	
	//哪个课件
	UPROPERTY(BlueprintReadOnly)
	int32 MapID = 0;
	
	//哪个房间
	UPROPERTY(BlueprintReadOnly)
	int32 RoomID = 0;
	
	//任务名称
	UPROPERTY(BlueprintReadOnly)
	FString TaskName = TEXT("");
	
	//任务执行结果
	UPROPERTY(BlueprintReadOnly)
	ETaskResult TaskResult = ETaskResult::None;
	
	//任务一些记录文本
	UPROPERTY(BlueprintReadOnly)
	FString RecordText = TEXT("");
	
	//语音识别到的文本
	UPROPERTY(BlueprintReadOnly)
	FString YYSpeak = TEXT("");
	
	//任务得分
	UPROPERTY(BlueprintReadOnly)
	int32 Score = 0;
	
	//任务关键词
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Keywords;
};

// 定义任务结束后玩家需要执行的场景过渡配置
USTRUCT(BlueprintType)
struct FCoursePlayerTransitionDefinition
{
	GENERATED_BODY()

	// 是否在当前任务结束后执行玩家场景过渡
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "玩家场景过渡")
	bool bEnabled = false;

	// 设置当前过渡是直接移动玩家还是移动载具和乘员
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "玩家场景过渡", meta = (EditCondition = "bEnabled", EditConditionHides))
	ECourseTransitionTargetMode TransitionTargetMode = ECourseTransitionTargetMode::MovePlayerToAnchor;

	// 保存需要移动的场景Actor标签，例如出警警车
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "玩家场景过渡", meta = (EditCondition = "bEnabled && TransitionTargetMode == ECourseTransitionTargetMode::MoveSceneActorWithOccupant", EditConditionHides))
	FName MovingSceneActorId = NAME_None;

	// 设置PC端执行过渡时移动玩家还是只移动相机
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "玩家场景过渡", meta = (EditCondition = "bEnabled", EditConditionHides))
	ECoursePCTransitionMode PCTransitionMode = ECoursePCTransitionMode::MovePlayer;

	// 是否让VR头显高度对齐目标锚点，开启后会减去相机相对玩家的Z轴高度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "玩家场景过渡", meta = (EditCondition = "bEnabled", EditConditionHides))
	bool bCompensateVRCameraHeight = false;

	// 普通模式表示玩家目标位置，载具模式表示场景Actor的目标位置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "玩家场景过渡", meta = (EditCondition = "bEnabled", EditConditionHides))
	FName TargetAnchorId = NAME_None;

	// 设置画面从正常状态变成全黑所需的时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "玩家场景过渡", meta = (EditCondition = "bEnabled", EditConditionHides, ClampMin = "0.0"))
	float FadeOutDuration = 0.25f;

	// 设置从任务结束到服务器正式传送玩家的等待时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "玩家场景过渡", meta = (EditCondition = "bEnabled", EditConditionHides, ClampMin = "0.0"))
	float TeleportDelay = 1.0f;

	// 设置玩家完成传送后继续保持全黑的时间，用于等待相机位置和网络同步稳定
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "玩家场景过渡", meta = (EditCondition = "bEnabled", EditConditionHides, ClampMin = "0.0"))
	float BlackHoldDuration = 0.1f;

	// 设置玩家传送完成后画面恢复所需的时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "玩家场景过渡", meta = (EditCondition = "bEnabled", EditConditionHides, ClampMin = "0.0"))
	float FadeInDuration = 0.25f;

	// 是否将玩家旋转设置为目标锚点的旋转
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "玩家场景过渡", meta = (EditCondition = "bEnabled", EditConditionHides))
	bool bApplyTargetRotation = true;
};

// 配置任务结束后需要在指定客户端本地生成的场景Actor
USTRUCT(BlueprintType)
struct FCourseLocalActorSpawnDefinition
{
	GENERATED_BODY()

	// 当前本地Actor的唯一ID，用于避免重复生成和后续查找
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor")
	FName ActorId = NAME_None;

	// 需要生成的Actor蓝图类
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor")
	TSubclassOf<AActor> ActorClass;

	// 场景中作为生成位置的Actor Tag
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor")
	FName SpawnAnchorId = NAME_None;

	// 当前Actor是否在创建它的任务结束时自动销毁
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor")
	bool bDestroyWhenTaskEnds = false;
};

// 定义任务开始或结束时需要对已有本地Actor执行的通用命令
USTRUCT(BlueprintType)
struct FCourseLocalActorCommandDefinition
{
	GENERATED_BODY()

	// 指定需要执行命令的本地Actor ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor命令")
	FName ActorId = NAME_None;

	// 设置当前需要执行的通用命令类型
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor命令")
	ECourseLocalActorCommandType CommandType = ECourseLocalActorCommandType::None;

	// 设置Actor变换时使用的场景锚点Tag
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor命令", meta = (EditCondition = "CommandType == ECourseLocalActorCommandType::SetTransformFromAnchor", EditConditionHides))
	FName TargetAnchorId = NAME_None;

	// 是否将Actor位置设置为锚点位置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor命令", meta = (EditCondition = "CommandType == ECourseLocalActorCommandType::SetTransformFromAnchor", EditConditionHides))
	bool bApplyLocation = true;

	// 是否将Actor旋转设置为锚点旋转
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor命令", meta = (EditCondition = "CommandType == ECourseLocalActorCommandType::SetTransformFromAnchor", EditConditionHides))
	bool bApplyRotation = true;

	// 是否将Actor缩放设置为锚点缩放
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor命令", meta = (EditCondition = "CommandType == ECourseLocalActorCommandType::SetTransformFromAnchor", EditConditionHides))
	bool bApplyScale = false;

	// 在锚点世界变换基础上追加的相对变换
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor命令", meta = (EditCondition = "CommandType == ECourseLocalActorCommandType::SetTransformFromAnchor", EditConditionHides))
	FTransform RelativeTransform = FTransform::Identity;

	// 自定义命令的唯一ID，例如关闭碰撞、开始逃跑或切换状态
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor命令", meta = (EditCondition = "CommandType == ECourseLocalActorCommandType::Custom", EditConditionHides))
	FName CustomCommandId = NAME_None;
};

// 定义课程本地角色动作期间使用的临时道具
USTRUCT(BlueprintType)
struct FCourseLocalActorAttachedPropDefinition
{
	GENERATED_BODY()

	// 动作期间需要显示的静态网格体，留空表示不使用临时道具
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor临时道具")
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	// 临时道具需要吸附的角色骨骼或Socket名称
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor临时道具")
	FName SocketName = NAME_None;

	// 临时道具吸附后的相对变换
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor临时道具")
	FTransform RelativeTransform = FTransform::Identity;

	// 临时道具显示后的持续时间，等于0时在动画结束时隐藏
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor临时道具", meta = (ClampMin = "0.0"))
	float VisibleDuration = 0.0f;
};

// 定义课程本地Actor动作附带的路线移动配置
USTRUCT(BlueprintType)
struct FCourseLocalActorPathMoveDefinition
{
	GENERATED_BODY()

	// 是否在播放当前动作时同时启动路线移动
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor路线移动", meta = (DisplayName = "启用路线移动"))
	bool bEnabled = false;

	// 指定场景中CourseNPCPathActor配置的路线ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor路线移动", meta = (DisplayName = "路线ID", EditCondition = "bEnabled", EditConditionHides))
	FName RouteId = NAME_None;

	// Actor沿路线移动的速度，单位为厘米每秒
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor路线移动", meta = (DisplayName = "移动速度", EditCondition = "bEnabled", EditConditionHides, ClampMin = "1.0", UIMin = "1.0"))
	float MoveSpeed = 120.0f;

	// 是否从Spline终点反向移动到起点
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor路线移动", meta = (DisplayName = "反向移动", EditCondition = "bEnabled", EditConditionHides))
	bool bMoveReverse = false;

	// 移动过程中是否让Actor朝向当前路线方向
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor路线移动", meta = (DisplayName = "朝向移动方向", EditCondition = "bEnabled", EditConditionHides))
	bool bFaceMovementDirection = true;

	// 到达路线终点后需要从NPC动作表中播放的待机动作ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor路线移动", meta = (DisplayName = "到达后待机动作ID", EditCondition = "bEnabled", EditConditionHides))
	FName ArrivalIdleActionId = NAME_None;
};

// 定义一个课程本地Actor需要执行的动作
USTRUCT(BlueprintType)
struct FCourseLocalActorActionDefinition
{
	GENERATED_BODY()

	// 指定执行动作的本地Actor ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor动作")
	FName ActorId = NAME_None;

	// 指定角色动作表中的动作ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor动作")
	FName ActionId = NAME_None;

	// 当前配置是否需要播放角色动画，关闭后只执行ActionSound
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor动作")
	bool bPlayAnimation = true;

	// 当前动作相对于任务开始的延迟时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor动作", meta = (ClampMin = "0.0"))
	float StartDelay = 0.0f;

	// 当前动作期间需要使用的临时道具
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor动作")
	FCourseLocalActorAttachedPropDefinition AttachedProp;

	// 动作开始时立即播放的声音
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor动作")
	TObjectPtr<USoundBase> ActionSound = nullptr;

	// 动作声音附着的骨骼或Socket名称
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor动作")
	FName ActionSoundSocketName = FName(TEXT("head"));

	// 非循环动画结束时是否主动停止动作声音
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor动作")
	bool bStopActionSoundWhenAnimationEnds = false;

	// 动画是否循环播放
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor动作")
	bool bLooping = false;

	// 动画结束后是否恢复待机动画
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor动作")
	bool bReturnToIdle = true;

	// 动画结束后是否把根骨骼位移固化到Actor位置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor动作")
	bool bCommitRootLocationAtEnd = false;

	// 动作播放结束后需要播放的音频，留空表示不播放
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor动作")
	TObjectPtr<USoundBase> CompletionSound = nullptr;

	// 音频附着的骨骼或Socket名称
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor动作")
	FName CompletionSoundSocketName = FName(TEXT("head"));

	// 当前动作播放时需要同时执行的路线移动配置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "课程本地Actor动作", meta = (DisplayName = "路线移动配置"))
	FCourseLocalActorPathMoveDefinition PathMove;
};

// 定义数据驱动任务中的一个顺序阶段
USTRUCT(BlueprintType)
struct FCourseTaskStageDefinition
{
	GENERATED_BODY()

	// 当前阶段的唯一标识，主要用于编辑器识别和日志输出
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "任务阶段", meta = (DisplayName = "阶段ID"))
	FName StageId = NAME_None;

	// 当前阶段需要等待的课程事件ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "任务阶段", meta = (DisplayName = "阶段完成事件ID"))
	FName EventId = NAME_None;

	// 当前阶段需要操作的目标ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "任务阶段", meta = (DisplayName = "阶段目标ID"))
	FName TargetId = NAME_None;

	// 进入当前阶段时需要对本地Actor执行的命令
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "任务阶段", meta = (DisplayName = "进入阶段时Actor命令", TitleProperty = "ActorId"))
	TArray<FCourseLocalActorCommandDefinition> EnterActorCommands;

	// 进入当前阶段时是否播放黑闪
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "任务阶段 | 黑闪", meta = (DisplayName = "进入阶段时黑闪"))
	bool bUseBlackFade = false;

	// 画面变成全黑所需时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "任务阶段 | 黑闪", meta = (DisplayName = "渐黑时间", EditCondition = "bUseBlackFade", EditConditionHides, ClampMin = "0.01"))
	float FadeOutDuration = 0.15f;

	// 画面全黑后的保持时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "任务阶段 | 黑闪", meta = (DisplayName = "全黑保持时间", EditCondition = "bUseBlackFade", EditConditionHides, ClampMin = "0.0"))
	float BlackHoldDuration = 0.05f;

	// 画面恢复正常所需时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "任务阶段 | 黑闪", meta = (DisplayName = "渐亮时间", EditCondition = "bUseBlackFade", EditConditionHides, ClampMin = "0.01"))
	float FadeInDuration = 0.15f;
};

// 数据驱动课程任务定义
USTRUCT(BlueprintType)
struct FCourseTaskDefinition
{
	GENERATED_BODY()

	// 保存任务名称、限时、提示文本、关键词和得分等任务基础数据
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "01 任务基础", meta = (DisplayName = "任务数据"))
	FTaskData TaskData;

	// 编辑器测试时从第一个勾选该选项的任务开始执行
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "01 任务基础", meta = (DisplayName = "从此任务开始测试"))
	bool bTestStart = false;

	// 是否记录任务成绩并向中心DS上报
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "01 任务基础", meta = (DisplayName = "关键考核任务"))
	bool bIsKeyTask = false;

	// 是否只在训练模式执行，开启后考核模式会跳过当前任务
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "01 任务基础", meta = (DisplayName = "仅训练模式执行"))
	bool bOnlyXunLianModeValid = false;

	// 设置当前任务使用的完成触发方式
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "02 完成条件", meta = (DisplayName = "任务触发类型"))
	ECourseTaskTriggerType TriggerType = ECourseTaskTriggerType::None;

	// 顺序阶段任务需要依次执行的全部阶段
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "02 完成条件 | 顺序阶段", meta = (DisplayName = "任务阶段", TitleProperty = "StageId", EditCondition = "TriggerType == ECourseTaskTriggerType::StageSequence", EditConditionHides))
	TArray<FCourseTaskStageDefinition> TaskStages;

	// WaitEvent任务需要等待的课程事件ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "02 完成条件 | 等待事件", meta = (DisplayName = "完成事件ID", EditCondition = "TriggerType == ECourseTaskTriggerType::WaitEvent", EditConditionHides))
	FName EventId = NAME_None;

	// WaitEvent任务需要操作的正确目标ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "02 完成条件 | 等待事件", meta = (DisplayName = "正确目标ID", EditCondition = "TriggerType == ECourseTaskTriggerType::WaitEvent", EditConditionHides))
	FName TargetId = NAME_None;

	// 是否根据课程事件携带的整数判断任务成功或失败
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "02 完成条件 | 等待事件", meta = (DisplayName = "检查整数结果", EditCondition = "TriggerType == ECourseTaskTriggerType::WaitEvent", EditConditionHides))
	bool bEvaluateIntegerValue = false;

	// 保存整数选择任务期望的正确整数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "02 完成条件 | 等待事件", meta = (DisplayName = "期望整数值", EditCondition = "TriggerType == ECourseTaskTriggerType::WaitEvent && bEvaluateIntegerValue", EditConditionHides))
	int32 ExpectedIntegerValue = 0;

	// 装备状态检查任务需要查询的通用装备ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "02 完成条件 | 装备状态", meta = (DisplayName = "检查装备ID", EditCondition = "TriggerType == ECourseTaskTriggerType::EquipmentState", EditConditionHides))
	FName EquipmentStateId = NAME_None;

	// 装备状态检查任务期望该装备是否处于启用状态
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "02 完成条件 | 装备状态", meta = (DisplayName = "期望装备已启用", EditCondition = "TriggerType == ECourseTaskTriggerType::EquipmentState", EditConditionHides))
	bool bExpectedEquipmentActive = true;

	// PC端允许完成当前任务的通用快捷输入ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "02 完成条件 | 快捷输入", meta = (DisplayName = "PC快捷输入ID"))
	FName PCInputActionId = NAME_None;

	// VR端允许完成当前任务的通用快捷输入ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "02 完成条件 | 快捷输入", meta = (DisplayName = "VR快捷输入ID"))
	FName VRInputActionId = NAME_None;

	// 任务开始时立即发送给课件任务蓝图的通用动作ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "03 开始阶段", meta = (DisplayName = "开始动作ID"))
	FName StartActionId = NAME_None;

	// 任务开始时需要生成的本地场景Actor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "03 开始阶段 | 本地Actor", meta = (DisplayName = "开始时生成Actor", TitleProperty = "ActorId"))
	TArray<FCourseLocalActorSpawnDefinition> StartLocalActorSpawns;

	// 任务开始时需要对已有本地Actor执行的通用命令
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "03 开始阶段 | 本地Actor", meta = (DisplayName = "开始时Actor命令", TitleProperty = "ActorId"))
	TArray<FCourseLocalActorCommandDefinition> StartLocalActorCommands;

	// 任务开始时需要执行的本地Actor动画和声音动作
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "03 开始阶段 | 本地Actor", meta = (DisplayName = "开始时Actor动作", TitleProperty = "ActionId"))
	TArray<FCourseLocalActorActionDefinition> StartLocalActorActions;

	// 开始动作执行后等待多少秒再显示UI、播放旁白并开放交互
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "03 开始阶段", meta = (DisplayName = "任务内容开放延迟", ClampMin = "0.0", UIMin = "0.0"))
	float TaskContentStartDelay = 0.0f;

	// 是否需要玩家操作当前任务UI，纯提示文本UI应关闭
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "04 任务内容 | 交互权限", meta = (DisplayName = "任务UI允许交互"))
	bool bTaskWidgetInteractive = true;

	// 是否允许学员在当前任务中进行课程装备选择
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "04 任务内容 | 交互权限", meta = (DisplayName = "允许课程装备选择"))
	bool bAllowEquipmentSelectionUI = false;

	// 指定课程装备选择功能在哪些训练模式中开放
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "04 任务内容 | 交互权限", meta = (DisplayName = "装备选择适用模式", EditCondition = "bAllowEquipmentSelectionUI", EditConditionHides))
	ECourseModeAvailability EquipmentSelectionAvailability = ECourseModeAvailability::BothModes;

	// 保存任务开始时需要显示的UI类，未配置时不创建任务UI
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "04 任务内容 | UI显示", meta = (DisplayName = "任务UI类"))
	TSubclassOf<UUserWidget> TaskWidgetClass;

	// 保存任务UI默认显示的通用提示文本
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "04 任务内容 | UI显示", meta = (DisplayName = "通用提示文本", MultiLine = true))
	FText PromptText;

	// 保存PC学员端专用提示文本，留空时使用通用提示文本
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "04 任务内容 | UI显示", meta = (DisplayName = "PC提示文本覆盖", MultiLine = true))
	FText PCPromptTextOverride;

	// 保存VR学员端专用提示文本，留空时使用通用提示文本
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "04 任务内容 | UI显示", meta = (DisplayName = "VR提示文本覆盖", MultiLine = true))
	FText VRPromptTextOverride;

	// 保存VR和像素流世界空间UI使用的场景锚点ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "04 任务内容 | UI显示", meta = (DisplayName = "三维UI锚点ID"))
	FName WorldWidgetAnchorId = NAME_None;

	// 设置世界空间任务UI的实际宽度，单位为厘米
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "04 任务内容 | UI显示", meta = (DisplayName = "三维UI宽度", ClampMin = "10.0", UIMin = "50.0", UIMax = "600.0"))
	float WorldWidgetWidth = 500.0f;

	// 保存任务内容正式开始时需要播放的旁白
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "04 任务内容 | 音频", meta = (DisplayName = "任务开始旁白"))
	USoundBase* NarrationSound = nullptr;

	// 任务提交成功后等待多少秒再执行真正的任务结束处理
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "05 结束阶段 | 延迟结束", meta = (DisplayName = "结束处理延迟", ClampMin = "0.0", UIMin = "0.0"))
	float CompletionDelay = 0.0f;

	// 任务进入延迟结束阶段时立即播放的声音
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "05 结束阶段 | 延迟结束", meta = (DisplayName = "结束等待声音"))
	TObjectPtr<USoundBase> CompletionDelaySound = nullptr;

	// 任务真正结束时对已有本地Actor执行的通用命令
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "05 结束阶段 | 本地Actor", meta = (DisplayName = "结束时Actor命令", TitleProperty = "ActorId"))
	TArray<FCourseLocalActorCommandDefinition> CompletionLocalActorCommands;

	// 任务真正结束时需要生成的本地场景Actor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "05 结束阶段 | 本地Actor", meta = (DisplayName = "结束时生成Actor", TitleProperty = "ActorId"))
	TArray<FCourseLocalActorSpawnDefinition> CompletionLocalActorSpawns;

	// 任务结束后需要执行的黑屏、玩家移动或场景Actor移动配置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "05 结束阶段 | 场景过渡", meta = (DisplayName = "结束场景过渡"))
	FCoursePlayerTransitionDefinition CompletionTransition;

	// 任务结束时需要播放的回复声音
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "05 结束阶段 | 音频与动作", meta = (DisplayName = "任务结束声音"))
	USoundBase* CompletionSound = nullptr;

	// 任务结束声音播放完成后发送给课件任务蓝图的动作ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "05 结束阶段 | 音频与动作", meta = (DisplayName = "结束动作ID"))
	FName CompletionActionId = NAME_None;

	// 当前任务结束后到下一项任务正式开始之间的等待时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "06 下一任务", meta = (DisplayName = "下一任务等待时间", ClampMin = "0.6", UIMin = "0.6"))
	float NextTaskInterval = 0.6f;
};
#pragma endregion
