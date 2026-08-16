// PiSimConfigManager.h
// JSON Persistence Engine for Robot Configurations and Virtual Port Mappings (PWM_0, I2C_1, etc.)

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PiSimConfigManager.generated.h"

/** Enum representing supported Virtual Hardware Protocols */
UENUM(BlueprintType)
enum class EPiSimVirtualPortType : uint8
{
    PWM      UMETA(DisplayName = "Direct PWM"),
    I2C      UMETA(DisplayName = "I2C Bus"),
    SPI      UMETA(DisplayName = "SPI Bus"),
    UART     UMETA(DisplayName = "UART Serial"),
    CAMERA   UMETA(DisplayName = "FPV Video Camera"),
    GPIO     UMETA(DisplayName = "Digital GPIO Pin")
};

/** Enum representing Garage View Modes (Visual, Structural, Sensor) */
UENUM(BlueprintType)
enum class EGarageViewMode : uint8
{
    Visual       UMETA(DisplayName = "Visual Model"),
    Structural   UMETA(DisplayName = "Structural & Physics Model"),
    Sensor       UMETA(DisplayName = "Sensor Placeholders")
};

/** Enum representing CAD Mesh Category */
UENUM(BlueprintType)
enum class EMeshCategoryType : uint8
{
    Visual       UMETA(DisplayName = "Visual Mesh"),
    Structural   UMETA(DisplayName = "Structural / Collision (CM_)"),
    Sensor       UMETA(DisplayName = "Sensor Placeholder (S_)")
};

/** Enum representing Motor / Actuator behavior types */
UENUM(BlueprintType)
enum class EMotorBehaviorType : uint8
{
    ContinuousSpin UMETA(DisplayName = "Sürekli Döndüren (Wheel/Spin)"),
    Servo          UMETA(DisplayName = "Servo / Kontrol Yüzeyi (Angle)"),
    Thruster       UMETA(DisplayName = "İtki / Pervane Kuvveti (Thrust)")
};

/** Enum representing Physics Test Modes in Garage */
UENUM(BlueprintType)
enum class EPhysicsTestMode : uint8
{
    None           UMETA(DisplayName = "Edit Mode (Test Kapalı)"),
    HoldInAir      UMETA(DisplayName = "Havada Sabit Tut (Hold In Air)"),
    LockRotation   UMETA(DisplayName = "Rotasyonu Sabit Tut (Lock Rotation)"),
    FreeSim        UMETA(DisplayName = "Hepsini Serbest Bırak (Free Sim)")
};

/** Struct representing Physical & Aerodynamic specs for a structural mesh */
USTRUCT(BlueprintType)
struct PISIM_API FPiSimStructuralProperties
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Physics")
    float MassKg = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Physics")
    float AirDragCoeff = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Physics")
    float GroundFriction = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Physics")
    float LiftCoefficient = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Physics")
    float WingAreaSqMeters = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Physics")
    float AirDensityKgM3 = 1.225f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Physics")
    bool bIsChassisGroup = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Physics")
    FString ParentGroupName = TEXT("MainChassis");
};

/** Struct defining a Motor/Actuator action on a target mesh */
USTRUCT(BlueprintType)
struct PISIM_API FPiSimMotorActuator
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    FString MotorName = TEXT("Actuator_1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    EMotorBehaviorType MotorType = EMotorBehaviorType::Servo;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    int32 AssignedPwmChannel = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    FString DriverProfileName = TEXT("Standard Servo (1000 - 2000 us)");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    float MaxTorqueNm = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    float MaxThrustNewton = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    int32 TargetMeshIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    FString TargetMeshName = TEXT("");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    bool bAffectsRotation = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    bool bAffectsLiftForce = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    FVector AppliedForceAtMin = FVector(0, 0, -50);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    FVector AppliedForceAtZero = FVector(0, 0, 0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    FVector AppliedForceAtMax = FVector(0, 0, 50);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    FVector AppliedTorqueAxis = FVector(0, 1, 0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    float TestSliderValue = 0.0f;
};

/** Struct representing Sensor Placeholder details & live raycast look-at info */
USTRUCT(BlueprintType)
struct PISIM_API FPiSimSensorDetail
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Sensor")
    FString SensorName = TEXT("S_Cam_1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Sensor")
    FString SensorType = TEXT("Camera");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Sensor")
    int32 MeshIndex = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Sensor")
    bool bSensorActive = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Sensor")
    FString LiveDataStream = TEXT("FPS: 60 | FOV: 90° | RES: 1920x1080");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Sensor")
    FVector LookAtDirection = FVector(1, 0, 0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Sensor")
    float RaycastHitDistance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Sensor")
    FString RaycastHitActorName = TEXT("None");
};

/** Struct defining a Virtual Port Mapping (e.g. PWM_0 -> LeftMotor) */
USTRUCT(BlueprintType)
struct PISIM_API FPiSimVirtualPort
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Config")
    FString PortID = TEXT("PWM_0");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Config")
    EPiSimVirtualPortType PortType = EPiSimVirtualPortType::PWM;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Config")
    FString TargetComponent = TEXT("LeftWheel");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Config")
    int32 PinOrAddress = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Config")
    int32 MinValue = 1000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Config")
    int32 MaxValue = 2000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Config")
    FString RotationAxis = TEXT("Y");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Config")
    bool bInvertAxis = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Config")
    float MinAngleLimit = -90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Config")
    float MaxAngleLimit = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Config")
    FString HardwareControllerType = TEXT("Standard Servo (1000-2000 us)");
};

/** Struct representing a complete persistent Robot Configuration */
USTRUCT(BlueprintType)
struct PISIM_API FPiSimRobotConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Config")
    FString RobotName = TEXT("PiSim_Rover_Alpha");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Config")
    FString GlbMeshPath = TEXT("Saved/Robots/Cache/robot.glb");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Config")
    float MassKg = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Config")
    TArray<FPiSimVirtualPort> VirtualPorts;
};

/** Class providing JSON SerDes and persistence helpers for Robot Configs */
UCLASS()
class PISIM_API UPiSimConfigManager : public UObject
{
    GENERATED_BODY()

public:
    /** Get default file path for robot_config.json */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Config")
    static FString GetDefaultConfigPath();

    /** Save FPiSimRobotConfig to JSON file */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Config")
    static bool SaveRobotConfigToFile(const FPiSimRobotConfig& Config, const FString& FilePath);

    /** Load FPiSimRobotConfig from JSON file */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Config")
    static bool LoadRobotConfigFromFile(const FString& FilePath, FPiSimRobotConfig& OutConfig);

    /** Convert FPiSimRobotConfig to JSON String */
    static FString RobotConfigToJsonString(const FPiSimRobotConfig& Config);

    /** Parse FPiSimRobotConfig from JSON String */
    static bool JsonStringToRobotConfig(const FString& JsonStr, FPiSimRobotConfig& OutConfig);
};
