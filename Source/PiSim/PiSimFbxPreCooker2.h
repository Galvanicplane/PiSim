#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PiSimFbxPreCooker2.generated.h"

/**
 * PreCooker 2: Single FBX Pre-Cooker Actor.
 * Reads model geometry from a single FBX file (rbot1.fbx) with Auto Generate Collision DISABLED.
 */
UCLASS(BlueprintType, Blueprintable)
class PISIM_API APiSimFbxPreCooker2 : public AActor
{
    GENERATED_BODY()

public:
    APiSimFbxPreCooker2();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|PreCooker 2")
    USceneComponent* SceneRoot;

    /** Target FBX filename in Saved/Robots/Cache/ (Default: rbot1.fbx) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|PreCooker 2")
    FString SourceFbxFileName = TEXT("rbot1.fbx");

    /** Auto Generate Collision flag (Default: False / Disabled as requested) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|PreCooker 2")
    bool bAutoGenerateCollision = false;

    /** Scale Multiplier applied to FBX geometry (Default: 1.0f, change to 10.0 or 100.0 if exported in millimeters/meters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|PreCooker 2", meta = (ClampMin = "0.001", ClampMax = "1000.0"))
    float ImportScale = 1.0f;

    /** Optional custom full path if file is outside Saved/Robots/Cache/ */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|PreCooker 2")
    FString CustomFbxPath = TEXT("");

    /** Button in Details Panel to bake rbot1.fbx directly */
    UFUNCTION(CallInEditor, Category = "PiSim|PreCooker 2")
    void BakeSingleFbx();

    /** Standalone Bake function for single FBX file */
    UFUNCTION(BlueprintCallable, Category = "PiSim|PreCooker 2")
    bool BakeSingleFbxFile(const FString& FbxFilePath);
};
