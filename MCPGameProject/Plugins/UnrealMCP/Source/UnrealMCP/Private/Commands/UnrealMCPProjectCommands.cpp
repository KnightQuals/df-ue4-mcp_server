#include "Commands/UnrealMCPProjectCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "GameFramework/InputSettings.h"
#include "Engine/DataTable.h"
#include "UObject/StructOnScope.h"
#include "JsonObjectConverter.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/DataTableFactory.h"
#include "EditorAssetLibrary.h"
#include "Misc/PackageName.h"

FUnrealMCPProjectCommands::FUnrealMCPProjectCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_input_mapping"))
    {
        return HandleCreateInputMapping(Params);
    }
    else if (CommandType == TEXT("create_data_table"))
    {
        return HandleCreateDataTable(Params);
    }
    else if (CommandType == TEXT("add_row"))
    {
        return HandleAddRow(Params);
    }
    else if (CommandType == TEXT("read_row"))
    {
        return HandleReadRow(Params);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown project command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCreateInputMapping(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActionName;
    if (!Params->TryGetStringField(TEXT("action_name"), ActionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'action_name' parameter"));
    }

    FString Key;
    if (!Params->TryGetStringField(TEXT("key"), Key))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'key' parameter"));
    }

    // Get the input settings
    UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();
    if (!InputSettings)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get input settings"));
    }

    // Create the input action mapping
    FInputActionKeyMapping ActionMapping;
    ActionMapping.ActionName = FName(*ActionName);
    ActionMapping.Key = FKey(*Key);

    // Add modifiers if provided
    if (Params->HasField(TEXT("shift")))
    {
        ActionMapping.bShift = Params->GetBoolField(TEXT("shift"));
    }
    if (Params->HasField(TEXT("ctrl")))
    {
        ActionMapping.bCtrl = Params->GetBoolField(TEXT("ctrl"));
    }
    if (Params->HasField(TEXT("alt")))
    {
        ActionMapping.bAlt = Params->GetBoolField(TEXT("alt"));
    }
    if (Params->HasField(TEXT("cmd")))
    {
        ActionMapping.bCmd = Params->GetBoolField(TEXT("cmd"));
    }

    // Add the mapping
    InputSettings->AddActionMapping(ActionMapping);
    InputSettings->SaveConfig();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("action_name"), ActionName);
    ResultObj->SetStringField(TEXT("key"), Key);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCreateDataTable(const TSharedPtr<FJsonObject>& Params)
{
    // Create a new UDataTable asset at 'asset_path' using the row struct at 'struct_path'.
    // e.g. asset_path="/Game/Config/DT_MatchConfig", struct_path="/Game/Config/ST_MatchConfig"
    FString AssetPath, StructPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("struct_path"), StructPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'struct_path' parameter"));
    }

    UScriptStruct* RowStruct = Cast<UScriptStruct>(LoadObject<UObject>(nullptr, *StructPath));
    if (!RowStruct)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Row struct not found: %s"), *StructPath));
    }

    // Previous implementation used GetTransientPackage(), which returned success but
    // created an unsaved, unfindable table. Create the asset through AssetTools so it
    // appears under Content Browser and can be loaded by GameMode after an editor restart.
    const FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
    const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);
    if (PackagePath.IsEmpty() || AssetName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid asset_path: %s"), *AssetPath));
    }

    UDataTable* NewTable = LoadObject<UDataTable>(nullptr, *FString::Printf(TEXT("%s.%s"), *AssetPath, *AssetName));
    if (!NewTable)
    {
        UDataTableFactory* Factory = NewObject<UDataTableFactory>();
        Factory->Struct = RowStruct;
        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
        NewTable = Cast<UDataTable>(AssetToolsModule.Get().CreateAsset(AssetName, PackagePath, UDataTable::StaticClass(), Factory));
    }

    if (!NewTable)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create persistent UDataTable asset"));
    }

    NewTable->RowStruct = RowStruct;
    NewTable->MarkPackageDirty();
    if (!UEditorAssetLibrary::SaveLoadedAsset(NewTable, false))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("DataTable created but failed to save: %s"), *AssetPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("asset_path"), AssetPath);
    ResultObj->SetStringField(TEXT("struct_path"), StructPath);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleAddRow(const TSharedPtr<FJsonObject>& Params)
{
    // Add a row to an existing DataTable.
    // 'table_path' = DataTable asset path, 'row_name' = row key, 'row_data' = JSON object of struct fields.
    FString TablePath, RowName;
    if (!Params->TryGetStringField(TEXT("table_path"), TablePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'table_path' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("row_name"), RowName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'row_name' parameter"));
    }
    const TSharedPtr<FJsonObject>* RowDataPtr = nullptr;
    if (!Params->TryGetObjectField(TEXT("row_data"), RowDataPtr) || !RowDataPtr)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'row_data' parameter"));
    }

    UDataTable* Table = LoadObject<UDataTable>(nullptr, *TablePath);
    if (!Table || !Table->RowStruct)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("DataTable not found or has no RowStruct: %s"), *TablePath));
    }

    // Build a struct instance from JSON via JsonObjectConverter.
    uint8* RowData = (uint8*)FMemory::Malloc(Table->RowStruct->GetStructureSize());
    Table->RowStruct->InitializeStruct(RowData);
    FStructOnScope StructOnScope(Table->RowStruct, RowData);
    // JsonObjectToUStruct takes a TSharedRef<FJsonObject>; convert from the TSharedPtr.
    if (!FJsonObjectConverter::JsonObjectToUStruct(RowDataPtr->ToSharedRef(), Table->RowStruct, RowData, 0, 0))
    {
        FMemory::Free(RowData);
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to convert row_data JSON to struct"));
    }
    Table->AddRow(FName(*RowName), *reinterpret_cast<FTableRowBase*>(RowData));
    FMemory::Free(RowData);
    Table->MarkPackageDirty();
    if (!UEditorAssetLibrary::SaveLoadedAsset(Table, false))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Row added but failed to save DataTable: %s"), *TablePath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("table_path"), TablePath);
    ResultObj->SetStringField(TEXT("row_name"), RowName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleReadRow(const TSharedPtr<FJsonObject>& Params)
{
    // Read a row from a DataTable and return it as JSON.
    FString TablePath, RowName;
    if (!Params->TryGetStringField(TEXT("table_path"), TablePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'table_path' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("row_name"), RowName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'row_name' parameter"));
    }

    UDataTable* Table = LoadObject<UDataTable>(nullptr, *TablePath);
    if (!Table || !Table->RowStruct)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("DataTable not found or has no RowStruct: %s"), *TablePath));
    }

    uint8* RowData = Table->FindRowUnchecked(FName(*RowName));
    if (!RowData)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Row not found: %s"), *RowName));
    }

    TSharedPtr<FJsonObject> RowJson = MakeShared<FJsonObject>();
    // UStructToJsonObject takes a TSharedRef<FJsonObject> for the output.
    FJsonObjectConverter::UStructToJsonObject(Table->RowStruct, RowData, RowJson.ToSharedRef(), 0, 0);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("table_path"), TablePath);
    ResultObj->SetStringField(TEXT("row_name"), RowName);
    ResultObj->SetObjectField(TEXT("row_data"), RowJson);
    return ResultObj;
} 