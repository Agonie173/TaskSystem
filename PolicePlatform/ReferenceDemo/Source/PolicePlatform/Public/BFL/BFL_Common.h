// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HAL/PlatformProcess.h"
#include "BFL_Common.generated.h"

/**
 * 
 */
UCLASS()
class POLICEPLATFORM_API UBFL_Common : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	// 执行bat脚本
	UFUNCTION(BlueprintCallable, Category = "警务|BFL")
	static bool ExecuteBatchScript(const FString& ScriptPath);
	
	// 动态构建bat脚本-房间DS
	UFUNCTION(BlueprintCallable, Category = "警务|BFL")
	static void GenerateBatchScript_RoomDS(int32 Port, int32 MapID, int32 RoomID, FString& BatFilePath);

	// 动态构建bat脚本-像素流客户端
	UFUNCTION(BlueprintCallable, Category = "警务|BFL")
	static void GenerateBatchScript_XSLClient(int32 Port, int32 MapID, int32 RoomID, FString& BatFilePath);
	
	// 判断是否在编辑器中运行
	UFUNCTION(BlueprintPure, Category = "警务|BFL")
	static bool IsRunningInEditor();
	
	//根据KJID获取pak文件路径
	UFUNCTION(BlueprintCallable, Category = "警务|BFL")
	static FString GetPakFromKJID(UObject* WorldContextObject, int32 KJID);

	//挂载指定KJID的pak文件
	UFUNCTION(BlueprintCallable, Category = "警务|BFL")
	static bool MountPakFile(UObject* WorldContextObject, int32 KJID);

	//卸载指定KJID的pak文件
	UFUNCTION(BlueprintCallable, Category = "警务|BFL")
	static bool UnmountPakFile(UObject* WorldContextObject, int32 KJID);

	//匹配字符串是否包含全部关键词，匹配成功返回true并把字符串通过OutString返回
	UFUNCTION(BlueprintCallable, Category = "警务|BFL")
	static bool MatchKeywords(const FString& InString, const TArray<FString>& Keywords, FString& OutString);

	//将字符串中的关键词转换为富文本格式，如 <title>关键词</>
	UFUNCTION(BlueprintCallable, Category = "警务|BFL")
	static FString ConvertRichTextFormat(const FString& InString, const TArray<FString>& Keywords, const FString& RichTextFormat);
};
