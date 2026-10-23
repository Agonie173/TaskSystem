#include "Tools/InfoSubsystem.h"
#include "VaRestSubsystem.h"
#include "GamePlay/PPGameInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "GamePlay/PPPlayerBase.h"// 提供学员信息和SN
#include "Components/SceneComponent.h"// 提供声音附加组件
#include "Sound/SoundBase.h"// 提供声音虚拟化模式


void UInfoSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    // 清除当前监控学员，避免GameInstance销毁时保留无效状态
    CurrentMonitoredStudentSN.Empty();

    // 清除所有声音对象绑定的监控学员变化事件
    OnMonitoredStudentChanged.Clear();

    Super::Initialize(Collection);
}

void UInfoSubsystem::Deinitialize()
{
    // 清空像素流学员声音弱引用列表
    MonitoredStudentAudioComponents.Empty();

    Super::Deinitialize();
}

void UInfoSubsystem::GetConfig()
{
     if (IsRunningDedicatedServer())
    {
        UE_LOG(LogTemp, Log, TEXT("DS服务器"));
        
        //TODO 后期中心服务器应该要读取配置文件获取市局端IP
        
        //PakDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Content/Paks/"));
         PakDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Paks/"));
    }
    else
    {
        FString ConfigPath = TEXT("");
#if PLATFORM_WINDOWS
        
        PakDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Paks/"));
       // PakDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Content/Paks/"));
        // 构建配置文件路径
        ConfigPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("json/config.json"));

        
#elif PLATFORM_ANDROID
        
        // 配置文件放在/storage/emulated/0/Android/data/com.lj.pp/files/目录下
        // VR头显上的配置文件路径
        extern FString  GFilePathBase;
      
        PakDirectory = FPaths::Combine(GFilePathBase, TEXT("/Paks/"));
        ConfigPath =  GFilePathBase + TEXT("/json/config.json");
        
#endif
        
        
        // 读取配置文件内容
        FString JsonContent;
        if (FFileHelper::LoadFileToString(JsonContent, *ConfigPath))
        {
            
            UVaRestSubsystem* VaRestSubsystem = GEngine->GetEngineSubsystem<UVaRestSubsystem>();
            if (VaRestSubsystem)
            {
                UVaRestJsonObject* JsonObject = VaRestSubsystem->ConstructVaRestJsonObject();
                if (JsonObject)
                {
                    JsonObject->DecodeJson(JsonContent);
                
                    // 获取 DSIP 字段值
                    DSIP = JsonObject->GetStringField(TEXT("DSIP"));
                
                    UE_LOG(LogTemp, Log, TEXT("成功加载配置文件，DSIP: %s"), *DSIP);
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("无法加载配置文件: %s"), *ConfigPath);
        }
        
        UE_LOG(LogTemp, Log, TEXT("Paks目录是: %s"), *PakDirectory);
    }
   
}

void UInfoSubsystem::SetProcessType(EProcessType LProcessType)
{
    switch (LProcessType)
    {
    case EProcessType::CenterDS:
        UE_LOG(LogTemp, Log, TEXT("我是中心DS"));
        break;
        
    case EProcessType::RoomDS:
        UE_LOG(LogTemp, Log, TEXT("我是房间DS"));
        break;
        
    case EProcessType::ViewPort:
        UE_LOG(LogTemp, Log, TEXT("我是像素流客户端"));
        break;
        
    case EProcessType::JLD:
        UE_LOG(LogTemp, Log, TEXT("我是教练客户端"));
        break;
        
    case EProcessType::PC_XYD:
        UE_LOG(LogTemp, Log, TEXT("我是PC学员客户端"));
        break;
        
    case EProcessType::VR_XYD:
        UE_LOG(LogTemp, Log, TEXT("我是VR学员客户端"));
        break;
        
    default:
        UE_LOG(LogTemp, Error, TEXT("客户端类型错误"));
        break;
    }
    
    ProcessType = LProcessType;
}

bool UInfoSubsystem::SetCurrentMonitoredStudentSN(const FString& LStudentSN)
{
    // 清除SN首尾可能存在的空格，避免相同SN因为格式不同而判断失败
    const FString NormalizedStudentSN = LStudentSN.TrimStartAndEnd();

    // 本次SN没有变化时不重复广播
    if (CurrentMonitoredStudentSN == NormalizedStudentSN)
    {
        return false;
    }

    // 保存切换前的监控学员SN
    const FString PreviousStudentSN = CurrentMonitoredStudentSN;

    // 保存新的监控学员SN
    CurrentMonitoredStudentSN = NormalizedStudentSN;

    // 当前监控学员改变后立即刷新全部已注册声音
    RefreshMonitoredStudentAudioVolumes();

    // 通知任务旁白、NPC和警车等声音刷新当前静音状态
    OnMonitoredStudentChanged.Broadcast(PreviousStudentSN, CurrentMonitoredStudentSN);

    // 输出监控学员变化日志
    UE_LOG(LogTemp, Warning, TEXT("像素流当前监控学员变化：原SN=%s，当前SN=%s"), PreviousStudentSN.IsEmpty() ? TEXT("无") : *PreviousStudentSN, CurrentMonitoredStudentSN.IsEmpty() ? TEXT("无") : *CurrentMonitoredStudentSN);

    // 返回本次监控学员已经发生变化
    return true;
}

bool UInfoSubsystem::IsCurrentMonitoredStudent(const FString& LStudentSN) const
{
    // 清除传入SN首尾可能存在的空格
    const FString NormalizedStudentSN = LStudentSN.TrimStartAndEnd();

    // 没有选中任何学员时不能匹配
    if (CurrentMonitoredStudentSN.IsEmpty())
    {
        return false;
    }

    // 使用学员唯一SN判断是否为当前监控对象
    return CurrentMonitoredStudentSN == NormalizedStudentSN;
}

// 获取指定学员声音在当前客户端应使用的音量
float UInfoSubsystem::GetMonitoredStudentAudioVolume(const FString& LStudentSN) const
{
    // 非像素流客户端不使用监控学员音量筛选
    if (ProcessType != EProcessType::ViewPort)
    {
        return 1.0f;
    }

    // 当前监控学员的声音正常播放，其他学员完全静音
    return IsCurrentMonitoredStudent(LStudentSN) ? 1.0f : 0.0f;
}

// 注册需要跟随当前监控学员切换音量的声音
void UInfoSubsystem::RegisterMonitoredStudentAudio(UAudioComponent* LAudioComponent, const FString& LStudentSN)
{
    // 只有像素流客户端需要维护监控声音列表
    if (ProcessType != EProcessType::ViewPort)
    {
        return;
    }

    // 无效声音组件不能注册
    if (!IsValid(LAudioComponent))
    {
        return;
    }

    // 清理学员SN首尾可能存在的空格
    const FString NormalizedStudentSN = LStudentSN.TrimStartAndEnd();

    // 创建声音组件弱引用，避免阻止声音组件自动销毁
    const TWeakObjectPtr<UAudioComponent> AudioComponentKey = LAudioComponent;

    // 保存或者更新声音组件对应的学员SN
    MonitoredStudentAudioComponents.Add(AudioComponentKey, NormalizedStudentSN);

    // 获取该学员声音当前应该使用的音量
    const float TargetVolume = GetMonitoredStudentAudioVolume(NormalizedStudentSN);

    // 立即设置声音音量，避免未监控学员的声音混入像素流
    LAudioComponent->SetVolumeMultiplier(TargetVolume);

    // 输出注册日志
    UE_LOG(LogTemp, Display, TEXT("像素流学员声音已注册：SN=%s，声音组件=%s，音量=%.1f"), NormalizedStudentSN.IsEmpty() ? TEXT("无") : *NormalizedStudentSN, *GetNameSafe(LAudioComponent), TargetVolume);

}

void UInfoSubsystem::UnregisterMonitoredStudentAudio(UAudioComponent* LAudioComponent)
{
    // 无效组件不需要继续处理
    if (!IsValid(LAudioComponent))
    {
        return;
    }

    // 创建与注册时相同的声音组件弱引用键
    const TWeakObjectPtr<UAudioComponent> AudioComponentKey = LAudioComponent;

    // 从像素流声音列表中移除该组件
    MonitoredStudentAudioComponents.Remove(AudioComponentKey);
}

// 根据所属学员生成附加声音，并自动接入像素流声音路由
UAudioComponent* UInfoSubsystem::SpawnMonitoredStudentSoundAttached(USoundBase* LSound, USceneComponent* LAttachToComponent, FName LAttachPointName, AActor* LStudentActor)
{
    // 声音、附加组件或者学员Actor无效时不能创建声音
    if (!IsValid(LSound) || !IsValid(LAttachToComponent) || !IsValid(LStudentActor))
    {
        UE_LOG(LogTemp, Error, TEXT("生成监控学员附加声音失败：声音、附加组件或学员Actor无效"));
        return nullptr;
    }

    // 将传入Actor转换为学员玩家
    APPPlayerBase* StudentPlayer = Cast<APPPlayerBase>(LStudentActor);

    // 传入Actor不是学员玩家时不能取得SN
    if (!IsValid(StudentPlayer))
    {
        UE_LOG(LogTemp, Error, TEXT("生成监控学员附加声音失败：传入Actor不是学员玩家，Actor=%s"), *GetNameSafe(LStudentActor));
        return nullptr;
    }

    // 获取并整理当前声音所属学员SN
    const FString StudentSN = StudentPlayer->GPlayerInfo.SN.TrimStartAndEnd();

    // 只有像素流客户端必须使用学员SN区分多个学员的声音
    if (ProcessType == EProcessType::ViewPort && StudentSN.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("生成监控学员附加声音失败：像素流客户端的学员SN为空，学员=%s"), *GetNameSafe(StudentPlayer));
        return nullptr;
    }

    // 像素流客户端需要让零音量声音继续推进播放进度
    if (ProcessType == EProcessType::ViewPort)
    {
        LSound->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
    }

    // 获取当前学员声音创建时应该使用的初始音量
    const float InitialVolume = GetMonitoredStudentAudioVolume(StudentSN);

    // 使用正确初始音量创建附加声音，避免未监控学员的声音短暂泄漏
    UAudioComponent* AudioComponent = UGameplayStatics::SpawnSoundAttached(LSound, LAttachToComponent, LAttachPointName, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true, InitialVolume, 1.0f, 0.0f, nullptr, nullptr, true);

    // 检查声音组件是否创建成功
    if (!IsValid(AudioComponent))
    {
        UE_LOG(LogTemp, Error, TEXT("生成监控学员附加声音失败：没有创建出声音组件，SN=%s，声音=%s"), *StudentSN, *GetNameSafe(LSound));
        return nullptr;
    }

    // 将声音组件注册到当前监控学员声音路由
    RegisterMonitoredStudentAudio(AudioComponent, StudentSN);

    // 输出声音生成结果
    //UE_LOG(LogTemp, Warning, TEXT("监控学员附加声音生成完成：SN=%s，声音=%s，组件=%s，初始音量=%.1f"), *StudentSN, *GetNameSafe(LSound), *GetNameSafe(AudioComponent), InitialVolume);

    // 返回声音组件供蓝图继续保存或停止
    return AudioComponent;
}

int32 UInfoSubsystem::GetNewRoomDSPort()
{
    if (!AvailablePorts.IsEmpty())
    {
        int32 LPort;
        LPort = AvailablePorts[0];
        AvailablePorts.RemoveAt(0);
        
        return LPort;
    }
    
    //一个DS似乎要占用3个端口
    TempRoomDSPort = TempRoomDSPort + 3;
    return TempRoomDSPort;
}

void UInfoSubsystem::RecyclingPort(int32 LPort)
{
    AvailablePorts.AddUnique(LPort);
}

void UInfoSubsystem::GetExistingKJ()
{
    
    CoursewareList.Empty();

    if (!FPaths::DirectoryExists(PakDirectory))
    {
        UE_LOG(LogTemp, Error, TEXT("Paks目录不存在: %s"), *PakDirectory);
        return;
    }

    FString SearchPath = FPaths::Combine(PakDirectory, TEXT("*.pak"));
    TArray<FString> PakFiles;
    IFileManager::Get().FindFiles(PakFiles, *SearchPath, true, false);

    TArray<int32> CoursewareIDs;
    
    //pak包名需严格按照规则命名才能识别到
    for (const FString& PakFile : PakFiles)
    {
        FString FileName = FPaths::GetBaseFilename(PakFile, true);

        if (FileName.StartsWith(TEXT("KJ")))
        {
            FString Remaining = FileName.RightChop(2);
            int32 PIndex = Remaining.Find(TEXT("P"));
            if (PIndex != INDEX_NONE && PIndex > 0)
            {
                FString NumberStr = Remaining.Left(PIndex);
                int32 CoursewareID = FCString::Atoi(*NumberStr);
                
                if (CoursewareID > 0)
                {
                    UE_LOG(LogTemp, Log, TEXT("找到课件%d的pak文件"), CoursewareID);
                    CoursewareIDs.Add(CoursewareID);
                }
            }
        }
    }

    //排序
    CoursewareIDs.Sort([](const int32& A, const int32& B) {
        return A < B;
    });

    if (CoursewareIDs.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("未找到任何课件pak文件"));
        return;
    }

    UVaRestJsonObject* JsonObject = nullptr;
    FString JsonPath = FPaths::Combine(PakDirectory, TEXT("KJ.json"));
    
    if (FPaths::FileExists(JsonPath))
    {
        FString JsonContent;
        if (FFileHelper::LoadFileToString(JsonContent, *JsonPath))
        {
            UVaRestSubsystem* VaRestSubsystem = GEngine->GetEngineSubsystem<UVaRestSubsystem>();
            check(VaRestSubsystem);
            
            JsonObject = VaRestSubsystem->ConstructVaRestJsonObject();
            if (JsonObject && !JsonObject->DecodeJson(JsonContent))
            {
                JsonObject = nullptr;
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("无法读取KJ.json文件: %s"), *JsonPath);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("KJ.json文件不存在: %s"), *JsonPath);
    }

    for (int32 ID : CoursewareIDs)
    {
        FCoursewareInfo Info;
        
        if (JsonObject != nullptr)
        {
            FString Key = FString::FromInt(ID);
            UE_LOG(LogTemp, Log, TEXT("查找课件 - ID: %d, Key: '%s'"), ID, *Key);
            
            UVaRestJsonObject* CourseObj = JsonObject->GetObjectField(Key);
            
            if (CourseObj)
            {
                Info.MapId = ID;
                Info.Name = CourseObj->GetStringField(TEXT("Name"));
                Info.Description = CourseObj->GetStringField(TEXT("Description"));
                Info.Image = CourseObj->GetStringField(TEXT("Image"));
                
                //课件类型
                if (CourseObj->GetStringField(TEXT("Type")) == TEXT("JQCZ"))
                {
                    Info.Type = EKJType::JQCZ;
                }else if (CourseObj->GetStringField(TEXT("Type")) == TEXT("SJXL"))
                {
                    Info.Type = EKJType::SJXL;
                }else
                {
                    Info.Type = EKJType::None;
                }
               
                
                UE_LOG(LogTemp, Log, TEXT("  成功获取课件信息 - Id: %d, Name: '%s', Description: '%s', Image: '%s'"), Info.MapId, *Info.Name, *Info.Description, *Info.Image);
                CoursewareList.Add(Info);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("  有pak文件，但在json文件里没有找到Key='%s'对应的课件信息"), *Key);
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("找到 %d 个课件"), CoursewareList.Num());
}

FString UInfoSubsystem::GetKJNameFromMapID(int32 LMapID)
{
    for (int32 i = 0; i < CoursewareList.Num(); i++)
    {
        if (CoursewareList[i].MapId == LMapID)
        {
            return CoursewareList[i].Name;
        }
    }
    
    UE_LOG(LogTemp, Error, TEXT("错误 找不到MapID %d 对应的课件名称"), LMapID);
    return TEXT("");
}

void UInfoSubsystem::RefreshMonitoredStudentAudioVolumes()
{
    // 遍历当前注册的全部声音组件
    for (auto Iterator = MonitoredStudentAudioComponents.CreateIterator(); Iterator; ++Iterator)
    {
        // 获取当前弱引用对应的声音组件
        UAudioComponent* AudioComponent = Iterator.Key().Get();

        // 已经销毁的声音组件直接移除
        if (!IsValid(AudioComponent))
        {
            Iterator.RemoveCurrent();
            continue;
        }

        // 根据声音所属学员SN计算最新音量
        const float TargetVolume = GetMonitoredStudentAudioVolume(Iterator.Value());

        // 应用最新音量
        AudioComponent->SetVolumeMultiplier(TargetVolume);
    }

    // 输出本次声音切换结果
    UE_LOG(LogTemp, Display, TEXT("像素流学员声音路由已刷新：当前SN=%s，声音数量=%d"), CurrentMonitoredStudentSN.IsEmpty() ? TEXT("无") : *CurrentMonitoredStudentSN, MonitoredStudentAudioComponents.Num());

}

