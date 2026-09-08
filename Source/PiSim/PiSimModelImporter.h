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
    // =========================================================================
    // SEPARATED PARSED FBX DATA LISTS (Transient to prevent lag)
    // =========================================================================
    TArray<FImporterMeshSection> VisualSections;
    TArray<FImporterMeshSection> UCXSections;

    // Active On-Screen Slate UI Widget
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|UI")
    UPiSimModelImporterWidget* ImporterWidget = nullptr;

    // =========================================================================
    // UDP COMMUNICATION & NETWORK TELEMETRY (Raspberry Pi 5 / Edge Bridge)
    // =========================================================================
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Network")
    FString BridgeTargetIP = TEXT("192.168.1.20"); // Raspberry Pi 5 Ethernet IP (Proven Working Default)

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Network")
    bool bIsPiConnected = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Network")
    bool bIsSocketBound = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Network")
    FString ConnectedPiIP = TEXT("127.0.0.1");

    /** Connection Stage (1: Soket Açık, 2: Hedef Hazır, 3: Pi5 Bekleniyor, 4: Bağlandı & Veri Akışı, 5: Zaman Aşımı) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Network")
    int32 ConnectionStage = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Network")
    FString ConnectionStageText = TEXT("Aşama 1: Soket Başlatılıyor...");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Network")
    int32 TotalPacketsReceived = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Network")
    int32 TotalPacketsSent = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Network")
    float RxPacketRateHz = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Network")
    float TxPacketRateHz = 0.0f;

    // Detailed GELEN VERİLER (RX from Pi 5)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Control")
    int32 LastRxPacketBytes = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Control")
    float LastRxTimestampSec = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Control")
    FVector LastRxLinearVel = FVector::ZeroVector; // m/s (Linear X, Y, Z)

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Control")
    FVector LastRxAngularVel = FVector::ZeroVector; // rad/s (Angular X, Y, Z)

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Control")
    float TargetLinearX = 0.0f; // m/s (from Pi 5 cmd_vel)

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Control")
    float TargetAngularZ = 0.0f; // rad/s (from Pi 5 cmd_vel)

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Control")
    float LeftWheelsRpm = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Control")
    float RightWheelsRpm = 0.0f;

    // Detailed GİDEN VERİLER (TX to Pi 5)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Telemetry")
    int32 LastTxPacketBytes = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Telemetry")
    float CurrentForwardSpeedKmh = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Telemetry")
    FVector CurrentLinearAccel = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Telemetry")
    FVector LastTxAccel = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Telemetry")
    FVector LastTxGyro = FVector::ZeroVector; // rad/s or deg/s

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Telemetry")
    FQuat LastTxQuat = FQuat::Identity;

    /** Live bidirectional connection debug event log (Shown in HUD) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Network")
    TArray<FString> ConnectionDebugLogs;

    void AddConnectionDebugLog(const FString& LogMsg);

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

    /** Callback for incoming UDP control packets from Raspberry Pi 5 */
    void OnControlPacketReceived(const TArray<uint8>& PacketData, const FString& SenderIP);

    /** Publishes live IMU & kinematics telemetry to Raspberry Pi 5 over UDP 7401 */
    void PublishImuTelemetry(float DeltaTime);

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

    TUniquePtr<class FPiSimUDPManager> UDPManager;
    FVector PreviousLinearVelocityUE5 = FVector::ZeroVector;
    float TelemetryTimer = 0.0f;
    float RateCalcTimer = 0.0f;
    int32 RxCountInWindow = 0;
    int32 TxCountInWindow = 0;
    float LastPacketReceivedTime = -100.0f;
};
