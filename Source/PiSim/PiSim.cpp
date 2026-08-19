// Fill out your copyright notice in the Description page of Project Settings.

#include "PiSim.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "ToolMenus.h"
#include "PropertyEditorModule.h"
#include "DetailLayoutBuilder.h"
#include "IDetailCustomization.h"
#include "PiSimGarageRobot.h"
#include "PiSimPrimitiveCube.h"
#include "BlueprintEditorModule.h"
#include "BlueprintEditor.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "Kismet2/BlueprintEditorUtils.h"

class FPiSimRobotDetailCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FPiSimRobotDetailCustomization());
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
	{
		// ❌ Hide Lighting and other non-simulation categories from Details Panel!
		DetailBuilder.HideCategory("Lighting");
		DetailBuilder.HideCategory("Rendering");
		DetailBuilder.HideCategory("HLOD");
		DetailBuilder.HideCategory("Navigation");
		DetailBuilder.HideCategory("Replication");
		DetailBuilder.HideCategory("Input");
		DetailBuilder.HideCategory("ActorTick");
		DetailBuilder.HideCategory("LOD");
		DetailBuilder.HideCategory("Cooking");
		DetailBuilder.HideCategory("DataLayers");
		DetailBuilder.HideCategory("WorldPartition");
	}
};
#endif

void FPiSimModule::StartupModule()
{
#if WITH_EDITOR
	if (GIsEditor && !IsRunningCommandlet())
	{
		FCoreDelegates::OnPostEngineInit.AddRaw(this, &FPiSimModule::CustomizeEditorToolbarsAndMenus);
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPiSimModule::CustomizeEditorToolbarsAndMenus));

		// If ToolMenus is already initialized, run customization immediately!
		if (UToolMenus::Get())
		{
			CustomizeEditorToolbarsAndMenus();
		}

		// Register Custom Details Layout for APiSimGarageRobot
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(
			APiSimGarageRobot::StaticClass()->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FPiSimRobotDetailCustomization::MakeInstance)
		);

		// Block Graph Editor Tab for APiSimPrimitiveCube and all its subclasses!
		auto RegisterBlueprintTabBlocker = []()
		{
			if (FModuleManager::Get().IsModuleLoaded("Kismet"))
			{
				FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::GetModuleChecked<FBlueprintEditorModule>("Kismet");
				BlueprintEditorModule.OnRegisterTabsForEditor().AddLambda([](FWorkflowAllowedTabSet& TabSet, FName ModeName, TSharedPtr<FBlueprintEditor> BlueprintEditor)
				{
					if (!BlueprintEditor.IsValid()) return;

					UBlueprint* TargetBP = BlueprintEditor->GetBlueprintObj();
					if (TargetBP && TargetBP->ParentClass && TargetBP->ParentClass->IsChildOf(APiSimPrimitiveCube::StaticClass()))
					{
						// Strip and purge all graph pages so NO nodes can ever exist!
						TargetBP->UbergraphPages.Empty();
						TargetBP->FunctionGraphs.Empty();
						TargetBP->MacroGraphs.Empty();
						TargetBP->DelegateSignatureGraphs.Empty();
						TargetBP->LastEditedDocuments.Empty();

						static const FName GraphTabID("GraphEditor");
						TabSet.UnregisterFactory(GraphTabID);
					}
				});
			}
		};

		if (FModuleManager::Get().IsModuleLoaded("Kismet"))
		{
			RegisterBlueprintTabBlocker();
		}
		else
		{
			FModuleManager::Get().OnModulesChanged().AddLambda([RegisterBlueprintTabBlocker](FName InModuleName, EModuleChangeReason InReason)
			{
				if (InModuleName == "Kismet" && InReason == EModuleChangeReason::ModuleLoaded)
				{
					RegisterBlueprintTabBlocker();
				}
			});
		}
	}
#endif
}

void FPiSimModule::ShutdownModule()
{
#if WITH_EDITOR
	if (UToolMenus::Get())
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout(APiSimGarageRobot::StaticClass()->GetFName());
	}
#endif
}

#if WITH_EDITOR
void FPiSimModule::CustomizeEditorToolbarsAndMenus()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus) return;

	UE_LOG(LogTemp, Warning, TEXT(">>> [PISIM TOOLMENUS] SADELESTIRME UYGULANIYOR! <<<"));

	// 1. Viewport Toolbar: Hide Left Tools (Transform/Snapping/Gizmos) & Right Camera/Settings (Keep ONLY ViewModes)
	UToolMenu* VpToolbarLeft = ToolMenus->ExtendMenu("LevelEditor.ViewportToolbar.Left");
	if (VpToolbarLeft)
	{
		VpToolbarLeft->RemoveSection("Transform");
		VpToolbarLeft->RemoveSection("Snapping");
		VpToolbarLeft->RemoveSection("CoordinateSystem");
	}

	UToolMenu* VpToolbarRight = ToolMenus->ExtendMenu("LevelEditor.ViewportToolbar.Right");
	if (VpToolbarRight)
	{
		VpToolbarRight->RemoveSection("Camera");
		VpToolbarRight->RemoveSection("CameraSpeed");
		VpToolbarRight->RemoveSection("Performance");
		VpToolbarRight->RemoveSection("Settings");
		VpToolbarRight->RemoveSection("Layout");
	}

	// 2. Content Browser Add New / Right-Click Context Menu:
	// Remove Niagara from basic assets & hide all sub-categories below Material!
	const FName MenuNames[] = {
		"ContentBrowser.AddNewContextMenu",
		"ContentBrowser.FolderContextMenu",
		"ContentBrowser.AssetContextMenu"
	};

	for (const FName& MenuName : MenuNames)
	{
		UToolMenu* TargetMenu = ToolMenus->ExtendMenu(MenuName);
		if (!TargetMenu) continue;

		// Remove Niagara from CreateBasicAssets section
		FToolMenuSection* BasicSection = TargetMenu->FindSection("CreateBasicAssets");
		if (BasicSection)
		{
			BasicSection->Blocks.RemoveAll([](const FToolMenuEntry& Entry) {
				return Entry.Name == "NiagaraSystem" || Entry.Name == "NiagaraEmitter" || Entry.Name == "Niagara";
			});
		}

		// Remove all category sub-menus below Material
		const TCHAR* CategoriesToRemove[] = {
			TEXT("Animation"),
			TEXT("Artificial Intelligence"),
			TEXT("Audio"),
			TEXT("Blueprint"),
			TEXT("Cinematics"),
			TEXT("Editor Utilities"),
			TEXT("Foliage"),
			TEXT("FX"),
			TEXT("Gameplay"),
			TEXT("Input"),
			TEXT("Interchange"),
			TEXT("Media"),
			TEXT("Miscellaneous"),
			TEXT("Paper2D"),
			TEXT("Physics"),
			TEXT("Texture"),
			TEXT("Tool Presets"),
			TEXT("User Interface"),
			TEXT("World")
		};

		for (const TCHAR* Cat : CategoriesToRemove)
		{
			TargetMenu->RemoveSection(FName(Cat));
		}
	}
}
#endif

IMPLEMENT_PRIMARY_GAME_MODULE( FPiSimModule, PiSim, "PiSim" );
