// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class PiSimTarget : TargetRules
{
	public PiSimTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;

		bUsePCHFiles = true;
		bUseUnityBuild = false; // Fast incremental Live Coding & single file compile

		ExtraModuleNames.AddRange( new string[] { "PiSim" } );
	}
}
