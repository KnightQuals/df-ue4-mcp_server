# Refactoring Plan for UnrealMCPBridge

## Overview
The UnrealMCPBridge class has grown too large (4000+ lines) and needs to be refactored into smaller, more manageable components. This document outlines the plan to split it into separate command handlers and a utility class.

## File Structure
```
Source/UnrealMCP/
├── Public/
│   ├── UnrealMCPBridge.h
│   └── Commands/
│       ├── UnrealMCPActorCommands.h
│       ├── UnrealMCPEditorCommands.h
│       ├── UnrealMCPBlueprintCommands.h
│       ├── UnrealMCPBlueprintNodeCommands.h
│       └── UnrealMCPCommonUtils.h  
├── Private/
│   ├── UnrealMCPBridge.cpp
│   └── Commands/
│       ├── UnrealMCPActorCommands.cpp
│       ├── UnrealMCPEditorCommands.cpp
│       ├── UnrealMCPBlueprintCommands.cpp
│       ├── UnrealMCPBlueprintNodeCommands.cpp
│       └── UnrealMCPCommonUtils.cpp
```

## Implementation Steps

### Step 1: Set Up Structure (5 min)
- Create `Commands` folders in Public and Private
- Create all header and implementation files

### Step 2: Implement Common Utils (5 min)
- Move all utility methods from `UnrealMCPBridge.cpp` to `UnrealMCPCommonUtils.cpp`
- Actor utilities: `ActorToJson`, `ActorToJsonObject`
- Blueprint utilities: `FindBlueprint`, `FindBlueprintByName`, `FindOrCreateEventGraph`
- Blueprint node utilities: All node creation and connection methods
- JSON utilities: `GetIntArrayFromJson` and other parsing helpers

### Step 3: Implement Command Handlers (10 min)
- Cut & paste each command handler from `UnrealMCPBridge.cpp` to the appropriate implementation file
- For each handler method that uses utility functions, modify to use `FUnrealMCPCommonUtils` instead
- Update the top-level `HandleCommand` in each implementation to route to the appropriate handler

### Step 4: Modify UnrealMCPBridge (10 min)
- Update header file with new member variables
- Add includes for new handler classes
- Initialize command handlers in `Initialize`
- Replace command handling in `ExecuteCommand` with delegation to handlers
- Remove all command handler methods and utility methods that have been moved

## Command Distribution

### FUnrealMCPActorCommands:
- `get_actors_in_level`
- `find_actors_by_name`
- `create_actor`
- `delete_actor`
- `set_actor_transform`
- `get_actor_properties`

### FUnrealMCPEditorCommands:
- `focus_viewport`
- `take_screenshot`

### FUnrealMCPBlueprintCommands:
- `create_blueprint`
- `add_component_to_blueprint`
- `set_component_property`
- `set_physics_properties`
- `compile_blueprint`
- `spawn_blueprint_actor`
- `set_blueprint_property`
- `set_static_mesh_properties`

### FUnrealMCPBlueprintNodeCommands:
- `connect_blueprint_nodes`
- `create_input_mapping`
- `add_blueprint_get_self_component_reference`
- `add_blueprint_self_reference`
- `find_blueprint_nodes`
- `add_blueprint_event_node`
- `add_blueprint_input_action_node`
- `add_blueprint_function_node`
- `add_blueprint_get_component_node`
- `add_blueprint_variable`

## Utility Methods in UnrealMCPCommonUtils

### Actor Utilities
- `ActorToJson`
- `ActorToJsonObject`

### Blueprint Utilities
- `FindBlueprint`
- `FindBlueprintByName`
- `FindOrCreateEventGraph`

### Blueprint Node Utilities
- `CreateEventNode`
- `CreateFunctionCallNode`
- `CreateVariableGetNode`
- `CreateVariableSetNode`
- `CreateInputActionNode`
- `CreateSelfReferenceNode`
- `ConnectGraphNodes`
- `FindPin`

### JSON Utilities
- `GetIntArrayFromJson`
- `GetFloatArrayFromJson`
- `GetVector2DFromJson`
- `GetVectorFromJson`
- `GetRotatorFromJson`
- `CreateErrorResponse`
- `CreateSuccessResponse`

## Modified ExecuteCommand Function

```cpp
FString UUnrealMCPBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Executing command: %s"), *CommandType);
    
    TPromise<FString> Promise;
    TFuture<FString> Future = Promise.GetFuture();
    
    AsyncTask(ENamedThreads::GameThread, [this, CommandType, Params, Promise = MoveTemp(Promise)]() mutable
    {
        TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);
        
        try
        {
            TSharedPtr<FJsonObject> ResultJson;
            
            if (CommandType == TEXT("ping"))
            {
                ResultJson = MakeShareable(new FJsonObject);
                ResultJson->SetStringField(TEXT("message"), TEXT("pong"));
            }
            // Actor Commands
            else if (CommandType == TEXT("get_actors_in_level") || 
                     CommandType == TEXT("find_actors_by_name") ||
                     CommandType == TEXT("create_actor") || 
                     CommandType == TEXT("delete_actor") || 
                     CommandType == TEXT("set_actor_transform") ||
                     CommandType == TEXT("get_actor_properties"))
            {
                ResultJson = ActorCommands->HandleCommand(CommandType, Params);
            }
            // Editor Commands
            else if (CommandType == TEXT("focus_viewport") || 
                     CommandType == TEXT("take_screenshot"))
            {
                ResultJson = EditorCommands->HandleCommand(CommandType, Params);
            }
            // Blueprint Commands
            else if (CommandType == TEXT("create_blueprint") || 
                     CommandType == TEXT("add_component_to_blueprint") || 
                     CommandType == TEXT("set_component_property") || 
                     CommandType == TEXT("set_physics_properties") || 
                     CommandType == TEXT("compile_blueprint") || 
                     CommandType == TEXT("spawn_blueprint_actor") || 
                     CommandType == TEXT("set_blueprint_property") || 
                     CommandType == TEXT("set_static_mesh_properties"))
            {
                ResultJson = BlueprintCommands->HandleCommand(CommandType, Params);
            }
            // Blueprint Node Commands
            else if (CommandType == TEXT("connect_blueprint_nodes") || 
                     CommandType == TEXT("create_input_mapping") || 
                     CommandType == TEXT("add_blueprint_get_self_component_reference") ||
                     CommandType == TEXT("add_blueprint_self_reference") ||
                     CommandType == TEXT("find_blueprint_nodes") ||
                     CommandType == TEXT("add_blueprint_event_node") ||
                     CommandType == TEXT("add_blueprint_input_action_node") ||
                     CommandType == TEXT("add_blueprint_function_node") ||
                     CommandType == TEXT("add_blueprint_get_component_node") ||
                     CommandType == TEXT("add_blueprint_variable"))
            {
                ResultJson = BlueprintNodeCommands->HandleCommand(CommandType, Params);
            }
            else
            {
                ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                ResponseJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown command: %s"), *CommandType));
                
                FString ResultString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
                FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
                Promise.SetValue(ResultString);
                return;
            }
            
            ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
            ResponseJson->SetObjectField(TEXT("result"), ResultJson);
        }
        catch (const std::exception& e)
        {
            ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
            ResponseJson->SetStringField(TEXT("error"), UTF8_TO_TCHAR(e.what()));
        }
        
        FString ResultString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
        FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
        Promise.SetValue(ResultString);
    });
    
    return Future.Get();
}
```

## Implementation Tips

1. **Work in parallel**: Have one person create file structure and headers while another prepares code blocks

2. **Copy-paste intelligently**: Use search and replace to quickly change references from class methods to utility methods

3. **Build incrementally**: Build after each major step to catch errors quickly

4. **Comment out rather than delete**: If unsure, comment out code temporarily

5. **Focus on correctness over elegance**: Get it working first

6. **Use IDE features**: Use IDE refactoring tools to help with moving code

## Process Summary
1. Create file structure
2. Copy utility methods to CommonUtils
3. Copy command handlers to respective files
4. Update references to use the new structure
5. Modify UnrealMCPBridge.cpp to delegate
6. Final build and test 