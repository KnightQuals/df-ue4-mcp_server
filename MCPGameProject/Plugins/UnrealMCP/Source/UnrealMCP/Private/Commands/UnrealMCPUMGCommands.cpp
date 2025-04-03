#include "Commands/UnrealMCPUMGCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"
#include "Factories/Factory.h"
#include "UMGEditorSubsystem.h"
#include "WidgetBlueprintEditor.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

FUnrealMCPUMGCommands::FUnrealMCPUMGCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_umg_widget_blueprint"))
    {
        return HandleCreateUMGWidgetBlueprint(Params);
    }
    else if (CommandType == TEXT("add_text_block_to_widget"))
    {
        return HandleAddTextBlockToWidget(Params);
    }
    else if (CommandType == TEXT("add_widget_to_viewport"))
    {
        return HandleAddWidgetToViewport(Params);
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown UMG command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleCreateUMGWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Create the full asset path
    FString PackagePath = TEXT("/Game/Widgets/");
    FString AssetName = BlueprintName;
    FString FullPath = PackagePath + AssetName;

    // Check if asset already exists
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' already exists"), *BlueprintName));
    }

    // Create Widget Blueprint
    UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
    UPackage* Package = CreatePackage(*FullPath);
    UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Factory->FactoryCreateNew(UWidgetBlueprint::StaticClass(), Package, *AssetName, RF_Standalone | RF_Public, nullptr, GWarn));

    if (!WidgetBlueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Widget Blueprint"));
    }

    // Add a default Canvas Panel if one doesn't exist
    if (!WidgetBlueprint->WidgetTree->RootWidget)
    {
        UCanvasPanel* RootCanvas = WidgetBlueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
        WidgetBlueprint->WidgetTree->RootWidget = RootCanvas;
    }

    // Mark the package dirty and notify asset registry
    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(WidgetBlueprint);

    // Compile the blueprint
    FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

    // Create success response
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("name"), BlueprintName);
    ResultObj->SetStringField(TEXT("path"), FullPath);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddTextBlockToWidget(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString WidgetName;
    if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
    }

    // Find the Widget Blueprint
    FString FullPath = TEXT("/Game/Widgets/") + BlueprintName;
    UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(FullPath));
    if (!WidgetBlueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *BlueprintName));
    }

    // Get optional parameters
    FString InitialText = TEXT("New Text Block");
    Params->TryGetStringField(TEXT("text"), InitialText);

    FVector2D Position(0.0f, 0.0f);
    if (Params->HasField(TEXT("position")))
    {
        const TArray<TSharedPtr<FJsonValue>>* PosArray;
        if (Params->TryGetArrayField(TEXT("position"), PosArray) && PosArray->Num() >= 2)
        {
            Position.X = (*PosArray)[0]->AsNumber();
            Position.Y = (*PosArray)[1]->AsNumber();
        }
    }

    // Create Text Block widget
    UTextBlock* TextBlock = WidgetBlueprint->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *WidgetName);
    if (!TextBlock)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Text Block widget"));
    }

    // Set initial text
    TextBlock->SetText(FText::FromString(InitialText));

    // Add to canvas panel
    UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetBlueprint->WidgetTree->RootWidget);
    if (!RootCanvas)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Root Canvas Panel not found"));
    }

    UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(TextBlock);
    PanelSlot->SetPosition(Position);

    // Mark the package dirty and compile
    WidgetBlueprint->MarkPackageDirty();
    FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

    // Create success response
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("widget_name"), WidgetName);
    ResultObj->SetStringField(TEXT("text"), InitialText);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddWidgetToViewport(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Find the Widget Blueprint
    FString FullPath = TEXT("/Game/Widgets/") + BlueprintName;
    UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(FullPath));
    if (!WidgetBlueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *BlueprintName));
    }

    // Get optional Z-order parameter
    int32 ZOrder = 0;
    Params->TryGetNumberField(TEXT("z_order"), ZOrder);

    // Create widget instance
    UClass* WidgetClass = WidgetBlueprint->GeneratedClass;
    if (!WidgetClass)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get widget class"));
    }

    // Note: This creates the widget but doesn't add it to viewport
    // The actual addition to viewport should be done through Blueprint nodes
    // as it requires a game context

    // Create success response with instructions
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetStringField(TEXT("class_path"), WidgetClass->GetPathName());
    ResultObj->SetNumberField(TEXT("z_order"), ZOrder);
    ResultObj->SetStringField(TEXT("note"), TEXT("Widget class ready. Use CreateWidget and AddToViewport nodes in Blueprint to display in game."));
    return ResultObj;
} 