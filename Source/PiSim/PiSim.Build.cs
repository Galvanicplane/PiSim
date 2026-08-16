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

		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
