#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Debug/log-related MCP commands.
 *
 * Lets an external AI read the Unreal Engine output log to self-diagnose
 * runtime issues (e.g. checking UE_LOG output from gameplay classes) without
 * needing a human to copy/paste log text.
 */
class UNREALMCP_API FUnrealMCPDebugCommands
{
public:
    FUnrealMCPDebugCommands();

    // Handle debug commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Specific debug command handlers
    TSharedPtr<FJsonObject> HandleGetOutputLog(const TSharedPtr<FJsonObject>& Params);
};
