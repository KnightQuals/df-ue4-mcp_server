#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Material-related MCP commands.
 *
 * Lets an external AI create/edit material assets and assign them to mesh
 * components, which closes the MCP boundary that previously prevented the
 * BattleSectorAnchor recolor from working (the Blueprint mesh had no usable
 * vector parameter material that the C++ side could drive).
 */
class UNREALMCP_API FUnrealMCPMaterialCommands
{
public:
    FUnrealMCPMaterialCommands();

    // Handle material commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Specific material command handlers
    TSharedPtr<FJsonObject> HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddVectorParameter(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMaterialOnComponent(const TSharedPtr<FJsonObject>& Params);
};
