#include "UnrealMCPBridge.h"
#include "MCPServerRunnable.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "JsonObjectConverter.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
// Add Blueprint related includes
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
// UE5.5 correct includes
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
// Blueprint Graph specific includes
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "GameFramework/InputSettings.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"

// Default settings
#define MCP_SERVER_HOST "127.0.0.1"
#define MCP_SERVER_PORT 55557

// Initialize subsystem
void UUnrealMCPBridge::Initialize(FSubsystemCollectionBase& Collection)
{
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Initializing"));
    
    bIsRunning = false;
    ListenerSocket = nullptr;
    ConnectionSocket = nullptr;
    ServerThread = nullptr;
    Port = MCP_SERVER_PORT;
    FIPv4Address::Parse(MCP_SERVER_HOST, ServerAddress);

    // Start the server automatically
    StartServer();
}

// Clean up resources when subsystem is destroyed
void UUnrealMCPBridge::Deinitialize()
{
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Shutting down"));
    StopServer();
}

// Start the MCP server
void UUnrealMCPBridge::StartServer()
{
    if (bIsRunning)
    {
        UE_LOG(LogTemp, Warning, TEXT("UnrealMCPBridge: Server is already running"));
        return;
    }

    // Create socket subsystem
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to get socket subsystem"));
        return;
    }

    // Create listener socket
    TSharedPtr<FSocket> NewListenerSocket = MakeShareable(SocketSubsystem->CreateSocket(NAME_Stream, TEXT("UnrealMCPListener"), false));
    if (!NewListenerSocket.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to create listener socket"));
        return;
    }

    // Allow address reuse for quick restarts
    NewListenerSocket->SetReuseAddr(true);
    NewListenerSocket->SetNonBlocking(true);

    // Bind to address
    FIPv4Endpoint Endpoint(ServerAddress, Port);
    if (!NewListenerSocket->Bind(*Endpoint.ToInternetAddr()))
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to bind listener socket to %s:%d"), *ServerAddress.ToString(), Port);
        return;
    }

    // Start listening
    if (!NewListenerSocket->Listen(5))
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to start listening"));
        return;
    }

    ListenerSocket = NewListenerSocket;
    bIsRunning = true;
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Server started on %s:%d"), *ServerAddress.ToString(), Port);

    // Start server thread
    ServerThread = FRunnableThread::Create(
        new FMCPServerRunnable(this, ListenerSocket),
        TEXT("UnrealMCPServerThread"),
        0, TPri_Normal
    );

    if (!ServerThread)
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to create server thread"));
        StopServer();
        return;
    }
}

// Stop the MCP server
void UUnrealMCPBridge::StopServer()
{
    if (!bIsRunning)
    {
        return;
    }

    bIsRunning = false;

    // Clean up thread
    if (ServerThread)
    {
        ServerThread->Kill(true);
        delete ServerThread;
        ServerThread = nullptr;
    }

    // Close sockets
    if (ConnectionSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ConnectionSocket.Get());
        ConnectionSocket.Reset();
    }

    if (ListenerSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket.Get());
        ListenerSocket.Reset();
    }

    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Server stopped"));
}

// Execute a command received from a client
FString UUnrealMCPBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Executing command: %s"), *CommandType);
    
    // Create a promise to wait for the result
    TPromise<FString> Promise;
    TFuture<FString> Future = Promise.GetFuture();
    
    // Queue execution on Game Thread
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
            else if (CommandType == TEXT("get_actors_in_level"))
            {
                ResultJson = HandleActorCommand(TEXT("get_actors_in_level"), Params);
            }
            else if (CommandType == TEXT("find_actors_by_name"))
            {
                if (!Params->HasField(TEXT("pattern")))
                {
                    ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                    ResponseJson->SetStringField(TEXT("error"), TEXT("Missing 'pattern' parameter"));
                }
                else
                {
                    ResultJson = HandleActorCommand(TEXT("find_actors_by_name"), Params);
                }
            }
            else if (CommandType == TEXT("create_actor"))
            {
                ResultJson = HandleActorCommand(TEXT("create_actor"), Params);
            }
            else if (CommandType == TEXT("delete_actor"))
            {
                ResultJson = HandleActorCommand(TEXT("delete_actor"), Params);
            }
            else if (CommandType == TEXT("set_actor_transform"))
            {
                ResultJson = HandleActorCommand(TEXT("set_actor_transform"), Params);
            }
            else if (CommandType == TEXT("get_actor_properties"))
            {
                ResultJson = HandleActorCommand(TEXT("get_actor_properties"), Params);
            }
            else if (CommandType == TEXT("focus_viewport"))
            {
                ResultJson = HandleEditorCommand(TEXT("focus_viewport"), Params);
            }
            else if (CommandType == TEXT("take_screenshot"))
            {
                ResultJson = HandleEditorCommand(TEXT("take_screenshot"), Params);
            }
            // Add blueprint commands
            else if (CommandType == TEXT("create_blueprint"))
            {
                ResultJson = HandleBlueprintCommand(TEXT("create_blueprint"), Params);
            }
            else if (CommandType == TEXT("add_component_to_blueprint"))
            {
                ResultJson = HandleBlueprintCommand(TEXT("add_component_to_blueprint"), Params);
            }
            else if (CommandType == TEXT("set_component_property"))
            {
                ResultJson = HandleBlueprintCommand(TEXT("set_component_property"), Params);
            }
            else if (CommandType == TEXT("set_physics_properties"))
            {
                ResultJson = HandleBlueprintCommand(TEXT("set_physics_properties"), Params);
            }
            else if (CommandType == TEXT("compile_blueprint"))
            {
                ResultJson = HandleBlueprintCommand(TEXT("compile_blueprint"), Params);
            }
            else if (CommandType == TEXT("spawn_blueprint_actor"))
            {
                ResultJson = HandleBlueprintCommand(TEXT("spawn_blueprint_actor"), Params);
            }
            // Add blueprint node commands
            else if (CommandType == TEXT("add_blueprint_event_node"))
            {
                ResultJson = HandleBlueprintCommand(TEXT("add_blueprint_event_node"), Params);
            }
            else if (CommandType == TEXT("add_blueprint_input_action_node"))
            {
                ResultJson = HandleBlueprintCommand(TEXT("add_blueprint_input_action_node"), Params);
            }
            else if (CommandType == TEXT("add_blueprint_function_node"))
            {
                ResultJson = HandleBlueprintCommand(TEXT("add_blueprint_function_node"), Params);
            }
            else if (CommandType == TEXT("add_blueprint_get_component_node"))
            {
                ResultJson = HandleBlueprintCommand(TEXT("add_blueprint_get_component_node"), Params);
            }
            else if (CommandType == TEXT("connect_blueprint_nodes"))
            {
                ResultJson = HandleBlueprintNodeCommand(TEXT("connect_blueprint_nodes"), Params);
            }
            else if (CommandType == TEXT("add_blueprint_variable"))
            {
                ResultJson = HandleBlueprintCommand(TEXT("add_blueprint_variable"), Params);
            }
            else if (CommandType == TEXT("create_input_mapping"))
            {
                ResultJson = HandleBlueprintNodeCommand(TEXT("create_input_mapping"), Params);
            }
            else if (CommandType == TEXT("add_blueprint_get_self_component_reference"))
            {
                ResultJson = HandleBlueprintNodeCommand(TEXT("add_blueprint_get_self_component_reference"), Params);
            }
            else if (CommandType == TEXT("set_blueprint_property"))
            {
                ResultJson = HandleSetBlueprintProperty(Params);
            }
            else if (CommandType == TEXT("add_blueprint_self_reference"))
            {
                ResultJson = HandleAddBlueprintSelfReference(Params);
            }
            else if (CommandType == TEXT("find_blueprint_nodes"))
            {
                ResultJson = HandleFindBlueprintNodes(Params);
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
    
    // Wait for the result
    return Future.Get();
}

// Handle actor-related commands
TSharedPtr<FJsonObject> UUnrealMCPBridge::HandleActorCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject);
    
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Handling actor command: %s"), *CommandType);
    
    // Get the EditorActorSubsystem once at the beginning
    UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    
    if (CommandType == TEXT("find_actors_by_name"))
    {
        FString Name;
        Params->TryGetStringField(TEXT("name"), Name);
        
        UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Searching for actors with name pattern: %s"), *Name);
        
        // Find actors matching the name
        TArray<AActor*> AllActors;
        if (EditorActorSubsystem)
        {
            AllActors = EditorActorSubsystem->GetAllLevelActors();
        }
        
        UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Found %d total actors in level"), AllActors.Num());
        
        TArray<TSharedPtr<FJsonValue>> ActorArray;
        for (AActor* Actor : AllActors)
        {
            if (Actor->GetActorLabel().Contains(Name))
            {
                ActorArray.Add(ActorToJson(Actor));
                UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Found matching actor: %s"), *Actor->GetActorLabel());
            }
        }
        
        ResultJson->SetArrayField(TEXT("actors"), ActorArray);
        UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Found %d matching actors"), ActorArray.Num());
    }
    else if (CommandType == TEXT("create_actor"))
    {
        FString Name, Type;
        
        // Properly extract string parameters
        Params->TryGetStringField(TEXT("name"), Name);
        Params->TryGetStringField(TEXT("type"), Type);
        
        // Get array fields using UE5.5 compatible method
        const TArray<TSharedPtr<FJsonValue>>* LocationArrayPtr = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* RotationArrayPtr = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* ScaleArrayPtr = nullptr;
        
        // Safe extraction of arrays using compatible methods
        bool bHasLocation = Params->HasField(TEXT("location")) && 
                           Params->TryGetArrayField(TEXT("location"), LocationArrayPtr);
                           
        bool bHasRotation = Params->HasField(TEXT("rotation")) && 
                           Params->TryGetArrayField(TEXT("rotation"), RotationArrayPtr);
                           
        bool bHasScale = Params->HasField(TEXT("scale")) && 
                        Params->TryGetArrayField(TEXT("scale"), ScaleArrayPtr);
        
        // Convert arrays to vectors
        FVector Location = FVector::ZeroVector;
        FRotator Rotation = FRotator::ZeroRotator;
        FVector Scale = FVector(1.0f, 1.0f, 1.0f);
        
        if (bHasLocation && LocationArrayPtr && LocationArrayPtr->Num() == 3)
        {
            Location.X = (*LocationArrayPtr)[0]->AsNumber();
            Location.Y = (*LocationArrayPtr)[1]->AsNumber();
            Location.Z = (*LocationArrayPtr)[2]->AsNumber();
        }
        
        if (bHasRotation && RotationArrayPtr && RotationArrayPtr->Num() == 3)
        {
            Rotation.Pitch = (*RotationArrayPtr)[0]->AsNumber();
            Rotation.Yaw = (*RotationArrayPtr)[1]->AsNumber();
            Rotation.Roll = (*RotationArrayPtr)[2]->AsNumber();
        }
        
        if (bHasScale && ScaleArrayPtr && ScaleArrayPtr->Num() == 3)
        {
            Scale.X = (*ScaleArrayPtr)[0]->AsNumber();
            Scale.Y = (*ScaleArrayPtr)[1]->AsNumber();
            Scale.Z = (*ScaleArrayPtr)[2]->AsNumber();
        }
        
        // Create actor based on type
        AActor* NewActor = nullptr;
        
        if (Type == TEXT("CUBE") || Type == TEXT("SPHERE") || Type == TEXT("PLANE") || Type == TEXT("CYLINDER") || Type == TEXT("CONE"))
        {
            // Create static mesh actor
            if (EditorActorSubsystem)
            {
                NewActor = EditorActorSubsystem->SpawnActorFromClass(AStaticMeshActor::StaticClass(), Location, Rotation, false);
            }
            
            if (NewActor)
            {
                // Set static mesh asset
                AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(NewActor);
                if (MeshActor && MeshActor->GetStaticMeshComponent())
                {
                    FString MeshPath;
                    if (Type == TEXT("CUBE")) MeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
                    else if (Type == TEXT("SPHERE")) MeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
                    else if (Type == TEXT("PLANE")) MeshPath = TEXT("/Engine/BasicShapes/Plane.Plane");
                    else if (Type == TEXT("CYLINDER")) MeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
                    else if (Type == TEXT("CONE")) MeshPath = TEXT("/Engine/BasicShapes/Cone.Cone");
                    
                    UStaticMesh* StaticMesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
                    if (StaticMesh)
                    {
                        MeshActor->GetStaticMeshComponent()->SetStaticMesh(StaticMesh);
                        MeshActor->GetStaticMeshComponent()->SetWorldScale3D(Scale);
                    }
                }
            }
        }
        else if (Type == TEXT("CAMERA"))
        {
            if (EditorActorSubsystem)
            {
                NewActor = EditorActorSubsystem->SpawnActorFromClass(ACameraActor::StaticClass(), Location, Rotation, false);
            }
            if (NewActor)
            {
                NewActor->SetActorScale3D(Scale);
            }
        }
        else if (Type == TEXT("LIGHT"))
        {
            if (EditorActorSubsystem)
            {
                NewActor = EditorActorSubsystem->SpawnActorFromClass(ADirectionalLight::StaticClass(), Location, Rotation, false);
            }
            if (NewActor)
            {
                NewActor->SetActorScale3D(Scale);
            }
        }
        else if (Type == TEXT("POINT_LIGHT"))
        {
            if (EditorActorSubsystem)
            {
                NewActor = EditorActorSubsystem->SpawnActorFromClass(APointLight::StaticClass(), Location, Rotation, false);
            }
            if (NewActor)
            {
                NewActor->SetActorScale3D(Scale);
            }
        }
        else if (Type == TEXT("SPOT_LIGHT"))
        {
            if (EditorActorSubsystem)
            {
                NewActor = EditorActorSubsystem->SpawnActorFromClass(ASpotLight::StaticClass(), Location, Rotation, false);
            }
            if (NewActor)
            {
                NewActor->SetActorScale3D(Scale);
            }
        }
        
        // Set actor label
        if (NewActor)
        {
            NewActor->SetActorLabel(Name);
            ResultJson = ActorToJsonObject(NewActor);
        }
        else
        {
            TSharedPtr<FJsonObject> ErrorResult = MakeShareable(new FJsonObject);
            ErrorResult->SetBoolField(TEXT("success"), false);
            ErrorResult->SetStringField(TEXT("message"), FString::Printf(TEXT("Failed to create actor of type %s"), *Type));
            ResultJson = ErrorResult;
        }
    }
    else if (CommandType == TEXT("delete_actor"))
    {
        FString Name;
        Params->TryGetStringField(TEXT("name"), Name);
        
        // Find actor with the given name
        TArray<AActor*> AllActors;
        if (EditorActorSubsystem)
        {
            AllActors = EditorActorSubsystem->GetAllLevelActors();
        }
        
        bool bActorFound = false;
        for (AActor* Actor : AllActors)
        {
            if (Actor->GetActorLabel() == Name)
            {
                // Use the existing EditorActorSubsystem variable from the outer scope
                EditorActorSubsystem->DestroyActor(Actor);
                bActorFound = true;
                break;
            }
        }
        
        ResultJson->SetBoolField(TEXT("success"), bActorFound);
        if (bActorFound)
        {
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Actor '%s' deleted"), *Name));
        }
        else
        {
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Actor '%s' not found"), *Name));
        }
    }
    else if (CommandType == TEXT("set_actor_transform"))
    {
        FString Name;
        Params->TryGetStringField(TEXT("name"), Name);
        
        // Get array fields using UE5.5 compatible method
        const TArray<TSharedPtr<FJsonValue>>* LocationArrayPtr = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* RotationArrayPtr = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* ScaleArrayPtr = nullptr;
        
        // Safe extraction of arrays using compatible methods
        bool bHasLocation = Params->HasField(TEXT("location")) && 
                           Params->TryGetArrayField(TEXT("location"), LocationArrayPtr);
                           
        bool bHasRotation = Params->HasField(TEXT("rotation")) && 
                           Params->TryGetArrayField(TEXT("rotation"), RotationArrayPtr);
                           
        bool bHasScale = Params->HasField(TEXT("scale")) && 
                        Params->TryGetArrayField(TEXT("scale"), ScaleArrayPtr);
        
        // Find actor with the given name
        TArray<AActor*> AllActors;
        if (EditorActorSubsystem)
        {
            AllActors = EditorActorSubsystem->GetAllLevelActors();
        }
        
        AActor* TargetActor = nullptr;
        for (AActor* Actor : AllActors)
        {
            if (Actor->GetActorLabel() == Name)
            {
                TargetActor = Actor;
                break;
            }
        }
        
        if (TargetActor)
        {
            // Set location if provided
            if (bHasLocation && LocationArrayPtr && LocationArrayPtr->Num() == 3)
            {
                FVector Location;
                Location.X = (*LocationArrayPtr)[0]->AsNumber();
                Location.Y = (*LocationArrayPtr)[1]->AsNumber();
                Location.Z = (*LocationArrayPtr)[2]->AsNumber();
                TargetActor->SetActorLocation(Location, false);
            }
            
            // Set rotation if provided
            if (bHasRotation && RotationArrayPtr && RotationArrayPtr->Num() == 3)
            {
                FRotator Rotation;
                Rotation.Pitch = (*RotationArrayPtr)[0]->AsNumber();
                Rotation.Yaw = (*RotationArrayPtr)[1]->AsNumber();
                Rotation.Roll = (*RotationArrayPtr)[2]->AsNumber();
                TargetActor->SetActorRotation(Rotation);
            }
            
            // Set scale if provided
            if (bHasScale && ScaleArrayPtr && ScaleArrayPtr->Num() == 3)
            {
                FVector Scale;
                Scale.X = (*ScaleArrayPtr)[0]->AsNumber();
                Scale.Y = (*ScaleArrayPtr)[1]->AsNumber();
                Scale.Z = (*ScaleArrayPtr)[2]->AsNumber();
                TargetActor->SetActorScale3D(Scale);
            }
            
            ResultJson = ActorToJsonObject(TargetActor);
        }
        else
        {
            TSharedPtr<FJsonObject> ErrorResult = MakeShareable(new FJsonObject);
            ErrorResult->SetBoolField(TEXT("success"), false);
            ErrorResult->SetStringField(TEXT("message"), FString::Printf(TEXT("Actor '%s' not found"), *Name));
            ResultJson = ErrorResult;
        }
    }
    else if (CommandType == TEXT("get_actor_properties"))
    {
        FString Name;
        Params->TryGetStringField(TEXT("name"), Name);
        
        // Find actor with the given name
        TArray<AActor*> AllActors;
        if (EditorActorSubsystem)
        {
            AllActors = EditorActorSubsystem->GetAllLevelActors();
        }
        
        AActor* TargetActor = nullptr;
        for (AActor* Actor : AllActors)
        {
            if (Actor->GetActorLabel() == Name)
            {
                TargetActor = Actor;
                break;
            }
        }
        
        if (TargetActor)
        {
            ResultJson = ActorToJsonObject(TargetActor, true);
        }
        else
        {
            TSharedPtr<FJsonObject> ErrorResult = MakeShareable(new FJsonObject);
            ErrorResult->SetBoolField(TEXT("success"), false);
            ErrorResult->SetStringField(TEXT("message"), FString::Printf(TEXT("Actor '%s' not found"), *Name));
            ResultJson = ErrorResult;
        }
    }
    else if (CommandType == TEXT("get_actors_in_level"))
    {
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

        TArray<TSharedPtr<FJsonValue>> ActorsArray;
        for (AActor* Actor : AllActors)
        {
            TSharedPtr<FJsonObject> ActorJson = MakeShared<FJsonObject>();
            ActorJson->SetStringField(TEXT("name"), *Actor->GetName());
            ActorJson->SetStringField(TEXT("path"), *Actor->GetPathName());
            ActorJson->SetStringField(TEXT("type"), *Actor->GetClass()->GetName());

            FVector Location = Actor->GetActorLocation();
            FRotator Rotation = Actor->GetActorRotation();
            FVector Scale = Actor->GetActorScale3D();

            TArray<TSharedPtr<FJsonValue>> LocationArray;
            LocationArray.Add(MakeShared<FJsonValueNumber>(Location.X));
            LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
            LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
            ActorJson->SetArrayField(TEXT("location"), LocationArray);

            TArray<TSharedPtr<FJsonValue>> RotationArray;
            RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
            RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
            RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
            ActorJson->SetArrayField(TEXT("rotation"), RotationArray);

            TArray<TSharedPtr<FJsonValue>> ScaleArray;
            ScaleArray.Add(MakeShareable(new FJsonValueNumber(Scale.X)));
            ScaleArray.Add(MakeShareable(new FJsonValueNumber(Scale.Y)));
            ScaleArray.Add(MakeShareable(new FJsonValueNumber(Scale.Z)));
            ActorJson->SetArrayField(TEXT("scale"), ScaleArray);

            ActorsArray.Add(MakeShared<FJsonValueObject>(ActorJson));
        }

        // Create the result object with a simpler structure
        ResultJson = MakeShared<FJsonObject>();
        ResultJson->SetArrayField(TEXT("content"), ActorsArray);
    }
    
    return ResultJson;
}

// Handle editor-related commands
TSharedPtr<FJsonObject> UUnrealMCPBridge::HandleEditorCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject);
    
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Handling editor command: %s"), *CommandType);
    
    // Get the EditorActorSubsystem once at the beginning
    UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    
    if (CommandType == TEXT("focus_viewport"))
    {
        FString Target;
        Params->TryGetStringField(TEXT("target"), Target);
        
        // Get array fields for location and orientation
        const TArray<TSharedPtr<FJsonValue>>* LocationArrayPtr = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* OrientationArrayPtr = nullptr;
        
        bool bHasLocation = Params->HasField(TEXT("location")) && 
                           Params->TryGetArrayField(TEXT("location"), LocationArrayPtr);
                           
        bool bHasOrientation = Params->HasField(TEXT("orientation")) && 
                             Params->TryGetArrayField(TEXT("orientation"), OrientationArrayPtr);
        
        // Get distance parameter
        float Distance = 1000.0f;
        if (Params->HasField(TEXT("distance")))
        {
            Distance = (float)Params->GetNumberField(TEXT("distance"));
        }
        
        // Convert location array to vector
        FVector Location = FVector::ZeroVector;
        if (bHasLocation && LocationArrayPtr && LocationArrayPtr->Num() == 3)
        {
            Location.X = (*LocationArrayPtr)[0]->AsNumber();
            Location.Y = (*LocationArrayPtr)[1]->AsNumber();
            Location.Z = (*LocationArrayPtr)[2]->AsNumber();
        }
        
        // Convert orientation array to rotator
        FRotator Orientation = FRotator::ZeroRotator;
        if (bHasOrientation && OrientationArrayPtr && OrientationArrayPtr->Num() == 3)
        {
            Orientation.Pitch = (*OrientationArrayPtr)[0]->AsNumber();
            Orientation.Yaw = (*OrientationArrayPtr)[1]->AsNumber();
            Orientation.Roll = (*OrientationArrayPtr)[2]->AsNumber();
        }
        
        // Focus on target actor if specified
        if (!Target.IsEmpty())
        {
            // Find actor with the given name
            TArray<AActor*> AllActors;
            if (EditorActorSubsystem)
            {
                AllActors = EditorActorSubsystem->GetAllLevelActors();
            }
            
            AActor* TargetActor = nullptr;
            for (AActor* Actor : AllActors)
            {
                if (Actor->GetActorLabel() == Target)
                {
                    TargetActor = Actor;
                    break;
                }
            }
            
            if (TargetActor)
            {
                // Get the editor viewport client
                if (GEditor && GEditor->GetActiveViewport())
                {
                    FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
                    if (ViewportClient)
                    {
                        // Focus on the target actor
                        ViewportClient->FocusViewportOnBox(TargetActor->GetComponentsBoundingBox(), true);
                        
                        ResultJson->SetBoolField(TEXT("success"), true);
                        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Viewport focused on actor '%s'"), *Target));
                    }
                    else
                    {
                        ResultJson->SetBoolField(TEXT("success"), false);
                        ResultJson->SetStringField(TEXT("message"), TEXT("Failed to get viewport client"));
                    }
                }
                else
                {
                    ResultJson->SetBoolField(TEXT("success"), false);
                    ResultJson->SetStringField(TEXT("message"), TEXT("Failed to get active viewport"));
                }
            }
            else
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Actor '%s' not found"), *Target));
            }
        }
        // Focus on location if no target actor but location is provided
        else if (bHasLocation)
        {
            // Get the editor viewport client
            if (GEditor && GEditor->GetActiveViewport())
            {
                FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
                if (ViewportClient)
                {
                    // Create a box at the specified location
                    FBox FocusBox(Location - FVector(50.0f), Location + FVector(50.0f));
                    
                    // Focus on the box
                    ViewportClient->FocusViewportOnBox(FocusBox, true);
                    
                    // Set custom view location and rotation if orientation is provided
                    if (bHasOrientation)
                    {
                        ViewportClient->SetViewLocation(Location - Orientation.Vector() * Distance);
                        ViewportClient->SetViewRotation(Orientation);
                    }
                    
                    ResultJson->SetBoolField(TEXT("success"), true);
                    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Viewport focused on location [%f, %f, %f]"), Location.X, Location.Y, Location.Z));
                }
                else
                {
                    ResultJson->SetBoolField(TEXT("success"), false);
                    ResultJson->SetStringField(TEXT("message"), TEXT("Failed to get viewport client"));
                }
            }
            else
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("message"), TEXT("Failed to get active viewport"));
            }
        }
        else
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), TEXT("No target actor or location specified"));
        }
    }
    else if (CommandType == TEXT("take_screenshot"))
    {
        FString Filename;
        Params->TryGetStringField(TEXT("filename"), Filename);
        
        // Default to screenshot.png if no filename provided
        if (Filename.IsEmpty())
        {
            Filename = TEXT("screenshot.png");
        }
        
        // Get show_ui parameter
        bool bShowUI = false;
        if (Params->HasField(TEXT("show_ui")))
        {
            bShowUI = Params->GetBoolField(TEXT("show_ui"));
        }
        
        // Get resolution parameter
        const TArray<TSharedPtr<FJsonValue>>* ResolutionArrayPtr = nullptr;
        bool bHasResolution = Params->HasField(TEXT("resolution")) && 
                             Params->TryGetArrayField(TEXT("resolution"), ResolutionArrayPtr);
        
        int32 Width = 1920;
        int32 Height = 1080;
        if (bHasResolution && ResolutionArrayPtr && ResolutionArrayPtr->Num() == 2)
        {
            Width = (int32)(*ResolutionArrayPtr)[0]->AsNumber();
            Height = (int32)(*ResolutionArrayPtr)[1]->AsNumber();
        }
        
        // Take the screenshot
        if (GEngine && GEngine->GameViewport)
        {
            FString ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("Screenshots") / Filename;
            FScreenshotRequest::RequestScreenshot(ScreenshotPath, bShowUI, false);
            
            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Screenshot saved to '%s'"), *ScreenshotPath));
            ResultJson->SetStringField(TEXT("path"), ScreenshotPath);
        }
        else
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), TEXT("Failed to take screenshot: no viewport"));
        }
    }
    else
    {
        ResultJson->SetBoolField(TEXT("success"), false);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Unknown editor command: %s"), *CommandType));
    }
    
    return ResultJson;
}

// Handle blueprint-related commands
TSharedPtr<FJsonObject> UUnrealMCPBridge::HandleBlueprintCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject);
    
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Handling blueprint command: %s"), *CommandType);
    
    // Get the EditorActorSubsystem once at the beginning
    UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    
    if (CommandType == TEXT("create_blueprint"))
    {
        FString Name, ParentClass;
        Params->TryGetStringField(TEXT("name"), Name);
        Params->TryGetStringField(TEXT("parent_class"), ParentClass);
        
        UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Creating blueprint '%s' with parent class '%s'"), *Name, *ParentClass);
        
        // Create the Blueprint asset
        UBlueprint* NewBlueprint = nullptr;
        
        // Check if the blueprint already exists
        FString BlueprintPath = TEXT("/Game/Blueprints/") + Name;
        UBlueprint* ExistingBlueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        
        if (ExistingBlueprint)
        {
            // Blueprint already exists, return it as a success
            UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Blueprint '%s' already exists, returning existing blueprint"), *Name);
            NewBlueprint = ExistingBlueprint;
            
            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetStringField(TEXT("blueprint_name"), Name);
            ResultJson->SetStringField(TEXT("path"), NewBlueprint->GetPathName());
            ResultJson->SetBoolField(TEXT("already_exists"), true);
            
            return ResultJson;
        }
        
        // Find the parent class - using StaticFindFirstObject to search all packages
        UClass* ParentClassObj = nullptr;
        
        // Try different paths to find the class
        // First try direct lookup
        ParentClassObj = FindObject<UClass>(ANY_PACKAGE, *ParentClass);
        
        // If not found, try with engine path prefix
        if (!ParentClassObj)
        {
            FString EngineClassName = FString::Printf(TEXT("/Script/Engine.%s"), *ParentClass);
            ParentClassObj = FindObject<UClass>(ANY_PACKAGE, *EngineClassName);
        }
        
        // Finally try with _C suffix for Blueprint classes
        if (!ParentClassObj)
        {
            ParentClassObj = FindObject<UClass>(ANY_PACKAGE, *(ParentClass + TEXT("_C")));
        }
        
        if (ParentClassObj)
        {
            // Create Blueprint factory
            UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
            Factory->ParentClass = ParentClassObj;
            
            // Create package for new blueprint
            FString PackagePath = TEXT("/Game/Blueprints/");
            UPackage* Package = CreatePackage(*(PackagePath + Name));
            
            // Create the blueprint
            NewBlueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(
                UBlueprint::StaticClass(),
                Package,
                *Name,
                RF_Public | RF_Standalone | RF_Transactional,
                nullptr,
                GWarn
            ));
            
            if (NewBlueprint)
            {
                // Notify asset registry
                FAssetRegistryModule::AssetCreated(NewBlueprint);
                
                // Mark package dirty
                Package->MarkPackageDirty();
                
                ResultJson->SetBoolField(TEXT("success"), true);
                ResultJson->SetStringField(TEXT("blueprint_name"), Name);
                ResultJson->SetStringField(TEXT("path"), NewBlueprint->GetPathName());
            }
            else
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("message"), TEXT("Failed to create blueprint"));
            }
        }
        else
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Parent class '%s' not found"), *ParentClass));
        }
    }
    else if (CommandType == TEXT("add_component_to_blueprint"))
    {
        FString BlueprintName, ComponentType, ComponentName;
        Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName);
        Params->TryGetStringField(TEXT("component_type"), ComponentType);
        Params->TryGetStringField(TEXT("component_name"), ComponentName);
        
        // Get location, rotation, and scale parameters
        const TArray<TSharedPtr<FJsonValue>>* LocationArrayPtr = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* RotationArrayPtr = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* ScaleArrayPtr = nullptr;
        
        bool bHasLocation = Params->HasField(TEXT("location")) && 
                         Params->TryGetArrayField(TEXT("location"), LocationArrayPtr);
                         
        bool bHasRotation = Params->HasField(TEXT("rotation")) && 
                         Params->TryGetArrayField(TEXT("rotation"), RotationArrayPtr);
                         
        bool bHasScale = Params->HasField(TEXT("scale")) && 
                      Params->TryGetArrayField(TEXT("scale"), ScaleArrayPtr);
        
        // Convert arrays to vectors
        FVector Location = FVector::ZeroVector;
        FRotator Rotation = FRotator::ZeroRotator;
        FVector Scale = FVector(1.0f, 1.0f, 1.0f);
        
        if (bHasLocation && LocationArrayPtr && LocationArrayPtr->Num() == 3)
        {
            Location.X = (*LocationArrayPtr)[0]->AsNumber();
            Location.Y = (*LocationArrayPtr)[1]->AsNumber();
            Location.Z = (*LocationArrayPtr)[2]->AsNumber();
        }
        
        if (bHasRotation && RotationArrayPtr && RotationArrayPtr->Num() == 3)
        {
            Rotation.Pitch = (*RotationArrayPtr)[0]->AsNumber();
            Rotation.Yaw = (*RotationArrayPtr)[1]->AsNumber();
            Rotation.Roll = (*RotationArrayPtr)[2]->AsNumber();
        }
        
        if (bHasScale && ScaleArrayPtr && ScaleArrayPtr->Num() == 3)
        {
            Scale.X = (*ScaleArrayPtr)[0]->AsNumber();
            Scale.Y = (*ScaleArrayPtr)[1]->AsNumber();
            Scale.Z = (*ScaleArrayPtr)[2]->AsNumber();
        }
        
        // Find the blueprint asset
        FString BlueprintPath = TEXT("/Game/Blueprints/") + BlueprintName;
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        
        if (!Blueprint)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
            return ResultJson;
        }
        
        // Add the component
        USCS_Node* NewNode = nullptr;
        
        // Determine component class based on component type
        UClass* ComponentClass = nullptr;
        
        if (ComponentType == TEXT("StaticMesh"))
        {
            ComponentClass = UStaticMeshComponent::StaticClass();
        }
        else if (ComponentType == TEXT("BoxCollision"))
        {
            ComponentClass = UBoxComponent::StaticClass();
        }
        else if (ComponentType == TEXT("SphereCollision"))
        {
            ComponentClass = USphereComponent::StaticClass();
        }
        else
        {
            // Try to find the class directly
            FString ClassName = ComponentType;
            if (!ClassName.EndsWith(TEXT("Component")))
            {
                ClassName += TEXT("Component");
            }
            
            // Try different approaches to find the component class
            ComponentClass = FindObject<UClass>(ANY_PACKAGE, *ClassName);
            
            // If not found, try with engine path prefix
            if (!ComponentClass)
            {
                FString EngineClassName = FString::Printf(TEXT("/Script/Engine.%s"), *ClassName);
                ComponentClass = FindObject<UClass>(ANY_PACKAGE, *EngineClassName);
            }
            
            if (!ComponentClass)
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Component type '%s' not found"), *ComponentType));
                return ResultJson;
            }
        }
        
        if (ComponentClass)
        {
            // Create new component
            UActorComponent* NewComponent = NewObject<UActorComponent>(
                Blueprint->GeneratedClass->GetDefaultObject(),
                ComponentClass,
                *ComponentName,
                RF_Public
            );
            
            if (NewComponent)
            {
                // Set component transform
                USceneComponent* SceneComponent = Cast<USceneComponent>(NewComponent);
                if (SceneComponent)
                {
                    SceneComponent->SetRelativeLocation(Location);
                    SceneComponent->SetRelativeRotation(Rotation);
                    SceneComponent->SetRelativeScale3D(Scale);
                }
                
                // Add component to blueprint
                if (Blueprint->SimpleConstructionScript)
                {
                    NewNode = Blueprint->SimpleConstructionScript->CreateNodeAndRenameComponent(NewComponent);
                    
                    if (NewNode)
                    {
                        // Set the variable name explicitly to match what was requested
                        NewNode->SetVariableName(FName(*ComponentName));
                        
                        Blueprint->SimpleConstructionScript->AddNode(NewNode);
                        
                        // If this is a static mesh component, set default mesh
                        UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(NewComponent);
                        if (StaticMeshComp && ComponentType == TEXT("StaticMesh"))
                        {
                            UStaticMesh* DefaultCube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
                            if (DefaultCube)
                            {
                                StaticMeshComp->SetStaticMesh(DefaultCube);
                            }
                        }
                        
                        // Mark the blueprint as modified
                        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
                        
                        // Compile the blueprint
                        FKismetEditorUtilities::CompileBlueprint(Blueprint);
                        
                        ResultJson->SetBoolField(TEXT("success"), true);
                        ResultJson->SetStringField(TEXT("component_name"), ComponentName);
                        ResultJson->SetStringField(TEXT("blueprint_name"), BlueprintName);
                    }
                    else
                    {
                        ResultJson->SetBoolField(TEXT("success"), false);
                        ResultJson->SetStringField(TEXT("message"), TEXT("Failed to create component node"));
                    }
                }
            }
            else
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("message"), TEXT("Failed to create component object"));
            }
        }
    }
    else if (CommandType == TEXT("set_component_property"))
    {
        FString BlueprintName, ComponentName, PropertyName;
        Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName);
        Params->TryGetStringField(TEXT("component_name"), ComponentName);
        Params->TryGetStringField(TEXT("property_name"), PropertyName);
        
        // Get property value (could be various types)
        TSharedPtr<FJsonValue> PropertyValue = Params->GetField<EJson::None>(TEXT("property_value"));
        
        // Find the blueprint asset
        FString BlueprintPath = TEXT("/Game/Blueprints/") + BlueprintName;
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        
        if (!Blueprint)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
            return ResultJson;
        }
        
        // Find the component in the blueprint
        USCS_Node* TargetNode = nullptr;
        
        if (Blueprint->SimpleConstructionScript)
        {
            const TArray<USCS_Node*>& AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
            for (USCS_Node* Node : AllNodes)
            {
                if (Node->GetVariableName() == *ComponentName)
                {
                    TargetNode = Node;
                    break;
                }
            }
        }
        
        if (TargetNode && TargetNode->ComponentTemplate)
        {
            // Set the property on the component template
            UActorComponent* Component = TargetNode->ComponentTemplate;
            
            // Find the property
            FProperty* Property = Component->GetClass()->FindPropertyByName(*PropertyName);
            
            if (Property)
            {
                // Set property value based on its type
                void* PropertyPtr = Property->ContainerPtrToValuePtr<void>(Component);
                
                if (PropertyValue->Type == EJson::Boolean)
                {
                    bool BoolValue = PropertyValue->AsBool();
                    FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property);
                    if (BoolProperty)
                    {
                        BoolProperty->SetPropertyValue(PropertyPtr, BoolValue);
                    }
                }
                else if (PropertyValue->Type == EJson::Number)
                {
                    float NumValue = PropertyValue->AsNumber();
                    FNumericProperty* NumProperty = CastField<FNumericProperty>(Property);
                    if (NumProperty)
                    {
                        NumProperty->SetFloatingPointPropertyValue(PropertyPtr, NumValue);
                    }
                }
                else if (PropertyValue->Type == EJson::String)
                {
                    FString StringValue = PropertyValue->AsString();
                    FStrProperty* StrProperty = CastField<FStrProperty>(Property);
                    if (StrProperty)
                    {
                        StrProperty->SetPropertyValue(PropertyPtr, StringValue);
                    }
                }
                
                // Mark the blueprint as modified
                FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
                
                ResultJson->SetBoolField(TEXT("success"), true);
                ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Set property '%s' on component '%s'"), *PropertyName, *ComponentName));
            }
            else
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Property '%s' not found on component '%s'"), *PropertyName, *ComponentName));
            }
        }
        else
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Component '%s' not found in blueprint '%s'"), *ComponentName, *BlueprintName));
        }
    }
    else if (CommandType == TEXT("set_physics_properties"))
    {
        FString BlueprintName, ComponentName;
        Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName);
        Params->TryGetStringField(TEXT("component_name"), ComponentName);
        
        bool SimulatePhysics = true;
        bool GravityEnabled = true;
        float Mass = 1.0f;
        float LinearDamping = 0.01f;
        float AngularDamping = 0.0f;
        
        if (Params->HasField(TEXT("simulate_physics")))
        {
            SimulatePhysics = Params->GetBoolField(TEXT("simulate_physics"));
        }
        
        if (Params->HasField(TEXT("gravity_enabled")))
        {
            GravityEnabled = Params->GetBoolField(TEXT("gravity_enabled"));
        }
        
        if (Params->HasField(TEXT("mass")))
        {
            Mass = Params->GetNumberField(TEXT("mass"));
        }
        
        if (Params->HasField(TEXT("linear_damping")))
        {
            LinearDamping = Params->GetNumberField(TEXT("linear_damping"));
        }
        
        if (Params->HasField(TEXT("angular_damping")))
        {
            AngularDamping = Params->GetNumberField(TEXT("angular_damping"));
        }
        
        // Find the blueprint asset
        FString BlueprintPath = TEXT("/Game/Blueprints/") + BlueprintName;
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        
        if (!Blueprint)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
            return ResultJson;
        }
        
        // Find the component in the blueprint
        USCS_Node* TargetNode = nullptr;
        
        if (Blueprint->SimpleConstructionScript)
        {
            const TArray<USCS_Node*>& AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
            for (USCS_Node* Node : AllNodes)
            {
                if (Node->GetVariableName() == *ComponentName)
                {
                    TargetNode = Node;
                    break;
                }
            }
        }
        
        if (TargetNode && TargetNode->ComponentTemplate)
        {
            // Set physics properties on the component template
            UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(TargetNode->ComponentTemplate);
            
            if (PrimComp)
            {
                // Set physics properties
                PrimComp->SetSimulatePhysics(SimulatePhysics);
                PrimComp->SetEnableGravity(GravityEnabled);
                PrimComp->SetMassOverrideInKg(NAME_None, Mass);
                PrimComp->SetLinearDamping(LinearDamping);
                PrimComp->SetAngularDamping(AngularDamping);
                
                // Set the collision profile to PhysicsActor if simulating physics
                if (SimulatePhysics)
                {
                    PrimComp->SetCollisionProfileName(TEXT("PhysicsActor"));
                }
                
                // Mark the blueprint as modified
                FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
                
                // Compile the blueprint
                FKismetEditorUtilities::CompileBlueprint(Blueprint);
                
                ResultJson->SetBoolField(TEXT("success"), true);
                ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Set physics properties on component '%s'"), *ComponentName));
            }
            else
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Component '%s' is not a primitive component"), *ComponentName));
            }
        }
        else
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Component '%s' not found in blueprint '%s'"), *ComponentName, *BlueprintName));
        }
    }
    else if (CommandType == TEXT("compile_blueprint"))
    {
        FString BlueprintName;
        Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName);
        
        // Find the blueprint asset
        FString BlueprintPath = TEXT("/Game/Blueprints/") + BlueprintName;
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        
        if (!Blueprint)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
            return ResultJson;
        }
        
        // Compile the blueprint
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Blueprint '%s' compiled successfully"), *BlueprintName));
    }
    else if (CommandType == TEXT("spawn_blueprint_actor"))
    {
        FString BlueprintName, ActorName;
        Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName);
        Params->TryGetStringField(TEXT("actor_name"), ActorName);
        
        // Get location, rotation, and scale parameters
        const TArray<TSharedPtr<FJsonValue>>* LocationArrayPtr = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* RotationArrayPtr = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* ScaleArrayPtr = nullptr;
        
        bool bHasLocation = Params->HasField(TEXT("location")) && 
                         Params->TryGetArrayField(TEXT("location"), LocationArrayPtr);
                         
        bool bHasRotation = Params->HasField(TEXT("rotation")) && 
                         Params->TryGetArrayField(TEXT("rotation"), RotationArrayPtr);
                         
        bool bHasScale = Params->HasField(TEXT("scale")) && 
                      Params->TryGetArrayField(TEXT("scale"), ScaleArrayPtr);
        
        // Convert arrays to vectors
        FVector Location = FVector::ZeroVector;
        FRotator Rotation = FRotator::ZeroRotator;
        FVector Scale = FVector(1.0f, 1.0f, 1.0f);
        
        if (bHasLocation && LocationArrayPtr && LocationArrayPtr->Num() == 3)
        {
            Location.X = (*LocationArrayPtr)[0]->AsNumber();
            Location.Y = (*LocationArrayPtr)[1]->AsNumber();
            Location.Z = (*LocationArrayPtr)[2]->AsNumber();
        }
        
        if (bHasRotation && RotationArrayPtr && RotationArrayPtr->Num() == 3)
        {
            Rotation.Pitch = (*RotationArrayPtr)[0]->AsNumber();
            Rotation.Yaw = (*RotationArrayPtr)[1]->AsNumber();
            Rotation.Roll = (*RotationArrayPtr)[2]->AsNumber();
        }
        
        if (bHasScale && ScaleArrayPtr && ScaleArrayPtr->Num() == 3)
        {
            Scale.X = (*ScaleArrayPtr)[0]->AsNumber();
            Scale.Y = (*ScaleArrayPtr)[1]->AsNumber();
            Scale.Z = (*ScaleArrayPtr)[2]->AsNumber();
        }
        
        // Find the blueprint asset
        FString BlueprintPath = TEXT("/Game/Blueprints/") + BlueprintName;
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        
        if (!Blueprint)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
            return ResultJson;
        }
        
        // Spawn the actor in the editor world
        UClass* GeneratedClass = Blueprint->GeneratedClass;
        if (GeneratedClass && GeneratedClass->IsChildOf(AActor::StaticClass()))
        {
            TSubclassOf<AActor> ActorClass = TSubclassOf<AActor>(GeneratedClass);
            AActor* SpawnedActor = nullptr;
            if (EditorActorSubsystem)
            {
                SpawnedActor = EditorActorSubsystem->SpawnActorFromClass(
                    ActorClass,
                    Location,
                    Rotation
                );
            }
            
            if (SpawnedActor)
            {
                // Set actor label and scale
                SpawnedActor->SetActorLabel(ActorName);
                SpawnedActor->SetActorScale3D(Scale);
                
                ResultJson->SetBoolField(TEXT("success"), true);
                ResultJson->SetStringField(TEXT("actor_name"), ActorName);
                ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Actor '%s' spawned from blueprint '%s'"), *ActorName, *BlueprintName));
            }
            else
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Failed to spawn actor from blueprint '%s'"), *BlueprintName));
            }
        }
        else
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Blueprint '%s' does not generate an Actor class"), *BlueprintName));
        }
    }
    else if (CommandType == TEXT("add_blueprint_event_node"))
    {
        FString BlueprintName, EventType;
        Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName);
        Params->TryGetStringField(TEXT("event_type"), EventType);
        
        // Get node position if provided
        FVector2D NodePosition(0, 0);
        const TArray<TSharedPtr<FJsonValue>>* NodePosArrayPtr = nullptr;
        if (Params->HasField(TEXT("node_position")) && Params->TryGetArrayField(TEXT("node_position"), NodePosArrayPtr))
        {
            if (NodePosArrayPtr && NodePosArrayPtr->Num() == 2)
            {
                NodePosition.X = (*NodePosArrayPtr)[0]->AsNumber();
                NodePosition.Y = (*NodePosArrayPtr)[1]->AsNumber();
            }
        }
        
        // Find the blueprint asset
        FString BlueprintPath = TEXT("/Game/Blueprints/") + BlueprintName;
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        
        if (!Blueprint)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
            return ResultJson;
        }
        
        // Get event graph
        UEdGraph* EventGraph = nullptr;
        
        for (UEdGraph* Graph : Blueprint->UbergraphPages)
        {
            if (Graph->GetName().Contains(TEXT("EventGraph")))
            {
                EventGraph = Graph;
                break;
            }
        }
        
        if (!EventGraph)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), TEXT("Event graph not found in blueprint"));
            return ResultJson;
        }
        
        // We want to add an event node
        UEdGraphNode* EventNode = nullptr;
        
        if (EventType.Equals(TEXT("BeginPlay"), ESearchCase::IgnoreCase))
        {
            UK2Node_Event* BeginPlayNode = NewObject<UK2Node_Event>(EventGraph);
            
            UClass* BPClass = Blueprint->GeneratedClass;
            if (!BPClass)
                BPClass = Blueprint->ParentClass;
                
            UFunction* Function = BPClass->FindFunctionByName(FName(TEXT("ReceiveBeginPlay")));
            
            if (Function)
            {
                BeginPlayNode->EventReference.SetExternalMember(FName(TEXT("ReceiveBeginPlay")), BPClass);
                BeginPlayNode->bOverrideFunction = true;
                
                BeginPlayNode->NodePosX = NodePosition.X;
                BeginPlayNode->NodePosY = NodePosition.Y;
                
                EventGraph->AddNode(BeginPlayNode);
                BeginPlayNode->CreateNewGuid();
                BeginPlayNode->PostPlacedNewNode();
                BeginPlayNode->AllocateDefaultPins();
                
                FString NodeID = BeginPlayNode->NodeGuid.ToString();
                EventNode = BeginPlayNode;
                
                ResultJson->SetBoolField(TEXT("success"), true);
                ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Added BeginPlay event to blueprint '%s'"), *BlueprintName));
                ResultJson->SetStringField(TEXT("node_id"), *NodeID);
            }
            else
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("message"), TEXT("Could not find ReceiveBeginPlay function"));
                return ResultJson;
            }
        }
        else if (EventType.Equals(TEXT("Tick"), ESearchCase::IgnoreCase))
        {
            // Similar implementation for Tick event...
        }
        else if (EventType.Equals(TEXT("InputAction"), ESearchCase::IgnoreCase))
        {
            // We don't handle InputAction events here - needs special handling
            UE_LOG(LogTemp, Warning, TEXT("InputAction events should be created using CreateInputActionNode function"));
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), TEXT("Failed to create InputAction event node"));
            return ResultJson;
        }
        else
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Unsupported event type: %s"), *EventType));
            return ResultJson;
        }
        
        // Notify blueprint that it has changed
        Blueprint->MarkPackageDirty();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    }
    else if (CommandType == TEXT("add_blueprint_input_action_node"))
    {
        FString BlueprintName, ActionName;
        Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName);
        Params->TryGetStringField(TEXT("action_name"), ActionName);
        
        // Get node position if provided
        FVector2D NodePosition(0, 0);
        const TArray<TSharedPtr<FJsonValue>>* NodePosArrayPtr = nullptr;
        if (Params->HasField(TEXT("node_position")) && Params->TryGetArrayField(TEXT("node_position"), NodePosArrayPtr))
        {
            if (NodePosArrayPtr && NodePosArrayPtr->Num() == 2)
        {
            NodePosition.X = (*NodePosArrayPtr)[0]->AsNumber();
            NodePosition.Y = (*NodePosArrayPtr)[1]->AsNumber();
            }
        }
        
        // Find the blueprint asset
        FString BlueprintPath = TEXT("/Game/Blueprints/") + BlueprintName;
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        
        if (!Blueprint)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
            return ResultJson;
        }
        
        // Get event graph
        UEdGraph* EventGraph = nullptr;
        
        for (UEdGraph* Graph : Blueprint->UbergraphPages)
        {
            if (Graph->GetName().Contains(TEXT("EventGraph")))
            {
                EventGraph = Graph;
                break;
            }
        }
        
        if (!EventGraph)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), TEXT("Event graph not found in blueprint"));
            return ResultJson;
        }
        
        // Create the input action node
        UK2Node_InputAction* InputActionNode = CreateInputActionNode(EventGraph, ActionName, NodePosition);
        
        if (InputActionNode)
        {
            FString NodeID = InputActionNode->NodeGuid.ToString();
            
            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Added InputAction '%s' event to blueprint '%s'"), *ActionName, *BlueprintName));
            ResultJson->SetStringField(TEXT("node_id"), *NodeID);
            
            // Notify blueprint that it has changed
            Blueprint->MarkPackageDirty();
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        }
        else
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), TEXT("Failed to create InputAction node"));
        }
    }
    else if (CommandType == TEXT("add_blueprint_function_node"))
    {
        FString BlueprintName, Target, FunctionName;
        Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName);
        Params->TryGetStringField(TEXT("target"), Target);
        Params->TryGetStringField(TEXT("function_name"), FunctionName);
        
        // Get node position
        const TArray<TSharedPtr<FJsonValue>>* NodePosArrayPtr = nullptr;
        FVector2D NodePosition(0.0f, 0.0f);
        
        if (Params->TryGetArrayField(TEXT("node_position"), NodePosArrayPtr) && 
            NodePosArrayPtr && NodePosArrayPtr->Num() >= 2)
        {
            NodePosition.X = (*NodePosArrayPtr)[0]->AsNumber();
            NodePosition.Y = (*NodePosArrayPtr)[1]->AsNumber();
        }
        
        // Get function parameters
        const TSharedPtr<FJsonObject>* FunctionParams = nullptr;
        if (!Params->TryGetObjectField(TEXT("params"), FunctionParams))
        {
            FunctionParams = nullptr; // Set to nullptr if not found
        }
        
        // Find the blueprint
        UBlueprint* Blueprint = FindBlueprint(BlueprintName);
        
        if (!Blueprint)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
            return ResultJson;
        }
        
        // Get or create event graph
        UEdGraph* EventGraph = FindOrCreateEventGraph(Blueprint);
        if (!EventGraph)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), TEXT("Failed to get or create event graph"));
            return ResultJson;
        }
        
        // Find the target class and function
        UClass* TargetClass = nullptr;
        UFunction* Function = nullptr;
        
        UE_LOG(LogTemp, Display, TEXT("Searching for function '%s' with target '%s'"), *FunctionName, *Target);
        
        if (Target.Equals(TEXT("self"), ESearchCase::IgnoreCase))
        {
            // Target is the blueprint's class
            TargetClass = Blueprint->GeneratedClass;
            UE_LOG(LogTemp, Display, TEXT("Target is 'self', using Blueprint's generated class: %s"), *TargetClass->GetName());
        }
        else if (Target.Equals(TEXT("PrimitiveComponent"), ESearchCase::IgnoreCase))
        {
            // Use PrimitiveComponent class directly
            TargetClass = UPrimitiveComponent::StaticClass();
            UE_LOG(LogTemp, Display, TEXT("Using UPrimitiveComponent class directly as target"));
        }
        // Handle utility class targets like UGameplayStatics
        else if (Target.Equals(TEXT("GameplayStatics"), ESearchCase::IgnoreCase) ||
                 Target.Equals(TEXT("UGameplayStatics"), ESearchCase::IgnoreCase))
        {
            // Use GameplayStatics class directly
            TargetClass = UGameplayStatics::StaticClass();
            UE_LOG(LogTemp, Display, TEXT("Using UGameplayStatics class directly for target"));
        }
        else if (Target.Equals(TEXT("PlayerController"), ESearchCase::IgnoreCase) ||
                 Target.Equals(TEXT("UPlayerController"), ESearchCase::IgnoreCase) ||
                 Target.Equals(TEXT("APlayerController"), ESearchCase::IgnoreCase))
        {
            // Use PlayerController class directly
            TargetClass = APlayerController::StaticClass();
            UE_LOG(LogTemp, Display, TEXT("Using APlayerController class directly for target"));
        }
        // Try to find class by name if target starts with 'U' or 'A' (Unreal naming convention)
        else if (Target.StartsWith(TEXT("U")) || Target.StartsWith(TEXT("A")))
        {
            // Try direct lookup
            TargetClass = FindObject<UClass>(ANY_PACKAGE, *Target);
            
            // If not found, try with engine path prefix
            if (!TargetClass)
            {
                FString EngineClassName = FString::Printf(TEXT("/Script/Engine.%s"), *Target);
                TargetClass = FindObject<UClass>(ANY_PACKAGE, *EngineClassName);
            }
            
            if (TargetClass)
            {
                UE_LOG(LogTemp, Display, TEXT("Found class '%s' for target '%s'"), *TargetClass->GetName(), *Target);
            }
        }
        else
        {
            // Target is a component in the blueprint
            if (Blueprint->SimpleConstructionScript)
            {
                UE_LOG(LogTemp, Display, TEXT("Searching for component named '%s' in SimpleConstructionScript"), *Target);
                const TArray<USCS_Node*>& AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
                for (USCS_Node* Node : AllNodes)
                {
                    if (Node->GetVariableName() == *Target)
                    {
                        if (Node->ComponentTemplate)
                        {
                            TargetClass = Node->ComponentTemplate->GetClass();
                            UE_LOG(LogTemp, Display, TEXT("Found component, class is: %s"), *TargetClass->GetName());
                        }
                        break;
                    }
                }
                
                if (!TargetClass)
                {
                    UE_LOG(LogTemp, Warning, TEXT("Component '%s' not found in blueprint"), *Target);
                }
            }
        }
        
        if (TargetClass)
        {
            UE_LOG(LogTemp, Display, TEXT("Looking for function '%s' in class '%s'"), *FunctionName, *TargetClass->GetName());
            Function = TargetClass->FindFunctionByName(*FunctionName);
            if (Function)
            {
                UE_LOG(LogTemp, Display, TEXT("Found function '%s' in class '%s'"), *FunctionName, *TargetClass->GetName());
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Function '%s' not found in class '%s'"), *FunctionName, *TargetClass->GetName());
                
                // Try the class hierarchy
                UClass* CurrentClass = TargetClass->GetSuperClass();
                while (CurrentClass && !Function)
                {
                    UE_LOG(LogTemp, Display, TEXT("Trying superclass '%s'"), *CurrentClass->GetName());
                    Function = CurrentClass->FindFunctionByName(*FunctionName);
                    if (Function)
                    {
                        UE_LOG(LogTemp, Display, TEXT("Found function '%s' in superclass '%s'"), *FunctionName, *CurrentClass->GetName());
                        break;
                    }
                    CurrentClass = CurrentClass->GetSuperClass();
                }
            }
        }
        
        if (!Function)
        {
            // Try to directly access the primitive component class
            UE_LOG(LogTemp, Display, TEXT("Trying direct access to UPrimitiveComponent for AddImpulse"));
            UClass* PrimCompClass = UPrimitiveComponent::StaticClass();
            
            // Try different casings for AddImpulse
            if (FunctionName.Equals(TEXT("AddImpulse"), ESearchCase::IgnoreCase))
            {
                // Try with exact casing
                Function = PrimCompClass->FindFunctionByName(FName(TEXT("AddImpulse")));
                if (Function)
                {
                    UE_LOG(LogTemp, Display, TEXT("Found 'AddImpulse' with exact casing"));
                    TargetClass = PrimCompClass;
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Function 'AddImpulse' not found with exact casing"));
                    
                    // Try with other casing variations
                    const TCHAR* Variations[] = {
                        TEXT("addimpulse"),
                        TEXT("ADDIMPULSE"),
                        TEXT("addImpulse"),
                        TEXT("Addimpulse")
                    };
                    
                    for (const TCHAR* Variation : Variations)
                    {
                        UE_LOG(LogTemp, Display, TEXT("Trying variation: %s"), Variation);
                        Function = PrimCompClass->FindFunctionByName(FName(Variation));
                        if (Function)
                        {
                            UE_LOG(LogTemp, Display, TEXT("Found function with variation: %s"), Variation);
                            TargetClass = PrimCompClass;
                            break;
                        }
                    }
                }
            }
            else
            {
                Function = PrimCompClass->FindFunctionByName(*FunctionName);
            }
            
            if (Function)
            {
                UE_LOG(LogTemp, Display, TEXT("Found function '%s' in UPrimitiveComponent"), *FunctionName);
                TargetClass = PrimCompClass;
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Function '%s' not found in UPrimitiveComponent"), *FunctionName);
                
                // List available functions in UPrimitiveComponent for debugging
                UE_LOG(LogTemp, Display, TEXT("Available functions in UPrimitiveComponent:"));
                int FunctionCount = 0;
                for (TFieldIterator<UFunction> FuncIt(PrimCompClass); FuncIt; ++FuncIt)
                {
                    UFunction* AvailableFunc = *FuncIt;
                    UE_LOG(LogTemp, Display, TEXT("  - %s"), *AvailableFunc->GetName());
                    FunctionCount++;
                    
                    // Check if name contains "impulse" (case insensitive)
                    FString FuncName = AvailableFunc->GetName();
                    if (FuncName.Contains(TEXT("impulse"), ESearchCase::IgnoreCase))
                    {
                        UE_LOG(LogTemp, Display, TEXT("    ***** IMPULSE FUNCTION FOUND: %s *****"), *FuncName);
                    }
                }
                UE_LOG(LogTemp, Display, TEXT("Total functions in UPrimitiveComponent: %d"), FunctionCount);
            }
        }
        
        if (!Function)
        {
            // Try to find function in global scope using UObject::StaticClass()->GetOutermost() (was ANY_PACKAGE in UE < 5.5)
            UE_LOG(LogTemp, Display, TEXT("Trying global scope for function '%s'"), *FunctionName);
            Function = FindObject<UFunction>(UObject::StaticClass()->GetOutermost(), *FunctionName);
            if (Function)
            {
                UE_LOG(LogTemp, Display, TEXT("Found function '%s' in global scope"), *FunctionName);
            }
        }
        
        // Try common utility classes if function is still not found
        if (!Function)
        {
            UE_LOG(LogTemp, Display, TEXT("Trying common utility classes for function '%s'"), *FunctionName);
            
            // List of common utility classes to check
            TArray<UClass*> UtilityClasses;
            UtilityClasses.Add(UGameplayStatics::StaticClass());
            UtilityClasses.Add(APlayerController::StaticClass());
            
            for (UClass* Class : UtilityClasses)
            {
                Function = Class->FindFunctionByName(*FunctionName);
                if (Function)
                {
                    TargetClass = Class;
                    UE_LOG(LogTemp, Display, TEXT("Found function '%s' in utility class '%s'"), 
                           *FunctionName, *Class->GetName());
                    break;
                }
            }
        }
        
        if (!Function)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Function '%s' not found"), *FunctionName));
            return ResultJson;
        }
        
        // Create the function node
        UK2Node_CallFunction* FunctionNode = nullptr;
        
        // Special handling for AddImpulse
        if (FunctionName.Equals(TEXT("AddImpulse"), ESearchCase::IgnoreCase) && !Function)
        {
            UE_LOG(LogTemp, Display, TEXT("Using direct approach for creating AddImpulse node"));
            
            // Create a call function node manually
            FunctionNode = NewObject<UK2Node_CallFunction>(EventGraph);
            if (FunctionNode)
            {
                // Set up the node using more direct methods
                FunctionNode->FunctionReference.SetExternalMember(FName(TEXT("AddImpulse")), UPrimitiveComponent::StaticClass());
                
                // Set node position
                FunctionNode->NodePosX = NodePosition.X;
                FunctionNode->NodePosY = NodePosition.Y;
                
                // Add node to graph
                EventGraph->AddNode(FunctionNode);
                FunctionNode->CreateNewGuid();
                FunctionNode->PostPlacedNewNode();
                FunctionNode->AllocateDefaultPins();
                
                // Mark blueprint dirty
                FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
            }
        }
        else
        {
            // Regular function node creation
            FunctionNode = CreateFunctionCallNode(EventGraph, Function, NodePosition);
        }
        
        if (FunctionNode)
        {
            // Set parameters if any
            if (FunctionParams)
            {
                // This would be implemented to set default values for function parameters
                // For each input pin that matches a parameter name, set its default value
                for (auto& Pair : (*FunctionParams)->Values)
                {
                    FString ParamName = Pair.Key;
                    TSharedPtr<FJsonValue> ParamValue = Pair.Value;
                    
                    // Find the pin for this parameter
                    UEdGraphPin* ParamPin = FindPin(FunctionNode, ParamName, EGPD_Input);
                    if (ParamPin)
                    {
                        // Set the pin value based on its type
                        // This would need to handle different parameter types
                        // For simplicity, we're just handling basic types here
                        if (ParamValue->Type == EJson::Boolean)
                        {
                            ParamPin->DefaultValue = ParamValue->AsBool() ? TEXT("true") : TEXT("false");
                        }
                        else if (ParamValue->Type == EJson::Number)
                        {
                            // Special case for PlayerIndex parameter - make sure it's an integer
                            if (ParamName.Equals(TEXT("PlayerIndex"), ESearchCase::IgnoreCase))
                            {
                                // Force it to be an integer by truncating the decimal part
                                int32 IntValue = FMath::TruncToInt(ParamValue->AsNumber());
                                ParamPin->DefaultValue = FString::FromInt(IntValue);
                            }
                            else
                            {
                                ParamPin->DefaultValue = FString::Printf(TEXT("%f"), ParamValue->AsNumber());
                            }
                        }
                        else if (ParamValue->Type == EJson::String)
                        {
                            // Special case for ActorClass parameter - need to use proper class reference format
                            if (ParamName.Equals(TEXT("ActorClass"), ESearchCase::IgnoreCase))
                            {
                                FString ClassName = ParamValue->AsString().TrimQuotes();
                                UClass* ReferencedClass = nullptr;
                                
                                // For CameraActor, use the exact known class
                                if (ClassName.Equals(TEXT("CameraActor"), ESearchCase::IgnoreCase) || 
                                    ClassName.Equals(TEXT("ACameraActor"), ESearchCase::IgnoreCase) ||
                                    ClassName.Equals(TEXT("Camera Actor"), ESearchCase::IgnoreCase))
                                {
                                    // Find the actual ACameraActor class
                                    ReferencedClass = ACameraActor::StaticClass();
                                }
                                else
                                {
                                    // Try finding the class with various prefixes
                                    // First try with 'A' prefix (for Actor classes)
                                    if (!ClassName.StartsWith(TEXT("A")))
                                    {
                                        ReferencedClass = FindObject<UClass>(ANY_PACKAGE, *(TEXT("A") + ClassName));
                                    }
                                    
                                    // If not found, try the name as-is
                                    if (!ReferencedClass)
                                    {
                                        ReferencedClass = FindObject<UClass>(ANY_PACKAGE, *ClassName);
                                    }
                                    
                                    // Try with Engine package path if not found
                                    if (!ReferencedClass)
                                    {
                                        FString EnginePath = TEXT("/Script/Engine.") + ClassName;
                                        ReferencedClass = FindObject<UClass>(nullptr, *EnginePath);
                                    }
                                    
                                    // Try with the Engine package path and 'A' prefix
                                    if (!ReferencedClass && !ClassName.StartsWith(TEXT("A")))
                                    {
                                        FString EnginePath = TEXT("/Script/Engine.A") + ClassName;
                                        ReferencedClass = FindObject<UClass>(nullptr, *EnginePath);
                                    }
                                }
                                
                                if (ReferencedClass)
                                {
                                    // Set the actual class object instead of a string
                                    const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(EventGraph->GetSchema());
                                    if (K2Schema)
                                    {
                                        K2Schema->TrySetDefaultObject(*ParamPin, ReferencedClass);
                                        UE_LOG(LogTemp, Log, TEXT("Successfully set class reference: %s"), *ReferencedClass->GetName());
                                    }
                                    else
                                    {
                                        UE_LOG(LogTemp, Error, TEXT("Failed to get K2Schema to set class reference"));
                                    }
                                }
                                else
                                {
                                    UE_LOG(LogTemp, Error, TEXT("Failed to find class for: %s"), *ClassName);
                                }
                            }
                            else
                            {
                                // For regular string parameters
                                ParamPin->DefaultValue = ParamValue->AsString();
                            }
                        }
                        else if (ParamValue->Type == EJson::Array)
                        {
                            // Handle array values like vectors
                            const TArray<TSharedPtr<FJsonValue>>* ArrayPtr = nullptr;
                            if (ParamValue->TryGetArray(ArrayPtr) && ArrayPtr && ArrayPtr->Num() > 0)
                            {
                                // Check if it's likely a vector (3 or 4 elements)
                                if (ArrayPtr->Num() == 3)
                                {
                                    // Likely a FVector
                                    float X = (*ArrayPtr)[0]->AsNumber();
                                    float Y = (*ArrayPtr)[1]->AsNumber();
                                    float Z = (*ArrayPtr)[2]->AsNumber();
                                    ParamPin->DefaultValue = FString::Printf(TEXT("%f,%f,%f"), X, Y, Z);
                                }
                                else if (ArrayPtr->Num() == 4)
                                {
                                    // Likely a FVector4 or FLinearColor
                                    float X = (*ArrayPtr)[0]->AsNumber();
                                    float Y = (*ArrayPtr)[1]->AsNumber();
                                    float Z = (*ArrayPtr)[2]->AsNumber();
                                    float W = (*ArrayPtr)[3]->AsNumber();
                                    ParamPin->DefaultValue = FString::Printf(TEXT("%f,%f,%f,%f"), X, Y, Z, W);
                                }
                            }
                        }
                    }
                }
            }
            
            // Mark the blueprint as modified
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
            
            // Compile the blueprint
            FKismetEditorUtilities::CompileBlueprint(Blueprint);
            
            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Added function node '%s' to blueprint '%s'"), *FunctionName, *BlueprintName));
            ResultJson->SetStringField(TEXT("node_id"), FunctionNode->NodeGuid.ToString());
        }
        else
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Failed to create function node '%s'"), *FunctionName));
        }
    }
    else if (CommandType == TEXT("add_blueprint_get_component_node"))
    {
        FString BlueprintName, ComponentName;
        Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName);
        Params->TryGetStringField(TEXT("component_name"), ComponentName);
        
        // Get node position
        const TArray<TSharedPtr<FJsonValue>>* NodePosArrayPtr = nullptr;
        FVector2D NodePosition(0.0f, 0.0f);
        
        if (Params->TryGetArrayField(TEXT("node_position"), NodePosArrayPtr) && 
            NodePosArrayPtr && NodePosArrayPtr->Num() >= 2)
        {
            NodePosition.X = (*NodePosArrayPtr)[0]->AsNumber();
            NodePosition.Y = (*NodePosArrayPtr)[1]->AsNumber();
        }
        
        // Find the blueprint asset
        FString BlueprintPath = TEXT("/Game/Blueprints/") + BlueprintName;
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        
        if (!Blueprint)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
            return ResultJson;
        }
        
        // Get event graph
        UEdGraph* EventGraph = nullptr;
        
        for (UEdGraph* Graph : Blueprint->UbergraphPages)
        {
            if (Graph->GetName().Contains(TEXT("EventGraph")))
            {
                EventGraph = Graph;
                break;
            }
        }
        
        if (!EventGraph)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), TEXT("Event graph not found in blueprint"));
            return ResultJson;
        }
        
        // Verify that the component exists in the SimpleConstructionScript
        USCS_Node* ComponentNode = nullptr;
        if (Blueprint->SimpleConstructionScript)
        {
            const TArray<USCS_Node*>& AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
            for (USCS_Node* Node : AllNodes)
            {
                if (Node->GetVariableName() == *ComponentName)
                {
                    ComponentNode = Node;
                    break;
                }
            }
        }
        
        if (ComponentNode)
        {
            // Create the get component node in the EventGraph (not SimpleConstructionScript->GetGraph())
            UK2Node_VariableGet* GetComponentNode = CreateVariableGetNode(EventGraph, Blueprint, ComponentName, NodePosition);
            if (GetComponentNode)
            {
                ResultJson->SetBoolField(TEXT("success"), true);
                ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Added get component node '%s' to blueprint '%s'"), *ComponentName, *BlueprintName));
                ResultJson->SetStringField(TEXT("node_id"), *GetComponentNode->NodeGuid.ToString());
            }
            else
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("message"), TEXT("Failed to create get component node"));
            }
        }
        else
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Component '%s' not found in blueprint '%s'"), *ComponentName, *BlueprintName));
        }
    }
    else if (CommandType == TEXT("connect_blueprint_nodes"))
    {
        FString BlueprintName, SourceNodeId, SourcePinName, TargetNodeId, TargetPinName;
        Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName);
        Params->TryGetStringField(TEXT("source_node_id"), SourceNodeId);
        Params->TryGetStringField(TEXT("source_pin"), SourcePinName);
        Params->TryGetStringField(TEXT("target_node_id"), TargetNodeId);
        Params->TryGetStringField(TEXT("target_pin"), TargetPinName);
        
        // Find the blueprint
        UBlueprint* Blueprint = FindBlueprint(BlueprintName);
        
        if (!Blueprint)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
            return ResultJson;
        }
        
        // Get the event graph
        UEdGraph* EventGraph = FindOrCreateEventGraph(Blueprint);
        if (!EventGraph)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), TEXT("Failed to get event graph"));
            return ResultJson;
        }
        
        // Find source and target nodes
        UEdGraphNode* SourceNode = nullptr;
        UEdGraphNode* TargetNode = nullptr;
        
        FGuid SourceGuid;
        FGuid TargetGuid;
        
        if (FGuid::Parse(SourceNodeId, SourceGuid))
        {
            // Find the node with this GUID
            for (UEdGraphNode* Node : EventGraph->Nodes)
            {
                if (Node && Node->NodeGuid == SourceGuid)
                {
                    SourceNode = Node;
                    break;
                }
            }
        }
        
        if (FGuid::Parse(TargetNodeId, TargetGuid))
        {
            // Find the node with this GUID
            for (UEdGraphNode* Node : EventGraph->Nodes)
            {
                if (Node && Node->NodeGuid == TargetGuid)
                {
                    TargetNode = Node;
                    break;
                }
            }
        }
        
        if (!SourceNode)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Source node with ID '%s' not found"), *SourceNodeId));
            return ResultJson;
        }
        
        if (!TargetNode)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Target node with ID '%s' not found"), *TargetNodeId));
            return ResultJson;
        }
        
        // Connect the nodes
        bool bConnectionSuccessful = ConnectGraphNodes(EventGraph, SourceNode, SourcePinName, TargetNode, TargetPinName);
        
        if (bConnectionSuccessful)
        {
            // Mark the blueprint as modified
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
            
            // Compile the blueprint
            FKismetEditorUtilities::CompileBlueprint(Blueprint);
            
            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Connected nodes in blueprint '%s'"), *BlueprintName));
        }
        else
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Failed to connect nodes in blueprint '%s'"), *BlueprintName));
        }
    }
    else if (CommandType == TEXT("add_blueprint_variable"))
    {
        FString BlueprintName, VariableName, VariableType;
        Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName);
        Params->TryGetStringField(TEXT("variable_name"), VariableName);
        Params->TryGetStringField(TEXT("variable_type"), VariableType);
        
        bool bIsExposed = false;
        if (Params->HasField(TEXT("is_exposed")))
        {
            bIsExposed = Params->GetBoolField(TEXT("is_exposed"));
        }
        
        // Get default value if provided
        TSharedPtr<FJsonValue> DefaultValue = Params->GetField<EJson::None>(TEXT("default_value"));
        
        // Find the blueprint asset
        FString BlueprintPath = TEXT("/Game/Blueprints/") + BlueprintName;
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        
        if (!Blueprint)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
            return ResultJson;
        }
        
        // Get the variable type
        FEdGraphPinType VariablePinType;
        
        if (VariableType.Equals(TEXT("Boolean"), ESearchCase::IgnoreCase))
        {
            VariablePinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        }
        else if (VariableType.Equals(TEXT("Integer"), ESearchCase::IgnoreCase) || 
                VariableType.Equals(TEXT("Int"), ESearchCase::IgnoreCase))
        {
            VariablePinType.PinCategory = UEdGraphSchema_K2::PC_Int;
        }
        else if (VariableType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
        {
            VariablePinType.PinCategory = UEdGraphSchema_K2::PC_Float;
        }
        else if (VariableType.Equals(TEXT("String"), ESearchCase::IgnoreCase))
        {
            VariablePinType.PinCategory = UEdGraphSchema_K2::PC_String;
        }
        else if (VariableType.Equals(TEXT("Vector"), ESearchCase::IgnoreCase))
        {
            VariablePinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            VariablePinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        }
        else if (VariableType.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase))
        {
            VariablePinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            VariablePinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
        }
        else
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Unsupported variable type: %s"), *VariableType));
            return ResultJson;
        }
        
        // Add the variable to the blueprint
        bool bSuccess = FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VariableName), VariablePinType);
        
        if (!bSuccess)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Failed to add variable '%s' to blueprint '%s'"), *VariableName, *BlueprintName));
            return ResultJson;
        }
        
        // Handle exposure flag
        if (bIsExposed)
        {
            FBlueprintEditorUtils::SetBlueprintPropertyReadOnlyFlag(Blueprint, FName(*VariableName), false);
        }
        
        // Set default value if provided
        if (DefaultValue.IsValid())
        {
            FString DefaultValueStr;
            
            if (VariablePinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
            {
                bool Value = DefaultValue->AsBool();
                DefaultValueStr = Value ? TEXT("true") : TEXT("false");
            }
            else if (VariablePinType.PinCategory == UEdGraphSchema_K2::PC_Int)
            {
                int32 Value = (int32)DefaultValue->AsNumber();
                DefaultValueStr = FString::FromInt(Value);
            }
            else if (VariablePinType.PinCategory == UEdGraphSchema_K2::PC_Float)
            {
                float Value = (float)DefaultValue->AsNumber();
                DefaultValueStr = FString::SanitizeFloat(Value);
            }
            else if (VariablePinType.PinCategory == UEdGraphSchema_K2::PC_String)
            {
                DefaultValueStr = DefaultValue->AsString();
            }
            else if (VariablePinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
            {
                // Vector and Rotator default values would need array processing
                // For simplicity, we're not implementing that here
            }
            
            // Set the default value using metadata
            if (!DefaultValueStr.IsEmpty())
            {
                FBlueprintEditorUtils::SetBlueprintVariableMetaData(
                    Blueprint, 
                    FName(*VariableName), 
                    nullptr, 
                    FName(TEXT("DefaultValue")), 
                    DefaultValueStr
                );
            }
        }
        
        // Mark the blueprint as modified
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Variable '%s' added to blueprint '%s'"), *VariableName, *BlueprintName));
    }
    else
    {
        ResultJson->SetBoolField(TEXT("success"), false);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Unknown blueprint node command: %s"), *CommandType));
    }
    
    return ResultJson;
}

UBlueprint* UUnrealMCPBridge::FindBlueprint(const FString& BlueprintName)
{
    // Check for the blueprint in the standard location
    FString BlueprintPath = TEXT("/Game/Blueprints/") + BlueprintName;
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    
    // If not found, try with _BP suffix (common naming convention)
    if (!Blueprint)
    {
        BlueprintPath = TEXT("/Game/Blueprints/") + BlueprintName + TEXT("_BP");
        Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    }
    
    // If not found, try to search in the entire asset registry
    if (!Blueprint)
    {
        // Attempt a more thorough search
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        TArray<FAssetData> AssetData;
        
        // Update FARFilter to use ClassPaths instead of ClassNames
        FARFilter Filter;
        Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
        Filter.bRecursiveClasses = true;
        
        AssetRegistryModule.Get().GetAssets(Filter, AssetData);
        
        for (const FAssetData& Asset : AssetData)
        {
            FString AssetName = Asset.AssetName.ToString();
            if (AssetName.Equals(BlueprintName, ESearchCase::IgnoreCase) || 
                AssetName.Equals(BlueprintName + TEXT("_BP"), ESearchCase::IgnoreCase))
            {
                Blueprint = Cast<UBlueprint>(Asset.GetAsset());
                if (Blueprint)
                {
                    break;
                }
            }
        }
    }
    
    return Blueprint;
} 

UEdGraph* UUnrealMCPBridge::FindOrCreateEventGraph(UBlueprint* Blueprint)
{
    if (!Blueprint)
        return nullptr;
        
    // Try to find existing event graph
    UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
    if (!EventGraph)
    {
        // Create new event graph
        EventGraph = FBlueprintEditorUtils::CreateNewGraph(
            Blueprint,
            FName(TEXT("EventGraph")),
            UEdGraph::StaticClass(),
            UEdGraphSchema_K2::StaticClass()
        );
        
        if (EventGraph)
        {
            FBlueprintEditorUtils::AddUbergraphPage(Blueprint, EventGraph);
        }
    }
    
    return EventGraph;
}

UK2Node_Event* UUnrealMCPBridge::CreateEventNode(UEdGraph* Graph, const FString& EventType, const FVector2D& Position)
{
    if (!Graph)
        return nullptr;
        
    UK2Node_Event* EventNode = nullptr;
    
    // Find the outer Blueprint
    UBlueprint* Blueprint = Cast<UBlueprint>(Graph->GetOuter());
    if (!Blueprint)
        return nullptr;
    
    // In UE5.5, the K2Node_Event API has been updated
    // We need to use different methods to set up event nodes
    
    // Handle different event types
    if (EventType.Equals(TEXT("BeginPlay"), ESearchCase::IgnoreCase))
    {
        UClass* ActorClass = AActor::StaticClass();
        UFunction* BeginPlayFunc = ActorClass->FindFunctionByName(FName(TEXT("ReceiveBeginPlay")));
        
        if (BeginPlayFunc)
        {
            EventNode = NewObject<UK2Node_Event>(Graph);
            
            // Use the UE5.5 API to set up the event
            // Instead of directly setting EventSignatureName and EventSignatureClass
            EventNode->CustomFunctionName = FName(*ActorClass->GetName());
            EventNode->EventReference.SetFromField<UFunction>(BeginPlayFunc, false);
            EventNode->bOverrideFunction = true;
        }
    }
    else if (EventType.Equals(TEXT("Tick"), ESearchCase::IgnoreCase))
    {
        UClass* ActorClass = AActor::StaticClass();
        UFunction* TickFunc = ActorClass->FindFunctionByName(FName(TEXT("ReceiveTick")));
        
        if (TickFunc)
        {
            EventNode = NewObject<UK2Node_Event>(Graph);
            
            EventNode->CustomFunctionName = FName(*ActorClass->GetName());
            EventNode->EventReference.SetFromField<UFunction>(TickFunc, false);
            EventNode->bOverrideFunction = true;
        }
    }
    else if (EventType.Equals(TEXT("ActorBeginOverlap"), ESearchCase::IgnoreCase))
    {
        UClass* ActorClass = AActor::StaticClass();
        UFunction* OverlapFunc = ActorClass->FindFunctionByName(FName(TEXT("ReceiveActorBeginOverlap")));
        
        if (OverlapFunc)
        {
            EventNode = NewObject<UK2Node_Event>(Graph);
            
            EventNode->CustomFunctionName = FName(*ActorClass->GetName());
            EventNode->EventReference.SetFromField<UFunction>(OverlapFunc, false);
            EventNode->bOverrideFunction = true;
        }
    }
    else if (EventType.Equals(TEXT("ActorEndOverlap"), ESearchCase::IgnoreCase))
    {
        UClass* ActorClass = AActor::StaticClass();
        UFunction* OverlapFunc = ActorClass->FindFunctionByName(FName(TEXT("ReceiveActorEndOverlap")));
        
        if (OverlapFunc)
        {
            EventNode = NewObject<UK2Node_Event>(Graph);
            
            EventNode->CustomFunctionName = FName(*ActorClass->GetName());
            EventNode->EventReference.SetFromField<UFunction>(OverlapFunc, false);
            EventNode->bOverrideFunction = true;
        }
    }
    else if (EventType.Equals(TEXT("InputAction"), ESearchCase::IgnoreCase))
    {
        // Handle input action event
        // This would need to do additional parameter handling for the action name
        // For now, we're not fully implementing this case
        UE_LOG(LogTemp, Warning, TEXT("InputAction events should be created using CreateInputActionNode function"));
        return nullptr;
    }
    else if (EventType.Equals(TEXT("ComponentBeginOverlap"), ESearchCase::IgnoreCase))
    {
        UClass* PrimitiveComponentClass = UPrimitiveComponent::StaticClass();
        UFunction* OverlapFunc = PrimitiveComponentClass->FindFunctionByName(FName(TEXT("OnComponentBeginOverlap")));
        
        if (OverlapFunc)
        {
            EventNode = NewObject<UK2Node_Event>(Graph);
            
            EventNode->CustomFunctionName = FName(*PrimitiveComponentClass->GetName());
            EventNode->EventReference.SetFromField<UFunction>(OverlapFunc, false);
            EventNode->bOverrideFunction = true;
        }
    }
    else if (EventType.Equals(TEXT("ComponentEndOverlap"), ESearchCase::IgnoreCase))
    {
        UClass* PrimitiveComponentClass = UPrimitiveComponent::StaticClass();
        UFunction* OverlapFunc = PrimitiveComponentClass->FindFunctionByName(FName(TEXT("OnComponentEndOverlap")));
        
        if (OverlapFunc)
        {
            EventNode = NewObject<UK2Node_Event>(Graph);
            
            EventNode->CustomFunctionName = FName(*PrimitiveComponentClass->GetName());
            EventNode->EventReference.SetFromField<UFunction>(OverlapFunc, false);
            EventNode->bOverrideFunction = true;
        }
    }
    else
    {
        // Try to find function with this name in the blueprint's parent class
        UClass* ParentClass = Blueprint->ParentClass;
        if (ParentClass)
        {
            UFunction* Function = ParentClass->FindFunctionByName(*EventType);
            if (Function && Function->HasAnyFunctionFlags(FUNC_Event))
            {
                EventNode = NewObject<UK2Node_Event>(Graph);
                
                EventNode->CustomFunctionName = FName(*EventType);
                EventNode->EventReference.SetFromField<UFunction>(Function, false);
                EventNode->bOverrideFunction = true;
            }
        }
    }
    
    if (EventNode)
    {
        // Set node position
        EventNode->NodePosX = Position.X;
        EventNode->NodePosY = Position.Y;
        
        // Add to graph
        Graph->AddNode(EventNode);
        EventNode->CreateNewGuid();
        EventNode->PostPlacedNewNode();
        EventNode->AllocateDefaultPins();
        
        // Fix the event node title if needed
        EventNode->ReconstructNode();
    }
    
    return EventNode;
}

UK2Node_CallFunction* UUnrealMCPBridge::CreateFunctionCallNode(UEdGraph* Graph, UFunction* Function, const FVector2D& Position)
{
    if (!Graph || !Function)
        return nullptr;
        
    UK2Node_CallFunction* FunctionNode = NewObject<UK2Node_CallFunction>(Graph);
    FunctionNode->SetFromFunction(Function);
    
    // Set node position
    FunctionNode->NodePosX = Position.X;
    FunctionNode->NodePosY = Position.Y;
    
    // Add to graph
    Graph->AddNode(FunctionNode);
    FunctionNode->CreateNewGuid();
    FunctionNode->PostPlacedNewNode();
    FunctionNode->AllocateDefaultPins();
    
    return FunctionNode;
}

UK2Node_VariableGet* UUnrealMCPBridge::CreateVariableGetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position)
{
    if (!Graph || !Blueprint)
        return nullptr;
        
    // Create the get variable node
    UK2Node_VariableGet* VarGetNode = NewObject<UK2Node_VariableGet>(Graph);
    
    // Set up the variable reference
    FMemberReference VarRef;
    VarRef.SetExternalMember(FName(*VariableName), Blueprint->SkeletonGeneratedClass);
    VarGetNode->VariableReference = VarRef;
    
    // Set node position
    VarGetNode->NodePosX = Position.X;
    VarGetNode->NodePosY = Position.Y;
    
    // Add to graph
    Graph->AddNode(VarGetNode);
    VarGetNode->CreateNewGuid();
    VarGetNode->PostPlacedNewNode();
    VarGetNode->AllocateDefaultPins();
    
    return VarGetNode;
}

UK2Node_VariableSet* UUnrealMCPBridge::CreateVariableSetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position)
{
    if (!Graph || !Blueprint)
        return nullptr;
        
    // Check if variable exists
    FProperty* TargetProperty = FindFProperty<FProperty>(Blueprint->GeneratedClass, *VariableName);
    if (!TargetProperty)
        return nullptr;
        
    UK2Node_VariableSet* VarNode = NewObject<UK2Node_VariableSet>(Graph);
    VarNode->VariableReference.SetSelfMember(FName(*VariableName));
    
    // Set node position
    VarNode->NodePosX = Position.X;
    VarNode->NodePosY = Position.Y;
    
    // Add to graph
    Graph->AddNode(VarNode);
    VarNode->CreateNewGuid();
    VarNode->PostPlacedNewNode();
    VarNode->AllocateDefaultPins();
    
    return VarNode;
}

UK2Node_InputAction* UUnrealMCPBridge::CreateInputActionNode(UEdGraph* Graph, const FString& ActionName, const FVector2D& Position)
{
    if (!Graph)
        return nullptr;
        
    UK2Node_InputAction* InputNode = NewObject<UK2Node_InputAction>(Graph);
    InputNode->InputActionName = FName(*ActionName);
    
    // Set node position
    InputNode->NodePosX = Position.X;
    InputNode->NodePosY = Position.Y;
    
    // Add to graph
    Graph->AddNode(InputNode);
    InputNode->CreateNewGuid();
    InputNode->PostPlacedNewNode();
    InputNode->AllocateDefaultPins();
    
    return InputNode;
}

bool UUnrealMCPBridge::ConnectGraphNodes(UEdGraph* Graph, UEdGraphNode* SourceNode, const FString& SourcePinName, 
                                      UEdGraphNode* TargetNode, const FString& TargetPinName)
{
    if (!Graph || !SourceNode || !TargetNode)
        return false;
        
    UEdGraphPin* SourcePin = FindPin(SourceNode, SourcePinName);
    UEdGraphPin* TargetPin = FindPin(TargetNode, TargetPinName);
    
    if (SourcePin && TargetPin)
    {
        // Make connection
        return Graph->GetSchema()->TryCreateConnection(SourcePin, TargetPin);
    }
    
    return false;
}

UEdGraphPin* UUnrealMCPBridge::FindPin(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction)
{
    if (!Node)
        return nullptr;
        
    // Find pin
    for (UEdGraphPin* Pin : Node->Pins)
    {
        // Pin matches the specified pin name
        bool bNameMatches = PinName.IsEmpty() || 
                           Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase) ||
                           Pin->GetDisplayName().ToString().Equals(PinName, ESearchCase::IgnoreCase);
        
        // Pin matches the specified direction (if any)
        bool bDirectionMatches = Direction == EGPD_MAX || Pin->Direction == Direction;
        
        if (bNameMatches && bDirectionMatches)
        {
            return Pin;
        }
    }
    
    return nullptr;
}

// Convert an actor to a JSON value
TSharedPtr<FJsonValue> UUnrealMCPBridge::ActorToJson(AActor* Actor)
{
    return MakeShareable(new FJsonValueObject(ActorToJsonObject(Actor)));
}

// Convert an actor to a JSON object
TSharedPtr<FJsonObject> UUnrealMCPBridge::ActorToJsonObject(AActor* Actor, bool bDetailed)
{
    TSharedPtr<FJsonObject> ActorJson = MakeShareable(new FJsonObject);
    
    if (!Actor)
    {
        return ActorJson;
    }
    
    // Basic properties
    ActorJson->SetStringField(TEXT("name"), Actor->GetActorLabel());
    ActorJson->SetStringField(TEXT("path"), Actor->GetPathName());
    ActorJson->SetStringField(TEXT("type"), Actor->GetClass()->GetName());
    
    // Transform
    FVector Location = Actor->GetActorLocation();
    FRotator Rotation = Actor->GetActorRotation();
    FVector Scale = Actor->GetActorScale3D();
    
    TArray<TSharedPtr<FJsonValue>> LocationArray, RotationArray, ScaleArray;
    
    LocationArray.Add(MakeShareable(new FJsonValueNumber(Location.X)));
    LocationArray.Add(MakeShareable(new FJsonValueNumber(Location.Y)));
    LocationArray.Add(MakeShareable(new FJsonValueNumber(Location.Z)));
    
    RotationArray.Add(MakeShareable(new FJsonValueNumber(Rotation.Pitch)));
    RotationArray.Add(MakeShareable(new FJsonValueNumber(Rotation.Yaw)));
    RotationArray.Add(MakeShareable(new FJsonValueNumber(Rotation.Roll)));
    
    ScaleArray.Add(MakeShareable(new FJsonValueNumber(Scale.X)));
    ScaleArray.Add(MakeShareable(new FJsonValueNumber(Scale.Y)));
    ScaleArray.Add(MakeShareable(new FJsonValueNumber(Scale.Z)));
    
    ActorJson->SetArrayField(TEXT("location"), LocationArray);
    ActorJson->SetArrayField(TEXT("rotation"), RotationArray);
    ActorJson->SetArrayField(TEXT("scale"), ScaleArray);
    
    // Detailed properties
    if (bDetailed)
    {
        TSharedPtr<FJsonObject> PropertiesJson = MakeShareable(new FJsonObject);
        
        // Basic actor properties - using compatible UE5.5 properties
        PropertiesJson->SetBoolField(TEXT("hidden"), Actor->IsHidden());
        
        // UE5.5 doesn't have bSelectable, using a reasonable alternative
        PropertiesJson->SetBoolField(TEXT("selectable"), !Actor->IsHidden());
        
        // Mobility requires root component
        if (Actor->GetRootComponent())
        {
            PropertiesJson->SetStringField(TEXT("mobility"), 
                StaticEnum<EComponentMobility::Type>()->GetNameStringByValue(
                    static_cast<int64>(Actor->GetRootComponent()->Mobility)));
        }
        
        // Components information
        TArray<TSharedPtr<FJsonValue>> ComponentsArray;
        TArray<USceneComponent*> SceneComponents;
        Actor->GetComponents<USceneComponent>(SceneComponents);
        
        for (USceneComponent* Component : SceneComponents)
        {
            TSharedPtr<FJsonObject> ComponentJson = MakeShareable(new FJsonObject);
            
            ComponentJson->SetStringField(TEXT("name"), Component->GetName());
            ComponentJson->SetStringField(TEXT("type"), Component->GetClass()->GetName());
            
            FVector CompLocation = Component->GetRelativeLocation();
            FRotator CompRotation = Component->GetRelativeRotation();
            FVector CompScale = Component->GetRelativeScale3D();
            
            TArray<TSharedPtr<FJsonValue>> CompLocationArray, CompRotationArray, CompScaleArray;
            
            CompLocationArray.Add(MakeShareable(new FJsonValueNumber(CompLocation.X)));
            CompLocationArray.Add(MakeShareable(new FJsonValueNumber(CompLocation.Y)));
            CompLocationArray.Add(MakeShareable(new FJsonValueNumber(CompLocation.Z)));
            
            CompRotationArray.Add(MakeShareable(new FJsonValueNumber(CompRotation.Pitch)));
            CompRotationArray.Add(MakeShareable(new FJsonValueNumber(CompRotation.Yaw)));
            CompRotationArray.Add(MakeShareable(new FJsonValueNumber(CompRotation.Roll)));
            
            CompScaleArray.Add(MakeShareable(new FJsonValueNumber(CompScale.X)));
            CompScaleArray.Add(MakeShareable(new FJsonValueNumber(CompScale.Y)));
            CompScaleArray.Add(MakeShareable(new FJsonValueNumber(CompScale.Z)));
            
            ComponentJson->SetArrayField(TEXT("location"), CompLocationArray);
            ComponentJson->SetArrayField(TEXT("rotation"), CompRotationArray);
            ComponentJson->SetArrayField(TEXT("scale"), CompScaleArray);
            
            ComponentsArray.Add(MakeShareable(new FJsonValueObject(ComponentJson)));
        }
        
        ActorJson->SetObjectField(TEXT("properties"), PropertiesJson);
        ActorJson->SetArrayField(TEXT("components"), ComponentsArray);
    }
    
    return ActorJson;
}

// Handle blueprint node commands
TSharedPtr<FJsonObject> UUnrealMCPBridge::HandleBlueprintNodeCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject);
    
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Handling blueprint node command: %s"), *CommandType);
    
    if (CommandType == TEXT("connect_blueprint_nodes"))
    {
        FString BlueprintName, SourceNodeID, SourcePinName, TargetNodeID, TargetPinName;
        Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName);
        Params->TryGetStringField(TEXT("source_node_id"), SourceNodeID);
        Params->TryGetStringField(TEXT("source_pin"), SourcePinName);
        Params->TryGetStringField(TEXT("target_node_id"), TargetNodeID);
        Params->TryGetStringField(TEXT("target_pin"), TargetPinName);
        
        // Find the blueprint asset
        FString BlueprintPath = TEXT("/Game/Blueprints/") + BlueprintName;
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        
        if (!Blueprint)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
            return ResultJson;
        }
        
        // Get event graph
        UEdGraph* EventGraph = nullptr;
        
        for (UEdGraph* Graph : Blueprint->UbergraphPages)
        {
            if (Graph->GetName().Contains(TEXT("EventGraph")))
            {
                EventGraph = Graph;
                break;
            }
        }
        
        if (!EventGraph)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), TEXT("Event graph not found in blueprint"));
            return ResultJson;
        }
        
        // Find the source and target nodes
        UEdGraphNode* SourceNode = nullptr;
        UEdGraphNode* TargetNode = nullptr;
        
        for (UEdGraphNode* Node : EventGraph->Nodes)
        {
            if (Node->NodeGuid.ToString() == SourceNodeID)
            {
                SourceNode = Node;
            }
            else if (Node->NodeGuid.ToString() == TargetNodeID)
            {
                TargetNode = Node;
            }
        }
        
        if (!SourceNode)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Source node with ID '%s' not found"), *SourceNodeID));
            return ResultJson;
        }
        
        if (!TargetNode)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Target node with ID '%s' not found"), *TargetNodeID));
            return ResultJson;
        }
        
        // Connect the nodes
        if (ConnectGraphNodes(EventGraph, SourceNode, SourcePinName, TargetNode, TargetPinName))
        {
            // Notify blueprint that it has changed
            Blueprint->MarkPackageDirty();
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
            
            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetStringField(TEXT("message"), TEXT("Nodes connected successfully"));
        }
        else
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), TEXT("Failed to connect nodes"));
        }
    }
    else if (CommandType == TEXT("create_input_mapping"))
    {
        FString ActionName, Key, InputType;
        Params->TryGetStringField(TEXT("action_name"), ActionName);
        Params->TryGetStringField(TEXT("key"), Key);
        Params->TryGetStringField(TEXT("input_type"), InputType);
        
        // Get the input settings
        UInputSettings* InputSettings = UInputSettings::GetInputSettings();
        if (!InputSettings)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), TEXT("Failed to get input settings"));
            return ResultJson;
        }
        
        // Create key object
        FKey KeyObj = FKey(*Key);
        if (!KeyObj.IsValid())
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Invalid key: %s"), *Key));
            return ResultJson;
        }
        
        // Add input mapping based on type
        if (InputType.Equals(TEXT("Action"), ESearchCase::IgnoreCase))
        {
            // Check if action mapping already exists
            TArray<FInputActionKeyMapping> ActionMappings;
            InputSettings->GetActionMappingByName(FName(*ActionName), ActionMappings);
            
            bool bMappingExists = false;
            for (const FInputActionKeyMapping& Mapping : ActionMappings)
            {
                if (Mapping.Key == KeyObj)
                {
                    bMappingExists = true;
                    break;
                }
            }
            
            if (!bMappingExists)
            {
                // Add new action mapping
                FInputActionKeyMapping NewMapping(FName(*ActionName), KeyObj);
                InputSettings->AddActionMapping(NewMapping);
                
                // Save the input settings
                InputSettings->SaveConfig();
                
                ResultJson->SetBoolField(TEXT("success"), true);
                ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Input action '%s' mapped to key '%s'"), *ActionName, *Key));
            }
            else
            {
                // Mapping already exists, return success
                ResultJson->SetBoolField(TEXT("success"), true);
                ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Input action '%s' is already mapped to key '%s'"), *ActionName, *Key));
            }
        }
        else if (InputType.Equals(TEXT("Axis"), ESearchCase::IgnoreCase))
        {
            // For axis mappings, we would need a scale parameter
            float Scale = 1.0f;
            if (Params->HasField(TEXT("scale")))
            {
                Scale = Params->GetNumberField(TEXT("scale"));
            }
            
            // Check if axis mapping already exists
            TArray<FInputAxisKeyMapping> AxisMappings;
            InputSettings->GetAxisMappingByName(FName(*ActionName), AxisMappings);
            
            bool bMappingExists = false;
            for (const FInputAxisKeyMapping& Mapping : AxisMappings)
            {
                if (Mapping.Key == KeyObj && FMath::IsNearlyEqual(Mapping.Scale, Scale))
                {
                    bMappingExists = true;
                    break;
                }
            }
            
            if (!bMappingExists)
            {
                // Add new axis mapping
                FInputAxisKeyMapping NewMapping(FName(*ActionName), KeyObj, Scale);
                InputSettings->AddAxisMapping(NewMapping);
                
                // Save the input settings
                InputSettings->SaveConfig();
                
                ResultJson->SetBoolField(TEXT("success"), true);
                ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Input axis '%s' mapped to key '%s' with scale %.2f"), *ActionName, *Key, Scale));
            }
            else
            {
                // Mapping already exists, return success
                ResultJson->SetBoolField(TEXT("success"), true);
                ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Input axis '%s' is already mapped to key '%s' with scale %.2f"), *ActionName, *Key, Scale));
            }
        }
        else
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Unsupported input type: %s"), *InputType));
        }
    }
    else if (CommandType == TEXT("add_blueprint_get_self_component_reference"))
    {
        FString BlueprintName, ComponentName;
        TArray<int32> NodePosition;
        
        Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName);
        Params->TryGetStringField(TEXT("component_name"), ComponentName);
        GetIntArrayFromJson(Params, TEXT("node_position"), NodePosition);
        
        // Find the blueprint
        UBlueprint* Blueprint = FindBlueprint(BlueprintName);
        if (!Blueprint)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
            return ResultJson;
        }
        
        // Get the event graph
        UEdGraph* EventGraph = FindOrCreateEventGraph(Blueprint);
        if (!EventGraph)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), TEXT("Failed to get event graph"));
            return ResultJson;
        }
        
        // Find the component in the blueprint
        UActorComponent* ComponentTemplate = nullptr;
        USCS_Node* FoundNode = nullptr;
        
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->GetVariableName() == *ComponentName)
            {
                ComponentTemplate = Node->ComponentTemplate;
                FoundNode = Node;
                break;
            }
        }
        
        if (!ComponentTemplate || !FoundNode)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Component '%s' not found in blueprint"), *ComponentName));
            return ResultJson;
        }
        
        // Create the component reference node
        UK2Node_VariableGet* VarNode = NewObject<UK2Node_VariableGet>(EventGraph);
        if (!VarNode)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("message"), TEXT("Failed to create variable get node"));
            return ResultJson;
        }
        
        // Set up the variable reference using the appropriate method for UE5.5
        FName VarName = FoundNode->GetVariableName();
        
        // Ensure we're using the proper variable reference
        VarNode->VariableReference.SetSelfMember(VarName);
        
        // Set node position if provided
        if (NodePosition.Num() >= 2)
        {
            VarNode->NodePosX = NodePosition[0];
            VarNode->NodePosY = NodePosition[1];
        }
        
        // Add the node to the graph
        EventGraph->AddNode(VarNode);
        
        // These calls are ESSENTIAL for the node to work properly and have output pins
        VarNode->CreateNewGuid();
        VarNode->PostPlacedNewNode();
        VarNode->AllocateDefaultPins();  // This creates the output pins
        
        // For UE5.5, we may need to explicitly reconstruct the node
        VarNode->ReconstructNode();
        
        // Log the pins for debugging
        UE_LOG(LogTemp, Display, TEXT("Created variable get node with %d pins"), VarNode->Pins.Num());
        for (UEdGraphPin* Pin : VarNode->Pins)
        {
            UE_LOG(LogTemp, Display, TEXT("  - Pin: %s, Direction: %d"), *Pin->GetName(), (int32)Pin->Direction);
        }
        
        // Mark the blueprint as modified
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        
        // Compile the blueprint to ensure everything is properly resolved
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        
        // Return success with the node ID
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("node_id"), VarNode->NodeGuid.ToString());
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Created component reference node for '%s'"), *ComponentName));
    }
    else
    {
        ResultJson->SetBoolField(TEXT("success"), false);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Unknown blueprint node command: %s"), *CommandType));
    }
    
    return ResultJson;
}

void UUnrealMCPBridge::GetIntArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<int32>& OutArray)
{
    const TArray<TSharedPtr<FJsonValue>>* JsonArray = nullptr;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray)
    {
        OutArray.Empty(JsonArray->Num());
        for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
        {
            if (Value.IsValid() && Value->Type == EJson::Number)
            {
                OutArray.Add(Value->AsNumber());
            }
        }
    }
}

// Sets a property on a Blueprint's CDO (Class Default Object)
TSharedPtr<FJsonObject> UUnrealMCPBridge::HandleSetBlueprintProperty(const TSharedPtr<FJsonObject>& RequestObj)
{
    TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
    bool bSuccess = false;
    FString Message;

    // Extract parameters
    FString BlueprintName;
    FString PropertyName;
    TSharedPtr<FJsonValue> PropertyValue;

    if (RequestObj->TryGetStringField(TEXT("blueprint_name"), BlueprintName) &&
        RequestObj->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        PropertyValue = RequestObj->TryGetField(TEXT("property_value"));
        if (!PropertyValue.IsValid())
        {
            Message = FString::Printf(TEXT("Missing property_value parameter"));
            UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
            ResponseObj->SetBoolField(TEXT("success"), false);
            ResponseObj->SetStringField(TEXT("message"), Message);
            return ResponseObj;
        }

        // Find the Blueprint asset
        UBlueprint* Blueprint = FindBlueprint(BlueprintName);
        if (!Blueprint)
        {
            Message = FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName);
            UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
            ResponseObj->SetBoolField(TEXT("success"), false);
            ResponseObj->SetStringField(TEXT("message"), Message);
            return ResponseObj;
        }

        // Get the Blueprint's generated class and CDO
        UClass* BPClass = Blueprint->GeneratedClass;
        if (!BPClass)
        {
            Message = FString::Printf(TEXT("Blueprint '%s' has no generated class"), *BlueprintName);
            UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
            ResponseObj->SetBoolField(TEXT("success"), false);
            ResponseObj->SetStringField(TEXT("message"), Message);
            return ResponseObj;
        }

        UObject* CDO = BPClass->GetDefaultObject();
        if (!CDO)
        {
            Message = FString::Printf(TEXT("Blueprint '%s' has no CDO"), *BlueprintName);
            UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
            ResponseObj->SetBoolField(TEXT("success"), false);
            ResponseObj->SetStringField(TEXT("message"), Message);
            return ResponseObj;
        }

        // Find the property by name
        FProperty* TargetProperty = FindFProperty<FProperty>(BPClass, *PropertyName);
        if (!TargetProperty)
        {
            Message = FString::Printf(TEXT("Property '%s' not found on Blueprint '%s'"), *PropertyName, *BlueprintName);
            UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
            ResponseObj->SetBoolField(TEXT("success"), false);
            ResponseObj->SetStringField(TEXT("message"), Message);
            return ResponseObj;
        }

        // Set the property value based on its type
        void* PropertyAddr = TargetProperty->ContainerPtrToValuePtr<void>(CDO);
        
        // Handle different property types
        if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(TargetProperty))
        {
            // Handle boolean properties
            if (PropertyValue->Type == EJson::Boolean)
            {
                BoolProperty->SetPropertyValue(PropertyAddr, PropertyValue->AsBool());
                bSuccess = true;
            }
            else
            {
                Message = TEXT("Property value must be a boolean");
            }
        }
        else if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(TargetProperty))
        {
            // Handle float properties
            if (PropertyValue->Type == EJson::Number)
            {
                FloatProperty->SetPropertyValue(PropertyAddr, PropertyValue->AsNumber());
                bSuccess = true;
            }
            else
            {
                Message = TEXT("Property value must be a number");
            }
        }
        else if (FIntProperty* IntProperty = CastField<FIntProperty>(TargetProperty))
        {
            // Handle integer properties
            if (PropertyValue->Type == EJson::Number)
            {
                IntProperty->SetPropertyValue(PropertyAddr, (int32)PropertyValue->AsNumber());
                bSuccess = true;
            }
            else
            {
                Message = TEXT("Property value must be a number");
            }
        }
        else if (FNameProperty* NameProperty = CastField<FNameProperty>(TargetProperty))
        {
            // Handle name properties
            if (PropertyValue->Type == EJson::String)
            {
                NameProperty->SetPropertyValue(PropertyAddr, FName(*PropertyValue->AsString()));
                bSuccess = true;
            }
            else
            {
                Message = TEXT("Property value must be a string");
            }
        }
        else if (FStrProperty* StringProperty = CastField<FStrProperty>(TargetProperty))
        {
            // Handle string properties
            if (PropertyValue->Type == EJson::String)
            {
                StringProperty->SetPropertyValue(PropertyAddr, PropertyValue->AsString());
                bSuccess = true;
            }
            else
            {
                Message = TEXT("Property value must be a string");
            }
        }
        else if (FByteProperty* ByteProperty = CastField<FByteProperty>(TargetProperty))
        {
            // Handle byte properties (often used for enums)
            if (UEnum* Enum = ByteProperty->GetIntPropertyEnum())
            {
                // This is an enum property
                if (PropertyValue->Type == EJson::String)
                {
                    FString EnumValueName = PropertyValue->AsString();
                    
                    // Handle enum values in the format "EnumTypeName::ValueName"
                    if (EnumValueName.Contains(TEXT("::")))
                    {
                        FString Right;
                        EnumValueName.Split(TEXT("::"), nullptr, &Right);
                        EnumValueName = Right;
                    }
                    
                    int64 EnumValue = Enum->GetValueByName(FName(*EnumValueName));
                    if (EnumValue != INDEX_NONE)
                    {
                        ByteProperty->SetPropertyValue(PropertyAddr, (uint8)EnumValue);
                        bSuccess = true;
                    }
                    else
                    {
                        Message = FString::Printf(TEXT("Invalid enum value '%s' for enum '%s'"), 
                            *EnumValueName, *Enum->GetName());
                    }
                }
                else if (PropertyValue->Type == EJson::Number)
                {
                    // Also accept direct numeric values for enums
                    int64 EnumValue = (int64)PropertyValue->AsNumber();
                    if (Enum->IsValidEnumValue(EnumValue))
                    {
                        ByteProperty->SetPropertyValue(PropertyAddr, (uint8)EnumValue);
                        bSuccess = true;
                    }
                    else
                    {
                        Message = FString::Printf(TEXT("Invalid enum value '%lld' for enum '%s'"), 
                            EnumValue, *Enum->GetName());
                    }
                }
                else
                {
                    Message = TEXT("Property value must be a string or number for enum properties");
                }
            }
            else
            {
                // Regular byte property
                if (PropertyValue->Type == EJson::Number)
                {
                    ByteProperty->SetPropertyValue(PropertyAddr, (uint8)PropertyValue->AsNumber());
                    bSuccess = true;
                }
                else
                {
                    Message = TEXT("Property value must be a number");
                }
            }
        }
        // Add more property types here as needed (FVector, FRotator, etc.)
        else
        {
            Message = FString::Printf(TEXT("Unsupported property type for '%s'"), *PropertyName);
        }

        if (bSuccess)
        {
            // Mark the Blueprint as modified
            Blueprint->Modify();
            
            // Compile the Blueprint to apply changes
            FKismetEditorUtilities::CompileBlueprint(Blueprint);
            
            Message = FString::Printf(TEXT("Successfully set property '%s' on Blueprint '%s'"), 
                *PropertyName, *BlueprintName);
            UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to set property '%s' on Blueprint '%s': %s"), 
                *PropertyName, *BlueprintName, *Message);
        }
    }
    else
    {
        Message = TEXT("Missing required parameters blueprint_name or property_name");
        UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
    }

    ResponseObj->SetBoolField(TEXT("success"), bSuccess);
    ResponseObj->SetStringField(TEXT("message"), Message);
    return ResponseObj;
}

// Adds a 'Get Self' node to a Blueprint's event graph
TSharedPtr<FJsonObject> UUnrealMCPBridge::HandleAddBlueprintSelfReference(const TSharedPtr<FJsonObject>& RequestObj)
{
    TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
    bool bSuccess = false;
    FString Message;
    FString NodeId;

    // Extract parameters
    FString BlueprintName;
    TArray<float> NodePosition;

    if (RequestObj->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        // Get node position (optional)
        const TArray<TSharedPtr<FJsonValue>>* PositionArrayPtr = nullptr;
        if (RequestObj->TryGetArrayField(TEXT("node_position"), PositionArrayPtr) && PositionArrayPtr && PositionArrayPtr->Num() >= 2)
        {
            NodePosition.Add((*PositionArrayPtr)[0]->AsNumber());
            NodePosition.Add((*PositionArrayPtr)[1]->AsNumber());
        }

        if (NodePosition.Num() < 2)
        {
            // Default position if not provided
            NodePosition.Add(0.0f);
            NodePosition.Add(0.0f);
        }

        // Find the Blueprint asset
        UBlueprint* Blueprint = FindBlueprint(BlueprintName);
        if (!Blueprint)
        {
            Message = FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName);
            UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
            ResponseObj->SetBoolField(TEXT("success"), false);
            ResponseObj->SetStringField(TEXT("message"), Message);
            return ResponseObj;
        }

        // Get the event graph
        UEdGraph* TargetGraph = FindOrCreateEventGraph(Blueprint);
        if (!TargetGraph)
        {
            Message = FString::Printf(TEXT("No event graph found in Blueprint '%s'"), *BlueprintName);
            UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
            ResponseObj->SetBoolField(TEXT("success"), false);
            ResponseObj->SetStringField(TEXT("message"), Message);
            return ResponseObj;
        }

        // Create a UK2Node_Self node (Get Self reference)
        // In UE5.5, we need to create it directly
        UK2Node_Self* SelfNode = NewObject<UK2Node_Self>(TargetGraph);
        if (SelfNode)
        {
            // Set node position
            SelfNode->NodePosX = NodePosition[0];
            SelfNode->NodePosY = NodePosition[1];
            
            // Add to graph
            TargetGraph->AddNode(SelfNode);
            SelfNode->CreateNewGuid();
            SelfNode->PostPlacedNewNode();
            SelfNode->AllocateDefaultPins();
            
            // For UE5.5, explicitly reconstruct the node
            SelfNode->ReconstructNode();
            
            NodeId = SelfNode->NodeGuid.ToString();
            
            // Mark the Blueprint as modified
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
            
            bSuccess = true;
            Message = FString::Printf(TEXT("Successfully added Self reference node to Blueprint '%s'"), *BlueprintName);
            UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
        }
        else
        {
            Message = FString::Printf(TEXT("Failed to create Self reference node in Blueprint '%s'"), *BlueprintName);
            UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
        }
    }
    else
    {
        Message = TEXT("Missing required parameter blueprint_name");
        UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
    }

    ResponseObj->SetBoolField(TEXT("success"), bSuccess);
    ResponseObj->SetStringField(TEXT("message"), Message);
    if (!NodeId.IsEmpty())
    {
        ResponseObj->SetStringField(TEXT("node_id"), NodeId);
    }
    return ResponseObj;
}

// Add this new function after HandleAddBlueprintSelfReference
// Find nodes in a Blueprint's graph based on type criteria
TSharedPtr<FJsonObject> UUnrealMCPBridge::HandleFindBlueprintNodes(const TSharedPtr<FJsonObject>& RequestObj)
{
    TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
    bool bSuccess = false;
    FString Message;
    TArray<TSharedPtr<FJsonValue>> FoundNodes;

    // Extract parameters
    FString BlueprintName;
    FString NodeType;
    FString EventType;

    if (RequestObj->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        // These parameters are optional
        RequestObj->TryGetStringField(TEXT("node_type"), NodeType);
        RequestObj->TryGetStringField(TEXT("event_type"), EventType);

        // Find the Blueprint asset
        UBlueprint* Blueprint = FindBlueprint(BlueprintName);
        if (!Blueprint)
        {
            Message = FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName);
            UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
            ResponseObj->SetBoolField(TEXT("success"), false);
            ResponseObj->SetStringField(TEXT("message"), Message);
            return ResponseObj;
        }

        // Get the event graph
        UEdGraph* TargetGraph = FindOrCreateEventGraph(Blueprint);
        if (!TargetGraph)
        {
            Message = FString::Printf(TEXT("No event graph found in Blueprint '%s'"), *BlueprintName);
            UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
            ResponseObj->SetBoolField(TEXT("success"), false);
            ResponseObj->SetStringField(TEXT("message"), Message);
            return ResponseObj;
        }

        // Search for nodes matching criteria
        for (UEdGraphNode* Node : TargetGraph->Nodes)
        {
            bool bMatchesNodeType = true;
            bool bMatchesEventType = true;

            // Check if node matches the specified type
            if (!NodeType.IsEmpty())
            {
                if (NodeType.Equals(TEXT("Event"), ESearchCase::IgnoreCase))
                {
                    bMatchesNodeType = Node->IsA<UK2Node_Event>();
                }
                else if (NodeType.Equals(TEXT("Function"), ESearchCase::IgnoreCase))
                {
                    bMatchesNodeType = Node->IsA<UK2Node_CallFunction>();
                }
                else if (NodeType.Equals(TEXT("Variable"), ESearchCase::IgnoreCase))
                {
                    bMatchesNodeType = Node->IsA<UK2Node_VariableGet>() || Node->IsA<UK2Node_VariableSet>();
                }
                else if (NodeType.Equals(TEXT("InputAction"), ESearchCase::IgnoreCase))
                {
                    bMatchesNodeType = Node->IsA<UK2Node_InputAction>();
                }
            }

            // Check if this is a matching event node
            if (!EventType.IsEmpty() && Node->IsA<UK2Node_Event>())
            {
                UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
                
                // For UE5.5 compatibility, we have to check function name via EventReference
                UFunction* EventFunc = EventNode->EventReference.ResolveMember<UFunction>(EventNode->GetBlueprintClassFromNode());
                if (EventFunc)
                {
                    // Check if this matches the BeginPlay event
                    if (EventType.Equals(TEXT("BeginPlay"), ESearchCase::IgnoreCase) &&
                        EventFunc->GetName().Contains(TEXT("ReceiveBeginPlay")))
                    {
                        bMatchesEventType = true;
                    }
                    // Check if this matches the Tick event
                    else if (EventType.Equals(TEXT("Tick"), ESearchCase::IgnoreCase) &&
                             EventFunc->GetName().Contains(TEXT("ReceiveTick")))
                    {
                        bMatchesEventType = true;
                    }
                    else
                    {
                        bMatchesEventType = false;
                    }
                }
                else
                {
                    bMatchesEventType = false;
                }
            }
            else if (!EventType.IsEmpty())
            {
                // If EventType is specified but this is not an event node
                bMatchesEventType = false;
            }

            // If node matches all criteria, add it to results
            if (bMatchesNodeType && bMatchesEventType)
            {
                TSharedPtr<FJsonObject> NodeInfo = MakeShared<FJsonObject>();
                NodeInfo->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
                NodeInfo->SetStringField(TEXT("node_type"), Node->GetClass()->GetName());
                
                // Add specific event info if this is an event node
                if (Node->IsA<UK2Node_Event>())
                {
                    UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
                    UFunction* EventFunc = EventNode->EventReference.ResolveMember<UFunction>(EventNode->GetBlueprintClassFromNode());
                    if (EventFunc)
                    {
                        NodeInfo->SetStringField(TEXT("event_name"), EventFunc->GetName());
                    }
                }
                
                FoundNodes.Add(MakeShared<FJsonValueObject>(NodeInfo));
            }
        }

        bSuccess = true;
        Message = FString::Printf(TEXT("Found %d matching nodes in Blueprint '%s'"), FoundNodes.Num(), *BlueprintName);
        UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
    }
    else
    {
        Message = TEXT("Missing required parameter blueprint_name");
        UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
    }

    ResponseObj->SetBoolField(TEXT("success"), bSuccess);
    ResponseObj->SetStringField(TEXT("message"), Message);
    ResponseObj->SetArrayField(TEXT("nodes"), FoundNodes);
    return ResponseObj;
}