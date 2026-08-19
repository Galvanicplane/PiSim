// PiSimGarageRobot.h
// Pawn representing a dynamic garage robot with CAD 3D Mesh loading, Convex Hull collisions, SpaceX Configurator Camera, and Virtual Port bindings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PiSimConfigManager.h"
#include "PiSimUDPManager.h"
#include "PiSimGarageRobot.generated.h"

USTRUCT(BlueprintType)
struct FPiSimJointLimits
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Joint")
    float MinAngle = -90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Joint")
    float MaxAngle = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Joint")
    float CurrentAngle = 0.0f;

    /** 3D Euler Angles (Roll X, Pitch Y, Yaw Z) for Min State (-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Joint")
    FVector MinAngles3D = FVector(-45.0f, 0.0f, 0.0f);

    /** 3D Euler Angles (Roll X, Pitch Y, Yaw Z) for Zero State (0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Joint")
    FVector ZeroAngles3D = FVector(0.0f, 0.0f, 0.0f);

    /** 3D Euler Angles (Roll X, Pitch Y, Yaw Z) for Max State (+1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Joint")
    FVector MaxAngles3D = FVector(45.0f, 0.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Joint")
    FVector RotationAxis = FVector(0.0f, 1.0f, 0.0f);


    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Joint")
    FString RotationAxisName = TEXT("Y");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Joint")
    bool bInvertAxis = false;
};

USTRUCT(BlueprintType)
struct FGLBMeshSection
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GLTF")
    FString MeshName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GLTF")
    int32 ParentSectionIndex = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GLTF")
    int32 DepthLevel = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GLTF")
    FVector PivotPoint = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GLTF")
    TArray<FVector> Vertices;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GLTF")
    TArray<int32> Triangles;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GLTF")
    TArray<FVector> Normals;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Physics")
    float MassKg = -1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Physics")
    float Friction = -1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Physics")
    FString JointType = TEXT("");
};

UCLASS()
class PISIM_API APiSimGarageRobot : public APawn


{
    GENERATED_BODY()
    
public:    
    APiSimGarageRobot();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:    
    virtual void Tick(float DeltaTime) override;

    /** Root Static Mesh Component for the Robot Chassis */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Garage")
    UStaticMeshComponent* ChassisMeshComponent;

    /** Primary Physics Collision Skeletal Mesh Component (robot_collision.fbx) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Skeletal")
    USkeletalMeshComponent* CollisionSkeletalMeshComponent;

    /** Visual High-Poly Skeletal Mesh Component (robot_visual.fbx - Syncs pose with Leader Component) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Skeletal")
    USkeletalMeshComponent* VisualSkeletalMeshComponent;

    /** SpaceX Configurator Spring Arm Component for 360 Camera Orbit */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Camera")
    USpringArmComponent* ConfiguratorSpringArm;

    /** SpaceX Configurator Orbit Camera Component */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Camera")
    UCameraComponent* ConfiguratorOrbitCamera;

    /** Array of Independent Procedural Mesh Components for each CAD Sub-Mesh */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Garage")
    TArray<class UProceduralMeshComponent*> SubMeshComponents;

    /** Array of Sub-Mesh Names matching the GLB hierarchy */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Garage")
    TArray<FString> SubMeshNames;

    /** Array of Original Unrotated Base Vertices & Normals per CAD Sub-Mesh */
    TArray<TArray<FVector>> OriginalSubMeshVertices;
    TArray<TArray<FVector>> OriginalSubMeshNormals;

    /** Cached mesh sections loaded from FBX / GLB / PreCooked binary cache */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Garage")
    TArray<FGLBMeshSection> LoadedMeshSections;

    /** Test Cylinder/Capsule Collision component created on BeginPlay */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Test")
    class UCapsuleComponent* TestCylinderCollision = nullptr;

    /** Parent section index for each sub-mesh (-1 if root) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Garage")
    TArray<int32> ParentJointIndices;

    /** Hierarchy Depth Level for each sub-mesh (0 = Root, 1 = Child, 2 = Grandchild, etc.) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Garage")
    TArray<int32> DepthLevels;

    /** Exact Blender Joint Pivot Points read directly from GLB Node Translations */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Garage")
    TArray<FVector> JointPivotPoints;

    /** Option to flip mesh normals on GLB import (Default = false) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Garage")
    bool bFlipMeshNormals = false;

    /** Toggle normal flipping on runtime mesh */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Garage")
    void ToggleFlipMeshNormals();

    /** Joint Sweep Test Variables */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Garage")
    bool bIsSweepingJoint = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Garage")
    int32 SweepJointIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Garage")
    float SweepTimer = 0.0f;

    /** Start Min-Max Sweep Test for specified joint */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Garage")
    void StartJointMinMaxSweep(int32 JointIndex);

    /** Array of Joint Limit Settings per sub-mesh */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Garage")
    TArray<FPiSimJointLimits> JointLimitsList;

    /** 3D Robot CAD Mesh Asset (Assigned in Details Panel or Content Browser) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Garage")
    UStaticMesh* RobotMeshAsset = nullptr;

    /** Optional Primary FBX Static Mesh Asset (Native Unreal Collision) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Garage")
    UStaticMesh* FbxRobotMeshAsset = nullptr;

    /** Default Base Opaque Material for Robot Sub-Meshes */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Garage")
    UMaterialInterface* DefaultMaterial = nullptr;

    /** Translucent/Glass-effect Material for Hidden Sub-Meshes (Renders translucent glass while leaving Player Collision wireframes 100% visible!) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Garage")
    UMaterialInterface* TranslucentMaterial = nullptr;

    /** Backward-compatible InvisibleMaterial slot */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Garage")
    UMaterialInterface* InvisibleMaterial = nullptr;

    /** Currently loaded CAD model format name (FBX or GLB) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Garage")
    FString LoadedModelFormatName = TEXT("YÜKLENMEDİ");



    /** FPV Camera Capture Component */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Garage")
    USceneCaptureComponent2D* FpvCameraComponent;

    /** Render Target 2D for FPV Video */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Garage")
    UTextureRenderTarget2D* VideoRenderTarget = nullptr;

    /** Robot persistent configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Garage")
    FPiSimRobotConfig RobotConfig;

    /** Load configuration from Saved/Robots/Config/robot_config.json */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Garage")
    bool LoadConfig(const FString& ConfigFilePath = TEXT(""));

    /** Save configuration to Saved/Robots/Config/robot_config.json */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Garage")
    bool SaveConfig(const FString& ConfigFilePath = TEXT(""));

    /** CAD Model Scale Multiplier (Default 0.1 for mm->cm scale conversion) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Garage", meta = (ClampMin = "0.001", ClampMax = "1000.0"))
    float CadUnitScaleMultiplier = 0.1f;

    /** Apply dynamic convex hull collision setup to chassis mesh */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Garage")
    void SetupDynamicConvexCollision();

    /** Pre-cook FBX model into UStaticMesh with Use Complex Collision As Simple (Call in Editor or Runtime Pre-Play) */
    UFUNCTION(CallInEditor, BlueprintCallable, Category = "PiSim|Garage")
    void BakeFbxRobotToStaticMesh();


    /** Enable live camera stream transmission (Set false to eliminate GPU ReadPixels lag) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Garage")
    bool bEnableVideoStream = false;


    /** Rotate a specific sub-mesh component by angle clamped within Min/Max limits and optional axis inversion */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Garage")
    void SetJointAngleClamped(int32 SectionIndex, float TargetAngleDegrees, FVector RotationAxis = FVector(0, 1, 0), bool bInvertAxis = false);

    /** Rotate a sub-mesh by 3D Euler angles (Roll X, Pitch Y, Yaw Z in degrees) */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Garage")
    void SetJointEulerAngles(int32 SectionIndex, FVector EulerDeg);

    /** Parse runtime FBX mesh file directly from disk (Saved/Robots/Cache/robot.fbx) */
    static bool ParseFbxAllBinaryMeshes(const FString& FilePath, TArray<FGLBMeshSection>& OutSections, float ScaleMultiplier = 0.1f);




    /** Active Garage View Mode (Visual, Structural, Sensor) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Garage")
    EGarageViewMode CurrentViewMode = EGarageViewMode::Visual;

    /** Mesh Category Classification per sub-mesh */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Garage")
    TArray<EMeshCategoryType> MeshCategories;

    /** Physical & Aerodynamic properties per sub-mesh (for Structural Mode) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Garage")
    TArray<FPiSimStructuralProperties> StructuralPropsList;

    /** Configured Motors & Actuators List */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Garage")
    TArray<FPiSimMotorActuator> ActuatorsList;

    /** Configured Sensor Placeholders List (S_Cam_1, S_Gyro, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Garage")
    TArray<FPiSimSensorDetail> SensorsList;

    /** Center of Gravity (COG) Detection Status & Location */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Garage")
    bool bFoundCOG = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Garage")
    FVector COGLocation = FVector::ZeroVector;

    /** Center of Lift (COL) Detection Status & Location */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Garage")
    bool bFoundCOL = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Garage")
    FVector COLLocation = FVector::ZeroVector;

    /** Active Physics Test Mode (HoldInAir, LockRotation, FreeSim, None) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Garage")
    EPhysicsTestMode CurrentPhysicsTestMode = EPhysicsTestMode::None;

    /** Initial transform for reset */
    FVector InitialChassisLocation = FVector::ZeroVector;
    FRotator InitialChassisRotation = FRotator::ZeroRotator;

    /** Switch active view mode and update mesh visibilities */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Garage")
    void SetGarageViewMode(EGarageViewMode NewMode);

    /** Classify CAD sub-meshes based on naming prefixes (CM_, S_, COG, COL) */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Garage")
    void ClassifySubMeshes();

    /** Re-import / refresh CAD mesh geometry with CadUnitScaleMultiplier and re-run classification (Button in Details Panel) */
    UFUNCTION(CallInEditor, BlueprintCallable, Category = "PiSim|Garage")
    void ReimportCadModel();

    /** Apply test slider (-1.0 .. +1.0) to actuator and draw 3D debug force/rotation vectors */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Garage")
    void ApplyActuatorTestValue(int32 ActuatorIndex, float SliderVal);

    /** Perform line trace from S_ sensors and update hit look-at direction info */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Garage")
    void UpdateSensorRaycasts();

    /** Set active physics test mode and configure chassis physics constraints */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Garage")
    void SetPhysicsTestMode(EPhysicsTestMode TestMode);

    /** Reset robot to original spawn position and rotation */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Garage")
    void ResetRobotPose();

    /** Toggle whether a structural mesh belongs to the main chassis group */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Garage")
    void ToggleChassisGroup(int32 MeshIndex);

    /** Create a new independent motor actuator in the actuator list */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Garage")
    void AddNewMotorActuator();

    /** Associate a specific motor actuator with a target mesh index */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Garage")
    void BindSelectedMotorToMesh(int32 ActuatorIndex, int32 MeshIndex);

    /** Currently active DevKit Servo Axis (0=X, 1=Y, 2=Z) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|DevKit")
    int32 DevKitServoAxis = 1;

    /** Applied RPM around local X, Y, Z axes for selected mesh */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|DevKit")
    FVector DevKitAppliedRpm = FVector::ZeroVector;

    /** Applied Force in Newtons along local X, Y, Z axes for selected mesh */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|DevKit")
    FVector DevKitAppliedForceN = FVector::ZeroVector;

    /** DevKit Numpad 0: Toggle Servo Axis (X -> Y -> Z -> X) */
    UFUNCTION(BlueprintCallable, Category = "PiSim|DevKit")
    void ToggleDevKitServoAxis();

    /** DevKit Numpad 1, 2, 3: Set Servo position (-1.0, 0.0, 1.0) for selected mesh */
    UFUNCTION(BlueprintCallable, Category = "PiSim|DevKit")
    void SetDevKitServoPosition(int32 TargetMeshIdx, float PositionState);

    /** DevKit Numpad 4, 5, 6: Add +1 RPM around X, Y, or Z axis */
    UFUNCTION(BlueprintCallable, Category = "PiSim|DevKit")
    void AddDevKitRpm(int32 TargetMeshIdx, int32 AxisIndex, float DeltaRpm = 1.0f);

    /** DevKit Numpad 7, 8, 9: Add +10 N Force along X, Y, or Z axis */
    UFUNCTION(BlueprintCallable, Category = "PiSim|DevKit")
    void AddDevKitForceN(int32 TargetMeshIdx, int32 AxisIndex, float DeltaForceN = 10.0f);

    /** Reset all DevKit RPM and Force test values */
    UFUNCTION(BlueprintCallable, Category = "PiSim|DevKit")
    void ResetDevKitTestValues();

    /** Dynamic physics constraints connecting sub-mesh joints to parent components */
    UPROPERTY(Transient)
    TArray<UPhysicsConstraintComponent*> JointPhysicsConstraints;

    /** Initial relative rotation for sub-mesh components */
    TMap<int32, FQuat> InitialSubMeshRelativeRotations;

    /** Accumulated spin angle per sub-mesh index (degrees) */
    TMap<int32, float> SubMeshSpinAngles;

private:
    TUniquePtr<FPiSimUDPManager> UDPManager;
    float VideoTimer = 0.0f;

    void CaptureAndSendVideoFrame();
};
