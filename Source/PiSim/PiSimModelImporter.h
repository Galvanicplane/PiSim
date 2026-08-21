// PiSimModelImporter.h
// Clean, Dedicated FBX Importer with strict Visual vs UCX Collision Separation, Hierarchy Linking, Scale Control, and Chaos Physics Toggle.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PiSimModelImporter.generated.h"

USTRUCT(BlueprintType)
struct FImporterMeshSection
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Mesh")
    FString MeshName = TEXT("");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Mesh")
    int32 ParentSectionIndex = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Mesh")
    int32 DepthLevel = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Mesh")
    FVector PivotPoint = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Mesh")
    TArray<FVector> Vertices;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Mesh")
    TArray<int32> Triangles;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Mesh")
    TArray<FVector> Normals;

    // Physics & Joint properties
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Physics")
    float MassKg = 2.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Physics")
    float Friction = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Joint")
    FVector RotationAxis = FVector(0.0f, 1.0f, 0.0f); // Default Y-axis axle rotation

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Joint")
    float MinAngle = -90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Joint")
    float MaxAngle = 90.0f;
};

UCLASS()
class PISIM_API APiSimModelImporter : public APawn
{
    GENERATED_BODY()

public:
    APiSimModelImporter();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    // =========================================================================
    // COMPONENTS
    // =========================================================================
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Components")
    USceneComponent* SceneRootComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Components")
    USpringArmComponent* OrbitSpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Components")
    UCameraComponent* OrbitCamera;

    // =========================================================================
    // SPAWNED SCENE MESH COMPONENTS
    // =========================================================================
    // 1) Visual Procedural Mesh Components (Pure Render, Clean 3D Mesh)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Visual")
    TArray<UProceduralMeshComponent*> VisualMeshComponents;

    // 2) UCX Collision Mesh Components (Pure Physics, Invisible Render)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Collision")
    TArray<UProceduralMeshComponent*> CollisionMeshComponents;

    // Joint physics constraints between Chassis and Wheels
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Physics")
    TArray<UPhysicsConstraintComponent*> JointConstraints;

    // =========================================================================
    // SEPARATED PARSED FBX DATA LISTS
    // =========================================================================
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Data")
    TArray<FImporterMeshSection> VisualSections;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Data")
    TArray<FImporterMeshSection> UCXSections;

    // =========================================================================
    // CONTROLS & SETTINGS (Clean 1.0f 1:1 Scale by default)
    // =========================================================================
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Settings")
    float ImportScaleMultiplier = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Settings")
    bool bIsPhysicsSimulating = false;

    // =========================================================================
    // CALL-IN-EDITOR BUTTONS (Details Panel)
    // =========================================================================
    UFUNCTION(CallInEditor, Category = "PiSim|Actions")
    void ImportAndSpawnRobot();

    UFUNCTION(CallInEditor, Category = "PiSim|Actions")
    void SetScale_0_1X();

    UFUNCTION(CallInEditor, Category = "PiSim|Actions")
    void SetScale_1_0X();

    UFUNCTION(CallInEditor, Category = "PiSim|Actions")
    void SetScale_10_0X();

    UFUNCTION(CallInEditor, Category = "PiSim|Actions")
    void TogglePhysicsSimulation();

    // =========================================================================
    // CORE PIPELINE FUNCTIONS
    // =========================================================================
    /** Parses Saved/Robots/Cache/robot_import_test.fbx into distinct VisualSections and UCXSections */
    static bool ParseBinaryFbxFile(const FString& FilePath, TArray<FImporterMeshSection>& OutVisual, TArray<FImporterMeshSection>& OutUCX, float Scale);

    /** Spawns and links both Visual and UCX meshes hierarchically with bone attachments and collisions */
    void BuildAndSpawnRobotHierarchy(float Scale);

    /** Activates or disables live Chaos physics simulation and gravity */
    void SetPhysicsSimulationActive(bool bActive);

private:
    void ClearSpawnedComponents();
};
