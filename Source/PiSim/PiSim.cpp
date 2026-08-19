// Fill out your copyright notice in the Description page of Project Settings.

#include "PiSim.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "ToolMenus.h"
#endif

void FPiSimModule::StartupModule()
{
#if WITH_EDITOR
	if (GIsEditor && !IsRunningCommandlet())
	{
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPiSimModule::CustomizeEditorToolbarsAndMenus));
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
#endif
}

#if WITH_EDITOR
void FPiSimModule::CustomizeEditorToolbarsAndMenus()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus) return;

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
	UToolMenu* AddNewMenu = ToolMenus->ExtendMenu("ContentBrowser.AddNewContextMenu");
	if (AddNewMenu)
	{
		// Remove Niagara from CreateBasicAssets section
		FToolMenuSection* BasicSection = AddNewMenu->FindSection("CreateBasicAssets");
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
			AddNewMenu->RemoveSection(FName(Cat));
		}
	}
}
#endif

IMPLEMENT_PRIMARY_GAME_MODULE( FPiSimModule, PiSim, "PiSim" );
