// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class PiSim : ModuleRules
{
	public PiSim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = true;

		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"Sockets", 
			"Networking", 
			"RenderCore", 
			"RHI", 
			"ImageWrapper",
			"Json",
			"JsonUtilities",
			"ProceduralMeshComponent",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PublicIncludePaths.AddRange(new string[] { 
			ModuleDirectory,
			System.IO.Path.Combine(ModuleDirectory, "1_Vehicle"),
			System.IO.Path.Combine(ModuleDirectory, "2_Motors"),
			System.IO.Path.Combine(ModuleDirectory, "3_Sensors"),
			System.IO.Path.Combine(ModuleDirectory, "4_Communication"),
			System.IO.Path.Combine(ModuleDirectory, "5_Configurators"),
			System.IO.Path.Combine(ModuleDirectory, "6_UI"),
			System.IO.Path.Combine(ModuleDirectory, "7_Core")
		});

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] {
				"UnrealEd",
				"ToolMenus",
				"ContentBrowser",
				"LevelEditor",
				"PropertyEditor",
				"BlueprintGraph",
				"Kismet"
			});
		}
	}
}
