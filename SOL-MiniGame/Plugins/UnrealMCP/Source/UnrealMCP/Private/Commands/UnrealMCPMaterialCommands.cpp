#include "Commands/UnrealMCPMaterialCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Factories/Factory.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/UObjectIterator.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"

FUnrealMCPMaterialCommands::FUnrealMCPMaterialCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_material"))
    {
        return HandleCreateMaterial(Params);
    }
    else if (CommandType == TEXT("add_vector_parameter"))
    {
        return HandleAddVectorParameter(Params);
    }
    else if (CommandType == TEXT("set_material_on_component"))
    {
        return HandleSetMaterialOnComponent(Params);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown material command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_name' parameter"));
    }

    FString SavePath = TEXT("/Game/Materials");
    Params->TryGetStringField(TEXT("save_path"), SavePath);

    // Normalize the save path: strip trailing slash, ensure it starts with /Game
    SavePath = SavePath.TrimEnd();
    if (SavePath.EndsWith(TEXT("/")))
    {
        SavePath = SavePath.LeftChop(1);
    }

    const FString PackagePath = SavePath + TEXT("/") + MaterialName;

    // Check if material already exists
    if (UEditorAssetLibrary::DoesAssetExist(PackagePath + TEXT(".") + MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material already exists: %s"), *PackagePath));
    }

    // Create the package (4.27 single-argument overload).
    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for material"));
    }

    // Create the material asset
    UMaterial* Material = NewObject<UMaterial>(Package, *MaterialName, RF_Public | RF_Standalone);
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material object"));
    }

    // Mark the package as dirty and save it (4.27 SavePackage signature).
    Material->MarkPackageDirty();
    Material->PostEditChange();

    const FString PackageFileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
    const bool bSaved = UPackage::SavePackage(Package, Material, RF_Public | RF_Standalone, *PackageFileName);

    if (!bSaved)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to save material package"));
    }

    // Notify the asset registry so the new asset shows up immediately
    FAssetRegistryModule::AssetCreated(Material);

    TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
    ResultData->SetStringField(TEXT("material_name"), MaterialName);
    ResultData->SetStringField(TEXT("material_path"), PackagePath + TEXT(".") + MaterialName);
    ResultData->SetStringField(TEXT("package_path"), PackagePath);

    return FUnrealMCPCommonUtils::CreateSuccessResponse(ResultData);
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleAddVectorParameter(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_name' parameter"));
    }

    FString ParamName;
    if (!Params->TryGetStringField(TEXT("param_name"), ParamName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'param_name' parameter"));
    }

    // Resolve the material asset. Accept either a bare name or a full asset path.
    FString AssetPath = MaterialName;
    if (!AssetPath.Contains(TEXT("/")))
    {
        // Search the common locations for a bare name.
        const FString CandidateA = FString::Printf(TEXT("/Game/Materials/%s.%s"), *MaterialName, *MaterialName);
        const FString CandidateB = FString::Printf(TEXT("/Game/%s.%s"), *MaterialName, *MaterialName);
        if (UEditorAssetLibrary::DoesAssetExist(CandidateA))
        {
            AssetPath = CandidateA;
        }
        else if (UEditorAssetLibrary::DoesAssetExist(CandidateB))
        {
            AssetPath = CandidateB;
        }
        else
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialName));
        }
    }

    UMaterial* Material = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load material: %s"), *AssetPath));
    }

    // Parse the default color (optional, defaults to mid-gray).
    FLinearColor DefaultColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
    const TSharedPtr<FJsonObject>* ColorObj = nullptr;
    if (Params->TryGetObjectField(TEXT("default_color"), ColorObj) && ColorObj && ColorObj->IsValid())
    {
        const TSharedPtr<FJsonObject>& C = *ColorObj;
        if (C->HasField(TEXT("r"))) DefaultColor.R = (float)C->GetNumberField(TEXT("r"));
        if (C->HasField(TEXT("g"))) DefaultColor.G = (float)C->GetNumberField(TEXT("g"));
        if (C->HasField(TEXT("b"))) DefaultColor.B = (float)C->GetNumberField(TEXT("b"));
        if (C->HasField(TEXT("a"))) DefaultColor.A = (float)C->GetNumberField(TEXT("a"));
    }
    else
    {
        // Also accept a flat [r,g,b,a] array.
        TArray<float> ColorArray;
        FUnrealMCPCommonUtils::GetFloatArrayFromJson(Params, TEXT("default_color"), ColorArray);
        if (ColorArray.Num() >= 3)
        {
            DefaultColor = FLinearColor(ColorArray[0], ColorArray[1], ColorArray[2],
                                        ColorArray.Num() >= 4 ? ColorArray[3] : 1.0f);
        }
    }

    // Add a Vector Parameter expression node (4.27: expressions live in Material->Expressions).
    Material->PreEditChange(nullptr);

    UMaterialExpressionVectorParameter* VectorParam = NewObject<UMaterialExpressionVectorParameter>(Material);
    VectorParam->ParameterName = FName(*ParamName);
    VectorParam->DefaultValue = DefaultColor;
    VectorParam->Desc = FString::Printf(TEXT("Vector parameter '%s' added via MCP"), *ParamName);
    VectorParam->Material = Material;

    // Register the expression with the material (4.27 API).
    Material->Expressions.Add(VectorParam);

    // Connect the node's output to the material's BaseColor input.
    // BaseColor is a FColorMaterialInput (which derives from FExpressionInput); set its fields directly.
    Material->BaseColor.Expression = VectorParam;
    Material->BaseColor.OutputIndex = 0;

    Material->PostEditChange();
    Material->MarkPackageDirty();

    // Re-save the package so the node persists (4.27 SavePackage signature).
    UPackage* Package = Material->GetOutermost();
    if (Package)
    {
        const FString PackageFileName = FPackageName::LongPackageNameToFilename(*Package->GetName(), FPackageName::GetAssetPackageExtension());
        UPackage::SavePackage(Package, Material, RF_Public | RF_Standalone, *PackageFileName);
    }

    TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
    ResultData->SetStringField(TEXT("material_name"), MaterialName);
    ResultData->SetStringField(TEXT("material_path"), AssetPath);
    ResultData->SetStringField(TEXT("param_name"), ParamName);
    ResultData->SetStringField(TEXT("connected_to"), TEXT("BaseColor"));

    return FUnrealMCPCommonUtils::CreateSuccessResponse(ResultData);
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleSetMaterialOnComponent(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
    }

    int32 MaterialIndex = 0;
    Params->TryGetNumberField(TEXT("material_index"), MaterialIndex);

    // Find the actor by name in the current level.
    AActor* TargetActor = nullptr;
    for (TActorIterator<AActor> It(GWorld); It; ++It)
    {
        if (It && It->GetName() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Find the mesh component by name.
    UMeshComponent* TargetComponent = nullptr;
    TArray<UMeshComponent*> MeshComps;
    TargetActor->GetComponents<UMeshComponent>(MeshComps);
    for (UMeshComponent* MeshComp : MeshComps)
    {
        if (MeshComp && MeshComp->GetName().Contains(ComponentName))
        {
            TargetComponent = MeshComp;
            break;
        }
    }
    if (!TargetComponent)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Mesh component '%s' not found on actor '%s'"), *ComponentName, *ActorName));
    }

    // Load the material asset.
    UMaterialInterface* MaterialAsset = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    if (!MaterialAsset)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
    }

    // Assign the material to the component slot.
    TargetComponent->SetMaterial(MaterialIndex, MaterialAsset);

    TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
    ResultData->SetStringField(TEXT("actor_name"), ActorName);
    ResultData->SetStringField(TEXT("component_name"), TargetComponent->GetName());
    ResultData->SetNumberField(TEXT("material_index"), MaterialIndex);
    ResultData->SetStringField(TEXT("material_path"), MaterialPath);

    return FUnrealMCPCommonUtils::CreateSuccessResponse(ResultData);
}
