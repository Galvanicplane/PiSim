// PiSimModelImporterBase.h
// Base Pawn class providing common FBX Parsing, Orbit Camera, Strict Visual vs UCX Collision,
// Sensor Sockets, and Root Bone Domain Dispatching (Land, Sea, Air).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "ProceduralMeshComponent.h"
#include "PiSimModelImporter.h" // For shared enums and structs
#include "PiSimActuatorComponent.h"
#include "PiSimAeroWingComponent.h"
#include "PiSimModelImporterBase.generated.h"

class UPiSimModelImporterWidget;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UPiSimUDPManager;

// EVehicleDomain is defined in PiSimModelImporter.h

UCLASS()
class PISIM_API APiSimModelImporterBase : public APawn
{
    GENERATED_BODY()

public:
    APiSimModelImporterBase();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // --- Vehicle Domain Classification ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Domain")
    EVehicleDomain VehicleDomain = EVehicleDomain::Unknown;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Domain")
    FString DetectedRootBoneName = TEXT("");

    /** Kök kemik adına göre aracın sınıfını belirler */
    static EVehicleDomain DetectVehicleDomain(const FString& RootBoneName);

    // --- Core Scene Components ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Components")
    USceneComponent* SceneRootComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Camera")
    USpringArmComponent* OrbitSpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Camera")
    UCameraComponent* OrbitCamera;

    // --- Mesh & Collision Geometry Collections ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Mesh")
    TArray<UProceduralMeshComponent*> VisualMeshComponents;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Mesh")
    TArray<UProceduralMeshComponent*> CollisionMeshComponents;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Mesh")
    TArray<UProceduralMeshComponent*> SensorMarkerComponents;

    TArray<FImporterMeshSection> VisualSections;
    TArray<FImporterMeshSection> UCXSections;
    TArray<FImporterSensorSection> SensorSections;
    TArray<FString> PureBoneNames;

    // --- Modular Socket / Component Collections ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Actuators")
    TArray<UPiSimActuatorComponent*> AttachedActuators;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Aero")
    TArray<UPiSimAeroWingComponent*> AttachedWingBodies;

    // --- Camera & Video Streaming ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Sensors")
    USceneCaptureComponent2D* FpvCameraCapture = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Sensors")
    TArray<USceneCaptureComponent2D*> SpawnedCameraComponents;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Sensors")
    UTextureRenderTarget2D* VideoRenderTarget = nullptr;

    // --- Telemetry & Networking ---
    TUniquePtr<class FPiSimUDPManager> UDPManager;

    // --- State Flags ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Physics")
    bool bIsPhysicsSimulating = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Model")
    FString ActiveModelFileName = TEXT("CurrentModel.fbx");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Model")
    float ActiveScaleMultiplier = 1.0f;

    // --- Virtual Domain Interface (Override in Land, Sea, Air) ---
    virtual void SetupDomainComponents();
    virtual void ApplyDomainPhysics(float DeltaTime);

    // --- Common FBX & Hierarchy API ---
    void ClearSpawnedComponents();
    void BuildAndSpawnRobotHierarchy(float Scale);
    bool ParseBinaryFbxFile(const FString& FilePath, TArray<FImporterMeshSection>& OutVisual, TArray<FImporterMeshSection>& OutUCX, TArray<FImporterSensorSection>& OutSensors, TArray<FString>& OutPureBones, float Scale);

    // Camera Orbit Controls
    void OnLeftMouseDown();
    void OnLeftMouseUp();
    void OnRightMouseDown();
    void OnRightMouseUp();
    void ZoomIn();
    void ZoomOut();

protected:
    bool bIsOrbiting = false;
    bool bIsPanning = false;
    FVector2D PreviousMousePosition;
};
