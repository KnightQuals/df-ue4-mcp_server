#include "Commands/UnrealMCPDebugCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"

FUnrealMCPDebugCommands::FUnrealMCPDebugCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPDebugCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("get_output_log"))
    {
        return HandleGetOutputLog(Params);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown debug command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPDebugCommands::HandleGetOutputLog(const TSharedPtr<FJsonObject>& Params)
{
    // Optional parameters.
    FString Filter;
    if (Params.IsValid())
    {
        Params->TryGetStringField(TEXT("filter"), Filter);
    }

    int32 MaxLines = 50;
    if (Params.IsValid())
    {
        Params->TryGetNumberField(TEXT("max_lines"), MaxLines);
    }
    if (MaxLines <= 0)
    {
        MaxLines = 50;
    }

    // The engine's active log file for this session is Saved/Logs/<ProjectName>.log.
    // Build the path with FPaths::Combine (handles slash normalization) and resolve it to an
    // absolute path, since relative paths can behave inconsistently depending on the process's
    // current working directory.
    const FString LogFilePath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Logs"), TEXT("MCPGameProject.log")));

    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*LogFilePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Log file not found: %s"), *LogFilePath));
    }

    // The running editor process holds the log file open for writing, so it must be opened with
    // FILEREAD_AllowWrite or the read will fail on platforms that enforce exclusive write locks.
    FString FileContents;
    if (!FFileHelper::LoadFileToString(FileContents, *LogFilePath, FFileHelper::EHashOptions::None, FILEREAD_AllowWrite))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to read log file: %s"), *LogFilePath));
    }

    // Split into individual lines (handles both \r\n and \n line endings).
    TArray<FString> AllLines;
    FileContents.ParseIntoArrayLines(AllLines, /*bCullEmpty=*/false);

    // Apply the keyword filter first (if any), then take the tail of the result.
    TArray<FString> FilteredLines;
    if (Filter.IsEmpty())
    {
        FilteredLines = AllLines;
    }
    else
    {
        for (const FString& Line : AllLines)
        {
            if (Line.Contains(Filter))
            {
                FilteredLines.Add(Line);
            }
        }
    }

    const int32 TotalMatches = FilteredLines.Num();
    const int32 StartIndex = FMath::Max(0, TotalMatches - MaxLines);

    TArray<TSharedPtr<FJsonValue>> LinesJson;
    for (int32 i = StartIndex; i < TotalMatches; ++i)
    {
        LinesJson.Add(MakeShared<FJsonValueString>(FilteredLines[i]));
    }

    TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
    ResultData->SetArrayField(TEXT("lines"), LinesJson);
    ResultData->SetNumberField(TEXT("total"), TotalMatches);

    return FUnrealMCPCommonUtils::CreateSuccessResponse(ResultData);
}
