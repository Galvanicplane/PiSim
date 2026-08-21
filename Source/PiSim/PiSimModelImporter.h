// PiSimModelImporter.h
// Full-Featured Pawn for GameMode with 360 Orbit Camera, Mouse Controls, Interactive Screen UI, and Strict Visual vs UCX Collision Separation.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PiSimModelImporter.generated.h"

class UPiSimModelImporterWidget;

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

    // Heavy vertex/triangle arrays excluded from PropertyEditor reflection to prevent Editor freezes
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
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
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
    virtual void Tick(float DeltaTime) override;

    // =========================================================================
    // COMPONENTS & SPACEX 360 ORBIT CAMERA
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
    // SEPARATED PARSED FBX DATA LISTS (Transient to prevent lag)
    // =========================================================================
    TArray<FImporterMeshSection> VisualSections;
    TArray<FImporterMeshSection> UCXSections;

    // Active On-Screen Slate UI Widget
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|UI")
    UPiSimModelImporterWidget* ImporterWidget = nullptr;

    // =========================================================================
    // CONTROLS & SETTINGS (Clean 1.0f 1:1 Scale by default)
    // =========================================================================
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Settings")
    float ImportScaleMultiplier = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Settings")
    bool bIsPhysicsSimulating = false;

    /** Applied wheel rotation speed in RPM (Controllable via G and F keys) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Physics")
    float AppliedWheelRpm = 0.0f;

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

    // =========================================================================
    // MOUSE ORBIT & ZOOM & RPM CONTROLS
    // =========================================================================
    void OnLeftMouseDown();
    void OnLeftMouseUp();
    void OnRightMouseDown();
    void OnRightMouseUp();
    void ZoomIn();
    void ZoomOut();
    void IncreaseWheelRpm();
    void DecreaseWheelRpm();

private:
    void ClearSpawnedComponents();

    bool bIsLeftMouseDown = false;
    bool bIsRightMouseDown = false;
};
