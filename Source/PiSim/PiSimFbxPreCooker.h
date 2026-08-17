#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PiSimFbxPreCooker.generated.h"

/**
 * Lightweight Editor Pre-Cooker Actor.
 * Place this Actor in the Level to bake robot.fbx into Use Complex Collision As Simple BEFORE PRESSING PLAY!
 */
UCLASS(BlueprintType, Blueprintable)
class PISIM_API APiSimFbxPreCooker : public AActor
{
    GENERATED_BODY()

public:
    APiSimFbxPreCooker();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|PreCooker")
    USceneComponent* SceneRoot;

    /** Button in Details Panel to bake Saved/Robots/Cache/robot_collision.fbx & robot_visual.fbx */
    UFUNCTION(CallInEditor, Category = "PiSim|PreCooker")
    void BakeFbxRobotToStaticMesh();

    /** Standalone Runtime Bake function (Runs in packaged cloud builds without Unreal Editor) */
    UFUNCTION(BlueprintCallable, Category = "PiSim|PreCooker")
    bool BakeTwoFbxRobotFiles(const FString& CollisionFbxPath, const FString& VisualFbxPath);
};
