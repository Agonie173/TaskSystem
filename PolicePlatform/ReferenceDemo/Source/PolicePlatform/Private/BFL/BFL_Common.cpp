// Fill out your copyright notice in the Description page of Project Settings.


#include "BFL/BFL_Common.h"
#include "Tools/InfoSubsystem.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformFilemanager.h"

bool UBFL_Common::ExecuteBatchScript(const FString& ScriptPath)
{
	if (!FPaths::FileExists(ScriptPath))
	{
		UE_LOG(LogTemp, Error, TEXT("脚本不存在: %s"), *ScriptPath);
		return false;
	}

	FString CmdPath = TEXT("C:\\Windows\\System32\\cmd.exe");
	FString CommandLine = FString::Printf(TEXT("/c \"%s\""), *ScriptPath);

	FProcHandle ProcHandle = FPlatformProcess::CreateProc(
		*CmdPath,                  // 程序路径
		*CommandLine,              // 命令行
		true,                      // bLaunchDetached - 分离进程
		false,                     // bCreateSuspended - 不挂起
		false,                     // bHideWindow - 不隐藏窗口（显示服务器日志）
		nullptr,                   // 进程ID - 不需要
		0,                         // 窗口模式 - 0 表示正常窗口
		nullptr,                   // 工作目录 - 使用默认
		nullptr,                   // 写入管道 - 不需要
		nullptr                    // 读取管道 - 不需要
	);
	
	UE_LOG(LogTemp, Log, TEXT("动态拉起脚本命令已执行！"));
	
	return true;
}

void UBFL_Common::GenerateBatchScript_RoomDS(int32 Port, int32 MapID, int32 RoomID, FString& BatFilePath)
{
	// 1. 拼接bat内容，exe文件是在bat文件的上一级目录下	
	FString BatContent;
	BatContent.Append("@echo off\n");
	BatContent.Append("chcp 65001\n");
	BatContent.Append("cd /d \"%~dp0\"\n");
	BatContent.Append(FString::Printf(TEXT("start \"\" \"..\\PolicePlatformServer.exe\" -Port=%d -log=Room/%d.log -MapID=%d -RoomID=%d -log "), Port, MapID, MapID, RoomID));
	BatContent.Append("exit\n");

	//TODO：注意新文件和旧文件之间是否会冲突
	// 2. 写入文件，使用绝对路径
	BatFilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / "StartRoomDS.bat");
	FFileHelper::SaveStringToFile(BatContent, *BatFilePath);
	
	UE_LOG(LogTemp, Log, TEXT("生成bat脚本文件 %s"), *BatFilePath);
}

void UBFL_Common::GenerateBatchScript_XSLClient(int32 Port, int32 MapID, int32 RoomID, FString& BatFilePath)
{
	// 1. 拼接bat内容，exe文件是在bat文件的上一级目录下	
	//注意这里的-ResX=1628 -ResY=1628分辨率比例和UI的布局有关，可以按比例修改，但如果比例变了，可能会导致UI布局错乱
	//--D3D12PSOCache=1 -SavePSOCache -LoadPSOCache 
	//-nopstoprecache
	//-FixedTimestep=1/40 -MaxTickRate=40

	// 编辑器测试时，调用已经打包好的 WindowsClient。
	// 正式打包运行时，保持原来的相对路径。
#if WITH_EDITOR
	const FString ClientExePath =
		TEXT("E:\\UE\\JW\\WindowsClient\\PolicePlatformClient.exe");
#else
	const FString ClientExePath =
		TEXT("..\\PolicePlatformClient.exe");
#endif

	FString BatContent;
	BatContent.Append("@echo off\n");
	BatContent.Append("chcp 65001\n");
	BatContent.Append("cd /d \"%~dp0\"\n");
	BatContent.Append(FString::Printf(TEXT("start \"\" \"%s\" -ClientType=ViewPort -Port=%d -MapID=%d -RoomID=%d "
		"-log=XSL/xsl%d.log -AudioMixer "
		"-PixelStreamingURL=ws://127.0.0.1:8888 "
		"-PixelStreamingID=%d "
		"-PixelStreamingCodec=H264 "
		"-PixelStreamingEnableNVENC "
		"-RenderOffScreen  -ForceRes -ResX=1628 -ResY=1628 -NoWindow "
		"-dx12 "
		" \n"), *ClientExePath, Port, MapID, RoomID, MapID, RoomID));//林新增*ClientExePath
	
	BatContent.Append("exit\n");

	//TODO：注意新文件和旧文件之间是否会冲突
	// 2. 写入文件，使用绝对路径
	BatFilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / "StartViewPortClient.bat");
	FFileHelper::SaveStringToFile(BatContent, *BatFilePath);
	
	UE_LOG(LogTemp, Log, TEXT("生成bat脚本文件 %s"), *BatFilePath);
}

// 判断当前是否运行在包含编辑器功能的开发版本中
bool UBFL_Common::IsRunningInEditor()
{
#if WITH_EDITOR
	return true;
#else
	return false;
#endif
}

FString UBFL_Common::GetPakFromKJID(UObject* WorldContextObject, int32 KJID)
{
	// 检查调用该函数的上下文对象
	check(WorldContextObject);

	// 获取当前世界
	UWorld* World = WorldContextObject->GetWorld();

	// 检查当前世界
	check(World);

	// 获取当前GameInstance
	UGameInstance* GameInstance = World->GetGameInstance();

	// 检查当前GameInstance
	check(GameInstance);

	// 获取信息子系统
	UInfoSubsystem* InfoSubsystem = GameInstance->GetSubsystem<UInfoSubsystem>();

	// 检查信息子系统
	check(InfoSubsystem);

	// 获取课件Pak存放目录
	const FString PakDirectory = InfoSubsystem->PakDirectory;

	// 构造当前课件的文件名前缀
	const FString SearchPrefix = FString::Printf(TEXT("KJ%dPP-"), KJID);

	// 保存当前程序需要匹配的平台名称
	FString ExpectedPlatformMarker;

	// Dedicated Server只能读取WindowsServer版本的课件Pak
	if (IsRunningDedicatedServer())
	{
		ExpectedPlatformMarker = TEXT("WindowsServer");
	}
	else
	{
#if PLATFORM_ANDROID
		// Android客户端匹配Android或Android_ASTC版本的课件Pak
		ExpectedPlatformMarker = TEXT("Android");
#elif PLATFORM_WINDOWS
		// Windows学员端、教练端和像素流端匹配WindowsClient版本的课件Pak
		ExpectedPlatformMarker = TEXT("WindowsClient");
#else
		// 未支持的平台使用不会匹配任何正常Pak的标记
		ExpectedPlatformMarker = TEXT("UnsupportedPlatform");
#endif
	}

	// 输出本次课件Pak的筛选条件
	UE_LOG(LogTemp, Warning, TEXT("开始查找课件Pak：课件ID=%d，目录=%s，前缀=%s，目标平台=%s"), KJID, *PakDirectory, *SearchPrefix, *ExpectedPlatformMarker);

	// 保存Pak目录中的全部Pak文件
	TArray<FString> FoundFiles;

	// 获取当前平台文件系统
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// 查找Pak目录中的全部Pak文件
	PlatformFile.FindFiles(FoundFiles, *PakDirectory, TEXT("pak"));

	// 遍历找到的Pak文件
	for (const FString& FilePath : FoundFiles)
	{
		// 取得不包含目录的Pak文件名
		const FString FileName = FPaths::GetCleanFilename(FilePath);

		// 判断文件是否属于当前课件
		const bool bMatchesCourse = FileName.StartsWith(SearchPrefix, ESearchCase::IgnoreCase);

		// 判断文件是否属于当前运行平台
		const bool bMatchesPlatform = FileName.Contains(ExpectedPlatformMarker, ESearchCase::IgnoreCase);

		// 判断文件扩展名是否为Pak
		const bool bIsPakFile = FileName.EndsWith(TEXT(".pak"), ESearchCase::IgnoreCase);

		// 输出当前文件的筛选结果
		UE_LOG(LogTemp, Warning, TEXT("检查课件Pak：文件=%s，匹配课件=%s，匹配平台=%s，扩展名正确=%s"), *FileName, bMatchesCourse ? TEXT("是") : TEXT("否"), bMatchesPlatform ? TEXT("是") : TEXT("否"), bIsPakFile ? TEXT("是") : TEXT("否"));

		// 跳过课件、平台或扩展名不匹配的文件
		if (!bMatchesCourse || !bMatchesPlatform || !bIsPakFile)
		{
			continue;
		}

		// 组合最终Pak完整路径
		const FString FullPath = FPaths::Combine(PakDirectory, FileName);

		// 输出最终匹配的Pak
		UE_LOG(LogTemp, Warning, TEXT("找到匹配课件Pak：课件ID=%d，平台=%s，文件=%s"), KJID, *ExpectedPlatformMarker, *FullPath);

		// 返回正确平台的课件Pak路径
		return FullPath;
	}

	// 没有找到当前平台的课件Pak时输出明确错误
	UE_LOG(LogTemp, Error, TEXT("未找到当前平台对应的课件Pak：课件ID=%d，目标平台=%s，目录=%s"), KJID, *ExpectedPlatformMarker, *PakDirectory);

	// 返回空路径，交给MountPakFile安全终止挂载
	return FString();
}

bool UBFL_Common::MountPakFile(UObject* WorldContextObject, int32 KJID)
{
	/*if (UBFL_Common::IsRunningInEditor())
	{
		return false;
	}
	
	FString PakFilePath = GetPakFromKJID(WorldContextObject, KJID);
	check(!PakFilePath.IsEmpty());*/

#if WITH_EDITOR
	// UnrealEditor启动的编辑器、游戏和服务器进程都直接读取项目源资源
	UE_LOG(LogTemp, Log, TEXT("编辑器开发版本跳过课件Pak挂载：KJID=%d"), KJID);
	return true;
#endif

	// 正式打包程序根据课件ID查找对应的Pak文件
	const FString PakFilePath = GetPakFromKJID(WorldContextObject, KJID);

	// 正式环境缺少课件Pak时记录错误并安全返回，不能使用check导致程序崩溃
	if (PakFilePath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("课件Pak挂载失败：未找到KJID=%d对应的Pak文件"), KJID);
		return false;
	}

	if (!FPaths::FileExists(PakFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Pak文件找不到: %s"), *PakFilePath);
		return false;
	}

	if (FCoreDelegates::MountPak.IsBound())
	{
		IPakFile* MountedPak = FCoreDelegates::MountPak.Execute(PakFilePath, 0);
		if (MountedPak)
		{
			UE_LOG(LogTemp, Log, TEXT("成功挂载pak: %s"), *PakFilePath);

			FString MountPoint = FString::Printf(TEXT("/KJ%d/"), KJID);
			FString ContentPath = FString::Printf(TEXT("../../../PolicePlatform/Plugins/KJ%d/Content/"), KJID);
			
			FPackageName::RegisterMountPoint(MountPoint, ContentPath);
			UE_LOG(LogTemp, Log, TEXT("注册挂载点: %s -> %s"), *MountPoint, *ContentPath);
			
			return true;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("挂载pak失败: %s"), *PakFilePath);
			return false;
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("pak挂载失败 MountPak delegate 还没有绑定"));
		return false;
	}
}

bool UBFL_Common::UnmountPakFile(UObject* WorldContextObject, int32 KJID)
{
	if (UBFL_Common::IsRunningInEditor())
	{
		return false;
	}
	
	FString PakFilePath = GetPakFromKJID(WorldContextObject, KJID);
	
	bool bResult = false;
	if (!PakFilePath.IsEmpty() && FPaths::FileExists(PakFilePath))
	{
		if (FCoreDelegates::OnUnmountPak.IsBound())
		{
			bResult = FCoreDelegates::OnUnmountPak.Execute(PakFilePath);
			if (bResult)
			{
				UE_LOG(LogTemp, Log, TEXT("成功卸载pak: %s"), *PakFilePath);
				GEngine->PerformGarbageCollectionAndCleanupActors();
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("卸载pak失败: %s"), *PakFilePath);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("卸载pak失败 UnmountPak delegate还没绑定"));
		}
	}

	FString MountPoint = FString::Printf(TEXT("/KJ%d/"), KJID);
	FString ContentPath = FString::Printf(TEXT("../../../PolicePlatform/Plugins/KJ%d/Content/"), KJID);
	
	FPackageName::UnRegisterMountPoint(MountPoint, ContentPath);
	UE_LOG(LogTemp, Log, TEXT("移除挂载点: %s -> %s"), *MountPoint, *ContentPath);

	return bResult;
}

bool UBFL_Common::MatchKeywords(const FString& InString, const TArray<FString>& Keywords, FString& OutString)
{
	if (Keywords.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("关键词数组为空了"));
		OutString = FString();
		return false;
	}

	for (const FString& Keyword : Keywords)
	{
		if (!InString.Contains(Keyword))
		{
			OutString = FString();
			return false;
		}
	}

	OutString = InString;
	return true;
}

FString UBFL_Common::ConvertRichTextFormat(const FString& InString, const TArray<FString>& Keywords, const FString& RichTextFormat)
{
	if (Keywords.Num() == 0 || RichTextFormat.IsEmpty() || InString.IsEmpty())
	{
		return InString;
	}

	FString Result;
	int32 Pos = 0;

	while (Pos < InString.Len())
	{
		bool bMatched = false;
		int32 BestMatchLen = 0;

		// 在当前位置查找最长匹配的关键词
		for (const FString& Keyword : Keywords)
		{
			int32 KeywordLen = Keyword.Len();
			if (KeywordLen > 0 && Pos + KeywordLen <= InString.Len())
			{
				if (InString.Mid(Pos, KeywordLen) == Keyword && KeywordLen > BestMatchLen)
				{
					BestMatchLen = KeywordLen;
					bMatched = true;
				}
			}
		}

		if (bMatched)
		{
			Result.Append(FString::Printf(TEXT("<%s>%s</>"), *RichTextFormat, *InString.Mid(Pos, BestMatchLen)));
			Pos += BestMatchLen;
		}
		else
		{
			Result.AppendChar(InString[Pos]);
			Pos++;
		}
	}

	return Result;
}


