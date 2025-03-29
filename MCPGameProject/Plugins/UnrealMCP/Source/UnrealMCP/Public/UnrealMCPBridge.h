#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Http.h"
#include "Json.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "UnrealMCPBridge.generated.h"

class FMCPServerRunnable;
class FUnrealMCPActorCommands;
class FUnrealMCPEditorCommands;
class FUnrealMCPBlueprintCommands;
class FUnrealMCPBlueprintNodeCommands;

// Forward declarations for Blueprint API classes
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UK2Node_Event;
class UK2Node_CallFunction;
class UK2Node_VariableGet;
class UK2Node_VariableSet;
class UK2Node_InputAction;
class UK2Node_Self;
class UFunction;
class UBlueprint;

/**
 * Editor subsystem for MCP Bridge
 */
UCLASS()
class UNREALMCP_API UUnrealMCPBridge : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	// UEditorSubsystem implementation
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Server functions
	void StartServer();
	void StopServer();
	bool IsRunning() const { return bIsRunning; }

	// Command execution
	FString ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

protected:
	// Handle actor-related commands
	TSharedPtr<FJsonObject> HandleActorCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

	// Convert an actor to a JSON value
	TSharedPtr<FJsonValue> ActorToJson(AActor* Actor);

	// Convert an actor to a JSON object
	TSharedPtr<FJsonObject> ActorToJsonObject(AActor* Actor, bool bDetailed = false);

private:
	// Server state
	bool bIsRunning;
	TSharedPtr<FSocket> ListenerSocket;
	TSharedPtr<FSocket> ConnectionSocket;
	FRunnableThread* ServerThread;

	// Server configuration
	FIPv4Address ServerAddress;
	uint16 Port;

	// Command handler instances
	TSharedPtr<FUnrealMCPActorCommands> ActorCommands;
	TSharedPtr<FUnrealMCPEditorCommands> EditorCommands;
	TSharedPtr<FUnrealMCPBlueprintCommands> BlueprintCommands;
	TSharedPtr<FUnrealMCPBlueprintNodeCommands> BlueprintNodeCommands;

	// Command handlers
	TSharedPtr<FJsonObject> HandleLevelCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAssetCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleEditorCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleBlueprintCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);
    
    // Blueprint node command handler
    TSharedPtr<FJsonObject> HandleBlueprintNodeCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);
    
    // New command handlers for blueprint property and self reference
    TSharedPtr<FJsonObject> HandleSetBlueprintProperty(const TSharedPtr<FJsonObject>& RequestObj);
    TSharedPtr<FJsonObject> HandleAddBlueprintSelfReference(const TSharedPtr<FJsonObject>& RequestObj);
    TSharedPtr<FJsonObject> HandleFindBlueprintNodes(const TSharedPtr<FJsonObject>& RequestObj);
    TSharedPtr<FJsonObject> HandleSetStaticMeshPropertiesCommand(const TSharedPtr<FJsonObject>& RequestObj);
    
    // Blueprint component functions
    UBlueprint* FindBlueprintByName(const FString& BlueprintName);
    TSharedPtr<FJsonObject> AddComponentToBlueprint(const FString& BlueprintName, const FString& ComponentType, 
                                                   const FString& ComponentName, const FString& MeshType,
                                                   const TArray<float>& Location, const TArray<float>& Rotation,
                                                   const TArray<float>& Scale, const TSharedPtr<FJsonObject>& ComponentProperties);
    
    // Helper functions for blueprint node operations
    UBlueprint* FindBlueprint(const FString& BlueprintName);
    UEdGraph* FindOrCreateEventGraph(UBlueprint* Blueprint);
    UK2Node_Event* CreateEventNode(UEdGraph* Graph, const FString& EventType, const FVector2D& Position);
    UK2Node_CallFunction* CreateFunctionCallNode(UEdGraph* Graph, UFunction* Function, const FVector2D& Position);
    UK2Node_VariableGet* CreateVariableGetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position);
    UK2Node_VariableSet* CreateVariableSetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position);
    UK2Node_InputAction* CreateInputActionNode(UEdGraph* Graph, const FString& ActionName, const FVector2D& Position);
    bool ConnectGraphNodes(UEdGraph* Graph, UEdGraphNode* SourceNode, const FString& SourcePinName, 
                          UEdGraphNode* TargetNode, const FString& TargetPinName);
    UEdGraphPin* FindPin(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction = EGPD_MAX);

    // Helper functions
    void GetIntArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<int32>& OutArray);
}; 