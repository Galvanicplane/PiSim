// PiSimAirImporter.h
// Specialized Air Vehicle Importer and Flight Physics Controller.
// Sets up Socket-based Wing Force Bodies, BLDC Thrusters, and Elevon Control Surfaces.
// Controllable uniformly via Slate UI sliders and Pi 5 ROS 2 UDP messages.

#pragma once

#include "CoreMinimal.h"
#include "PiSimModelImporterBase.h"
#include "PiSimAirImporter.generated.h"

UCLASS()
class PISIM_API APiSimAirImporter : public APiSimModelImporterBase
{
    GENERATED_BODY()

public:
    APiSimAirImporter();

    virtual void SetupDomainComponents() override;
    virtual void ApplyDomainPhysics(float DeltaTime) override;

    // --- Aerodynamic Flight Profile Configuration ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Air|Config")
    FPiSimAerodynamicsConfig FlightConfig;

    // --- Control Surface Command Inputs (from UI or ROS 2) ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Air|Input")
    float CommandThrottle = 0.0f; // [0.0 - 1.0] BLDC Gaz Yüzdesi

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Air|Input")
    float CommandPitchDeg = 0.0f; // [-20° - +20°] Elevon Pitch sapması

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Air|Input")
    float CommandRollDeg = 0.0f;  // [-20° - +20°] Elevon Roll sapması

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Air|Input")
    float CommandYawDeg = 0.0f;   // [-20° - +20°] Rudder Yaw sapması

    // --- Flight Telemetry Outputs (HUD & ROS 2 Telemetry) ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Air|Telemetry")
    float AirspeedKmh = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Air|Telemetry")
    float AirspeedMs = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Air|Telemetry")
    float AngleOfAttackDeg = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Air|Telemetry")
    float TotalLiftForceN = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Air|Telemetry")
    float TotalDragForceN = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Air|Telemetry")
    float TotalThrustForceN = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Air|Telemetry")
    float GForce = 1.0f;

    // --- Sockets / Component References ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Air|Components")
    UPiSimAeroWingComponent* LeftWingComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Air|Components")
    UPiSimAeroWingComponent* RightWingComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Air|Components")
    UPiSimActuatorComponent* MainThrusterActuator = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Air|Components")
    UPiSimActuatorComponent* LeftElevonActuator = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Air|Components")
    UPiSimActuatorComponent* RightElevonActuator = nullptr;

    // --- Unified Control Methods (Callable from UI & ROS 2 UDP) ---
    UFUNCTION(BlueprintCallable, Category = "PiSim|Air|Control")
    void SetThrottleCommand(float InThrottle);

    UFUNCTION(BlueprintCallable, Category = "PiSim|Air|Control")
    void SetElevonMixCommand(float InPitchDeg, float InRollDeg);

    UFUNCTION(BlueprintCallable, Category = "PiSim|Air|Control")
    void HandleRos2ControlPacket(const FString& Topic, float Value);
};
