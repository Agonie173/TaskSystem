#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Settings/Data.h"
#include "Components/AudioComponent.h"// 提供像素流声音组件的音量控制
#include "InfoSubsystem.generated.h"


/**
 * 信息子系统
 */

class AActor;
class USceneComponent;
class USoundBase;

 // 像素流当前监控学员发生变化时广播，后续用于关闭旧学员声音并开启新学员声音
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMonitoredStudentChanged, const FString&, PreviousStudentSN, const FString&, CurrentStudentSN);

UCLASS()
class POLICEPLATFORM_API UInfoSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

private:
	
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	
	//读取一些配置信息
	UFUNCTION()
	void GetConfig();
	
	//DS ip地址,所有客户端都需要这个信息
	UPROPERTY(BlueprintReadOnly, Category = "警务|信息子系统")
	FString DSIP = TEXT("127.0.0.1");
	
	//Pak文件放置的目录
	UPROPERTY(BlueprintReadOnly)
	FString PakDirectory = FPaths::ProjectSavedDir();

	//课件信息数组
	UPROPERTY(BlueprintReadOnly, Category = "警务|信息子系统")
	TArray<FCoursewareInfo> CoursewareList;
	
	//表示当前进程是哪个类型
	UPROPERTY(BlueprintReadOnly, Category = "警务|信息子系统")
	EProcessType ProcessType = EProcessType::None;
	
	UFUNCTION(BlueprintCallable, Category = "警务|信息子系统")
	void SetProcessType(EProcessType LProcessType);

	// 保存像素流客户端当前正在主画面中监控的学员SN，空字符串表示当前没有监控学员
	UPROPERTY(Transient, BlueprintReadOnly, Category = "警务|信息子系统|像素流监控")
	FString CurrentMonitoredStudentSN;

	// 当前监控学员发生变化时广播，已经播放的NPC、警笛和旁白可以通过该事件刷新静音状态
	UPROPERTY(BlueprintAssignable, Category = "警务|信息子系统|像素流监控")
	FOnMonitoredStudentChanged OnMonitoredStudentChanged;

	// 设置像素流客户端当前监控的学员SN，返回值表示本次SN是否真正发生变化
	UFUNCTION(BlueprintCallable, Category = "警务|信息子系统|像素流监控", meta = (DisplayName = "设置当前监控学员SN"))
	bool SetCurrentMonitoredStudentSN(const FString& LStudentSN);

	// 判断指定学员是否为像素流客户端当前监控的学员
	UFUNCTION(BlueprintPure, Category = "警务|信息子系统|像素流监控", meta = (DisplayName = "是否为当前监控学员"))
	bool IsCurrentMonitoredStudent(const FString& LStudentSN) const;

	// 获取指定学员声音在像素流客户端上应使用的音量
	UFUNCTION(BlueprintPure, Category = "警务|信息子系统|像素流监控", meta = (DisplayName = "获取监控学员声音音量"))
	float GetMonitoredStudentAudioVolume(const FString& LStudentSN) const;

	// 注册一条需要根据当前监控学员切换音量的声音
	UFUNCTION(BlueprintCallable, Category = "警务|信息子系统|像素流监控", meta = (DisplayName = "注册监控学员声音"))
	void RegisterMonitoredStudentAudio(UAudioComponent* LAudioComponent, const FString& LStudentSN);

	// 注销已经停止或销毁的学员声音
	UFUNCTION(BlueprintCallable, Category = "警务|信息子系统|像素流监控", meta = (DisplayName = "注销监控学员声音"))
	void UnregisterMonitoredStudentAudio(UAudioComponent* LAudioComponent);

	// 根据所属学员生成附加声音，并自动接入像素流声音路由
	UFUNCTION(BlueprintCallable, Category = "警务|信息子系统|像素流监控", meta = (DisplayName = "生成监控学员附加声音"))
	UAudioComponent* SpawnMonitoredStudentSoundAttached(USoundBase* LSound, USceneComponent* LAttachToComponent, FName LAttachPointName, AActor* LStudentActor);
	
#pragma region 教练端

public:
	

	//TODO后期的玩家编号用其它方法获取
	//已经有多少玩家连接进来过
	int32 TempNum = 0;

#pragma endregion


#pragma region 中心DS

public:
	//动态拉起一个DS，就加1，用于标识唯一的房间DS
	UPROPERTY()
	int32 CurrentRoomID = 0;

	//可用的房间DS释放的端口列表
	UPROPERTY(BlueprintReadOnly, Category = "警务|信息子系统")
	TArray<int32> AvailablePorts;

	//当前房间DS的端口
	UPROPERTY()
	int32 TempRoomDSPort = 7777;


	//获取一个可用的端口来启动房间DS
	UFUNCTION()
	int32 GetNewRoomDSPort();

	//房间DS关闭，回收利用端口
	UFUNCTION()
	void RecyclingPort(int32 LPort);

	//读取指定目录下的pak文件，根据pak名得到课件ID，通过课件ID从json文件里获取到课件的信息
	//课件名必须是KJ24Pxxxxx.pak这种命名
	UFUNCTION()
	void GetExistingKJ();
	
	//根据MapID获取对应的课件名称
	UFUNCTION(BlueprintPure, Category = "警务|信息子系统|中心DS")
	FString GetKJNameFromMapID(int32 LMapID);
	
	
#pragma endregion


#pragma region 房间DS

public:
	
	UPROPERTY(BlueprintReadWrite, Category = "警务|信息子系统|房间DS")
	FRoomDSInfo RoomDSInfo;

#pragma endregion


#pragma region 玩家端
	
	
	//玩家信息
	UPROPERTY(BlueprintReadOnly, Category = "警务|信息子系统|玩家端")
	FPlayerInfo PlayerInfo;

	//玩家状态信息
	UPROPERTY(BlueprintReadWrite, Category = "警务|信息子系统|玩家端")
	FPlayerStatus PlayerStatus;

	
#pragma endregion

	private:

		// 刷新像素流客户端中所有已注册学员声音的音量
		void RefreshMonitoredStudentAudioVolumes();

		// 保存声音组件及其所属学员SN，弱引用不会阻止声音组件销毁
		TMap<TWeakObjectPtr<UAudioComponent>, FString> MonitoredStudentAudioComponents;

};
