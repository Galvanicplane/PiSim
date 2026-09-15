// PiSimActuatorComponent.h
// Modular Actuator Component for PiSim Pawns (Motors, Servos, Thrusters, Wheels)
// Controllable uniformly via Slate UI sliders and Raspberry Pi 5 ROS 2 UDP commands.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "PiSimModelImporter.h" // For EPiSimMotorRole
#include "PiSimActuatorComponent.generated.h"

class UProceduralMeshComponent;

UENUM(BlueprintType)
enum class EPiSimMotorType : uint8
{
    BLDC_ESC        UMETA(DisplayName = "BLDC Fırçasız Motor (ESC / Gaz % / RPM)"),
    Brushed_DC      UMETA(DisplayName = "DC Fırçalı Motor (Hız / Tork)"),
    Servo_Position  UMETA(DisplayName = "RC / Akıllı Servo (Açı / Pozisyon)"),
    Stepper         UMETA(DisplayName = "Step Motor (Hassas Adım)")
};

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimActuatorComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UPiSimActuatorComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // --- Configuration ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    int32 BoneIndex = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    FString ActuatorName = TEXT("");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    EPiSimMotorRole Role = EPiSimMotorRole::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    EPiSimMotorType MotorType = EPiSimMotorType::Brushed_DC;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    FString Ros2Topic = TEXT("");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    FVector BoneDirection = FVector(0.0f, 1.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    float MaxVelocityRPM = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    float MaxTorqueNm = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    float MinLimitDeg = -45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    float MaxLimitDeg = +45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator")
    bool bReverseDirection = false;

    // --- Live Control Inputs ---
    /** Normalize edilmiş komut (-1.0 ile +1.0 arası) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator|Live")
    float TargetNormalizedValue = 0.0f;

    /** Hedef açı (Servo mafsallar için derece cinsinden) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator|Live")
    float TargetAngleDeg = 0.0f;

    /** Hedef açısal hız (RPM cinsinden) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Actuator|Live")
    float TargetRPM = 0.0f;

    // --- Live State Telemetry ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Actuator|State")
    float CurrentAngleDeg = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Actuator|State")
    float CurrentRPM = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Actuator|State")
    float CurrentOutputForceOrTorque = 0.0f;

    // --- Associated Visual Mesh (Pervane, elevon, tekerlek görsel parçası) ---
    UPROPERTY(BlueprintReadWrite, Category = "PiSim|Actuator")
    UProceduralMeshComponent* LinkedVisualMesh = nullptr;

    // --- Control API ---
    UFUNCTION(BlueprintCallable, Category = "PiSim|Actuator")
    void SetNormalizedCommand(float InValue);

    UFUNCTION(BlueprintCallable, Category = "PiSim|Actuator")
    void SetTargetAngle(float InAngleDeg);

    UFUNCTION(BlueprintCallable, Category = "PiSim|Actuator")
    void SetTargetVelocity(float InRPM);

    /** Belirli bir gövdeye itki / kuvvet uygula */
    void ApplyActuatorPhysics(float DeltaTime, UPrimitiveComponent* RootBody);
};
