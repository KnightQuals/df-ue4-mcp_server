#include "Commands/UnrealMCPEditorCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "ImageUtils.h"
#include "HighResScreenshot.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "Editor/EditorEngine.h"
#include "FileHelpers.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "Components/StaticMeshComponent.h"
#include "EditorSubsystem.h"
// UE4.27: EditorActorSubsystem does not exist (UE5 only); not used in this file, so removed.
// #include "Subsystems/EditorActorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "ObjectTools.h"
#include "Misc/OutputDeviceNull.h"

FUnrealMCPEditorCommands::FUnrealMCPEditorCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    // Actor manipulation commands
    if (CommandType == TEXT("get_actors_in_level"))
    {
        return HandleGetActorsInLevel(Params);
    }
    else if (CommandType == TEXT("find_actors_by_name"))
    {
        return HandleFindActorsByName(Params);
    }
    else if (CommandType == TEXT("spawn_actor") || CommandType == TEXT("create_actor"))
    {
        if (CommandType == TEXT("create_actor"))
        {
            UE_LOG(LogTemp, Warning, TEXT("'create_actor' command is deprecated and will be removed in a future version. Please use 'spawn_actor' instead."));
        }
        return HandleSpawnActor(Params);
    }
    else if (CommandType == TEXT("delete_actor"))
    {
        return HandleDeleteActor(Params);
    }
    else if (CommandType == TEXT("set_actor_transform"))
    {
        return HandleSetActorTransform(Params);
    }
    else if (CommandType == TEXT("get_actor_properties"))
    {
        return HandleGetActorProperties(Params);
    }
    else if (CommandType == TEXT("set_actor_property"))
    {
        return HandleSetActorProperty(Params);
    }
    else if (CommandType == TEXT("set_world_game_mode"))
    {
        return HandleSetWorldGameMode(Params);
    }
    // Set SkeletalMesh on an actor's SkeletalMeshComponent (Character GetMesh())
    // so users can pick a humanoid model via MCP.
    else if (CommandType == TEXT("set_actor_skeletal_mesh"))
    {
        return HandleSetActorSkeletalMesh(Params);
    }
    // Level management commands
    else if (CommandType == TEXT("save_level"))
    {
        return HandleSaveLevel(Params);
    }
    else if (CommandType == TEXT("new_level"))
    {
        return HandleNewLevel(Params);
    }
    else if (CommandType == TEXT("open_level"))
    {
        return HandleOpenLevel(Params);
    }
    else if (CommandType == TEXT("duplicate_level"))
    {
        return HandleDuplicateLevel(Params);
    }
    // Blueprint actor spawning
    else if (CommandType == TEXT("spawn_blueprint_actor"))
    {
        return HandleSpawnBlueprintActor(Params);
    }
    // Editor viewport commands
    else if (CommandType == TEXT("focus_viewport"))
    {
        return HandleFocusViewport(Params);
    }
    else if (CommandType == TEXT("take_screenshot"))
    {
        return HandleTakeScreenshot(Params);
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown editor command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
{
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> ActorArray;
    for (AActor* Actor : AllActors)
    {
        if (Actor)
        {
            ActorArray.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), ActorArray);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
{
    FString Pattern;
    if (!Params->TryGetStringField(TEXT("pattern"), Pattern))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pattern' parameter"));
    }
    
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> MatchingActors;
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName().Contains(Pattern))
        {
            MatchingActors.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), MatchingActors);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActorType;
    if (!Params->TryGetStringField(TEXT("type"), ActorType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    // Get actor name (required parameter)
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Get optional transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Create the actor based on type
    AActor* NewActor = nullptr;
    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Check if an actor with this name already exists
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
        }
    }

    FActorSpawnParameters SpawnParams;
    // Use Requested name mode: if the name is already taken, UE auto-appends a suffix
    // instead of crashing (LevelActor.cpp:417 Fatal error on duplicate FName). We also
    // set the actor's editor label separately below so it's still identifiable.
    SpawnParams.Name = MakeUniqueObjectName(World, AStaticMeshActor::StaticClass(), *ActorName);
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

    if (ActorType == TEXT("StaticMeshActor"))
    {
        NewActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("PointLight"))
    {
        NewActor = World->SpawnActor<APointLight>(APointLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("SpotLight"))
    {
        NewActor = World->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("DirectionalLight"))
    {
        NewActor = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("CameraActor"))
    {
        NewActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, SpawnParams);
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown actor type: %s"), *ActorType));
    }

    if (NewActor)
    {
        // Give the actor a readable editor label matching the requested name.
        NewActor->SetActorLabel(ActorName);

        // Set scale (since SpawnActor only takes location and rotation)
        FTransform Transform = NewActor->GetTransform();
        Transform.SetScale3D(Scale);
        NewActor->SetActorTransform(Transform);

        // Optional: if a 'mesh_path' is provided and this is a StaticMeshActor,
        // load and assign the StaticMesh so we can spawn real environment assets
        // (e.g. KiteDemo trees/rocks) via MCP, not just empty actors.
        FString MeshPath;
        if (Params->TryGetStringField(TEXT("mesh_path"), MeshPath) && !MeshPath.IsEmpty())
        {
            if (AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(NewActor))
            {
                UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
                if (Mesh && SMActor->GetStaticMeshComponent())
                {
                    // StaticMeshActor's mesh component is static by default; make it
                    // movable so SetStaticMesh at runtime/editor time is allowed.
                    SMActor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
                    SMActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("spawn_actor: mesh not found at %s"), *MeshPath);
                }
            }
        }

        // Return the created actor's details
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create actor"));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            // Store actor info before deletion for the response
            TSharedPtr<FJsonObject> ActorInfo = FUnrealMCPCommonUtils::ActorToJsonObject(Actor);
            
            // Delete the actor
            Actor->Destroy();
            
            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetObjectField(TEXT("deleted_actor"), ActorInfo);
            return ResultObj;
        }
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get transform parameters
    FTransform NewTransform = TargetActor->GetTransform();

    if (Params->HasField(TEXT("location")))
    {
        NewTransform.SetLocation(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        NewTransform.SetRotation(FQuat(FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"))));
    }
    if (Params->HasField(TEXT("scale")))
    {
        NewTransform.SetScale3D(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
    }

    // Set the new transform
    TargetActor->SetActorTransform(NewTransform);

    // Return updated actor info
    return FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Always return detailed properties for this command
    return FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get property name
    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }

    // Get property value
    if (!Params->HasField(TEXT("property_value")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));
    }
    
    TSharedPtr<FJsonValue> PropertyValue = Params->Values.FindRef(TEXT("property_value"));
    
    // Set the property using our utility function
    FString ErrorMessage;
    if (FUnrealMCPCommonUtils::SetObjectProperty(TargetActor, PropertyName, PropertyValue, ErrorMessage))
    {
        // Property set successfully
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("actor"), ActorName);
        ResultObj->SetStringField(TEXT("property"), PropertyName);
        ResultObj->SetBoolField(TEXT("success"), true);
        
        // Also include the full actor details
        ResultObj->SetObjectField(TEXT("actor_details"), FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true));
        return ResultObj;
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetWorldGameMode(const TSharedPtr<FJsonObject>& Params)
{
    FString ClassPath;
    if (!Params->TryGetStringField(TEXT("class_path"), ClassPath) || ClassPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'class_path' parameter (e.g. /Game/Blueprints/BP_GameMode_Conquest.BP_GameMode_Conquest_C)"));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || !World->GetWorldSettings())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor WorldSettings available"));
    }

    UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, *ClassPath);
    if (!GameModeClass)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load GameMode class: %s"), *ClassPath));
    }

    AWorldSettings* Settings = World->GetWorldSettings();
    Settings->Modify();
    Settings->DefaultGameMode = GameModeClass;
    Settings->MarkPackageDirty();

    const bool bSaved = UEditorLoadingAndSavingUtils::SaveCurrentLevel();
    if (!bSaved)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("GameMode set to %s but failed to save current map"), *ClassPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("level"), World->GetOutermost()->GetName());
    ResultObj->SetStringField(TEXT("class_path"), ClassPath);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorSkeletalMesh(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString MeshPath;
    if (!Params->TryGetStringField(TEXT("mesh_path"), MeshPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'mesh_path' parameter"));
    }

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Find the SkeletalMeshComponent: any actor with a mesh works (ACharacter has GetMesh()).
    USkeletalMeshComponent* SkelComp = TargetActor->FindComponentByClass<USkeletalMeshComponent>();
    if (!SkelComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Actor has no SkeletalMeshComponent"));
    }

    USkeletalMesh* NewMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
    if (!NewMesh)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load skeletal mesh: %s"), *MeshPath));
    }

    SkelComp->SetSkeletalMesh(NewMesh);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor"), ActorName);
    ResultObj->SetStringField(TEXT("mesh_path"), MeshPath);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSaveLevel(const TSharedPtr<FJsonObject>& Params)
{
    // Save the current level. If 'path' is provided, save as that path; otherwise
    // save in place (or prompt if the level has no path yet).
    FString SavePath;
    Params->TryGetStringField(TEXT("path"), SavePath);

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));
    }

    bool bSaved = false;
    const FString CurrentPackageName = World->GetOutermost()->GetName();
    if (!SavePath.IsEmpty() && CurrentPackageName != SavePath)
    {
        // Save As via EditorFileUtils. UEditorLoadingAndSavingUtils::SaveMap is fine
        // for saving the current package, but in UE4.27 it can block indefinitely
        // when asked to fork a loaded .umap into another map package. SaveLevelAs
        // creates a fresh world/package identity (no duplicate Map PrimaryAssetID).
        FString SaveFilename = FPackageName::LongPackageNameToFilename(SavePath, FPackageName::GetMapPackageExtension());
        bSaved = FEditorFileUtils::SaveLevelAs(World->PersistentLevel, &SaveFilename);
    }
    else
    {
        // Save in place. SaveCurrentLevel() pops a modal "Save As" dialog when the
        // level package has no disk file yet (fresh new_level), which deadlocks
        // the game-thread MCP dispatch. For that bootstrap case only, write the
        // package directly (the file persists even though the editor crashes
        // shortly after on a pure-virtual during post-save GC — a one-time cost
        // per brand-new map). Disk-backed levels keep using SaveCurrentLevel,
        // which is the battle-tested path.
        const FString SaveFilename = FPackageName::LongPackageNameToFilename(
            CurrentPackageName, FPackageName::GetMapPackageExtension());
        if (!FPaths::FileExists(SaveFilename))
        {
            World->GetOutermost()->MarkPackageDirty();
            bSaved = UPackage::SavePackage(World->GetOutermost(), World,
                RF_Public | RF_Standalone, *SaveFilename, GError, nullptr, false, true, SAVE_NoError);
        }
        else
        {
            bSaved = UEditorLoadingAndSavingUtils::SaveCurrentLevel();
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bSaved);
    ResultObj->SetStringField(TEXT("level"), World->GetName());
    ResultObj->SetStringField(TEXT("path"), SavePath.IsEmpty() ? World->GetPathName() : SavePath);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleNewLevel(const TSharedPtr<FJsonObject>& Params)
{
    // Create a new empty level and save it at the given asset path, e.g. "/Game/Maps/NewMap"
    FString LevelPath;
    if (!Params->TryGetStringField(TEXT("path"), LevelPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter (e.g. /Game/Maps/NewMap)"));
    }

    // NewBlankMap(bool bSaveExistingMap) — creates a fresh blank map, returns the new world.
    UWorld* NewWorld = UEditorLoadingAndSavingUtils::NewBlankMap(true);
    if (!NewWorld)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create new blank level"));
    }

    // Save the new map to the requested asset path so it actually persists.
    bool bSaved = UEditorLoadingAndSavingUtils::SaveMap(NewWorld, LevelPath);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bSaved);
    ResultObj->SetStringField(TEXT("level"), NewWorld->GetName());
    ResultObj->SetStringField(TEXT("path"), LevelPath);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleOpenLevel(const TSharedPtr<FJsonObject>& Params)
{
    // Open an existing level by asset path, e.g. "/Game/Maps/MainMap"
    FString LevelPath;
    if (!Params->TryGetStringField(TEXT("path"), LevelPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter (e.g. /Game/Maps/MainMap)"));
    }

    if (!FPackageName::DoesPackageExist(LevelPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Level not found: %s"), *LevelPath));
    }

    UWorld* Loaded = UEditorLoadingAndSavingUtils::LoadMap(LevelPath);
    if (!Loaded)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load level: %s"), *LevelPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("path"), LevelPath);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleDuplicateLevel(const TSharedPtr<FJsonObject>& Params)
{
    // Duplicate through EditorAssetLibrary rather than copying .umap bytes. A raw copy
    // preserves the source world's internal PrimaryAssetId and triggers UE's duplicate
    // Map-ID warning; this API creates a new package/world with a unique identity.
    FString SourcePath, DestinationPath;
    if (!Params->TryGetStringField(TEXT("source_path"), SourcePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_path' parameter (e.g. /Game/Maps/NewMap)"));
    }
    if (!Params->TryGetStringField(TEXT("destination_path"), DestinationPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'destination_path' parameter (e.g. /Game/Maps/MainMap)"));
    }
    // Map packages need engine-level duplication; raw .umap copies retain the source
    // identity and trigger duplicate Map-ID warnings. UE4.27's ObjectTools route is
    // stable when the source is not the active editor world. Guard the active-source
    // case instead of crashing the entire editor (EXCEPTION_ACCESS_VIOLATION observed
    // during NewMap -> BackUpMap snapshots). The orchestrator opens a different map
    // first, then retries the same command.
    const FString SourceObjectPath = SourcePath + TEXT(".") + FPackageName::GetLongPackageAssetName(SourcePath);
    const FString DestinationObjectPath = DestinationPath + TEXT(".") + FPackageName::GetLongPackageAssetName(DestinationPath);
    UWorld* SourceWorld = LoadObject<UWorld>(nullptr, *SourceObjectPath);
    if (!SourceWorld)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source level not found: %s"), *SourcePath));
    }
    if (UEditorAssetLibrary::DoesAssetExist(DestinationObjectPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Destination already exists: %s"), *DestinationPath));
    }
    UWorld* ActiveWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (ActiveWorld && ActiveWorld->GetOutermost()->GetName() == SourcePath)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source level is active: %s. Open a different level, then retry duplicate_level."), *SourcePath));
    }

    ObjectTools::FPackageGroupName NewPGN;
    NewPGN.PackageName = DestinationPath;
    NewPGN.ObjectName = FPackageName::GetLongPackageAssetName(DestinationPath);
    TSet<UPackage*> PackagesUserRefusedToFullyLoad;
    UWorld* DuplicatedWorld = Cast<UWorld>(ObjectTools::DuplicateSingleObject(SourceWorld, NewPGN, PackagesUserRefusedToFullyLoad, false));
    if (!DuplicatedWorld)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to duplicate level %s -> %s"), *SourcePath, *DestinationPath));
    }

    UPackage* DuplicatedPackage = DuplicatedWorld->GetOutermost();
    DuplicatedPackage->MarkAsFullyLoaded();
    const FString DestinationFilename = FPackageName::LongPackageNameToFilename(DestinationPath, FPackageName::GetMapPackageExtension());
    FOutputDeviceNull SaveOutput;
    const bool bSaved = GEditor->Exec(nullptr, *FString::Printf(TEXT("OBJ SAVEPACKAGE PACKAGE=\"%s\" FILE=\"%s\" SILENT=true"), *DuplicatedPackage->GetName(), *DestinationFilename), SaveOutput);
    if (bSaved)
    {
        FEditorFileUtils::SaveMapDataPackages(DuplicatedWorld, true, true);
    }
    if (!bSaved)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Duplicated level but failed to save: %s"), *DestinationPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("source_path"), SourcePath);
    ResultObj->SetStringField(TEXT("destination_path"), DestinationPath);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    // Find the blueprint
    if (BlueprintName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint name is empty"));
    }

    // Full object paths (e.g. /Game/KiteDemo/.../Skybox.Skybox) bypass the
    // /Game/Blueprints convention so migrated asset-pack blueprints are spawnable.
    FString AssetPath;
    if (BlueprintName.StartsWith(TEXT("/")))
    {
        AssetPath = BlueprintName;
    }
    else
    {
        AssetPath = TEXT("/Game/Blueprints/") + BlueprintName;
    }

    if (!FPackageName::DoesPackageExist(AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found – expected under /Game/Blueprints or a full object path"), *BlueprintName));
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Spawn the actor
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    FTransform SpawnTransform;
    SpawnTransform.SetLocation(Location);
    SpawnTransform.SetRotation(FQuat(Rotation));
    SpawnTransform.SetScale3D(Scale);

    // Guard against duplicate actor names. UE's low-level SpawnActor triggers a
    // Fatal error (check) if an actor with the same name already exists in the
    // level, which crashes the whole editor. Detect it here and return a normal
    // error response instead of letting the engine assert.
    if (!ActorName.IsEmpty())
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetName() == ActorName)
            {
                return FUnrealMCPCommonUtils::CreateErrorResponse(
                    FString::Printf(TEXT("An actor named '%s' already exists in the level. Use a unique actor_name."), *ActorName));
            }
        }
    }

    FActorSpawnParameters SpawnParams;
    if (!ActorName.IsEmpty())
    {
        SpawnParams.Name = *ActorName;
    }
    // If the requested name somehow still collides, ask the engine to pick a
    // unique name rather than fataling.
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

    AActor* NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform, SpawnParams);
    if (NewActor)
    {
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn blueprint actor"));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleFocusViewport(const TSharedPtr<FJsonObject>& Params)
{
    // Get target actor name if provided
    FString TargetActorName;
    bool HasTargetActor = Params->TryGetStringField(TEXT("target"), TargetActorName);

    // Get location if provided
    FVector Location(0.0f, 0.0f, 0.0f);
    bool HasLocation = false;
    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
        HasLocation = true;
    }

    // Get distance
    float Distance = 1000.0f;
    if (Params->HasField(TEXT("distance")))
    {
        Distance = Params->GetNumberField(TEXT("distance"));
    }

    // Get orientation if provided
    FRotator Orientation(0.0f, 0.0f, 0.0f);
    bool HasOrientation = false;
    if (Params->HasField(TEXT("orientation")))
    {
        Orientation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("orientation"));
        HasOrientation = true;
    }

    // Get the active viewport
    FLevelEditorViewportClient* ViewportClient = (FLevelEditorViewportClient*)GEditor->GetActiveViewport()->GetClient();
    if (!ViewportClient)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get active viewport"));
    }

    // If we have a target actor, focus on it
    if (HasTargetActor)
    {
        // Find the actor
        AActor* TargetActor = nullptr;
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
        
        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->GetName() == TargetActorName)
            {
                TargetActor = Actor;
                break;
            }
        }

        if (!TargetActor)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *TargetActorName));
        }

        // Focus on the actor
        ViewportClient->SetViewLocation(TargetActor->GetActorLocation() - FVector(Distance, 0.0f, 0.0f));
    }
    // Otherwise use the provided location
    else if (HasLocation)
    {
        ViewportClient->SetViewLocation(Location - FVector(Distance, 0.0f, 0.0f));
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Either 'target' or 'location' must be provided"));
    }

    // Set orientation if provided
    if (HasOrientation)
    {
        ViewportClient->SetViewRotation(Orientation);
    }

    // Force viewport to redraw
    ViewportClient->Invalidate();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    // Get file path parameter
    FString FilePath;
    if (!Params->TryGetStringField(TEXT("filepath"), FilePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'filepath' parameter"));
    }
    
    // Ensure the file path has a proper extension
    if (!FilePath.EndsWith(TEXT(".png")))
    {
        FilePath += TEXT(".png");
    }

    // Get the active viewport
    if (GEditor && GEditor->GetActiveViewport())
    {
        FViewport* Viewport = GEditor->GetActiveViewport();
        TArray<FColor> Bitmap;
        FIntRect ViewportRect(0, 0, Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y);
        
        if (Viewport->ReadPixels(Bitmap, FReadSurfaceDataFlags(), ViewportRect))
        {
            TArray<uint8> CompressedBitmap;
            FImageUtils::CompressImageArray(Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, Bitmap, CompressedBitmap);
            
            if (FFileHelper::SaveArrayToFile(CompressedBitmap, *FilePath))
            {
                TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
                ResultObj->SetStringField(TEXT("filepath"), FilePath);
                return ResultObj;
            }
        }
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to take screenshot"));
} 