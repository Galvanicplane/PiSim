// PiSimVirtualSensorSuite.h
// High-Fidelity Virtual Sensor Suite for PX4 HITL, ArduPilot SITL, and ROS 2 Telemetry.
// Simulates IMU (Accel, Gyro, Mag), Barometer (ISA), Pitot Airspeed, and WGS84 GPS.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PiSimVirtualSensorSuite.generated.h"

USTRUCT(BlueprintType)
struct PISIM_API FPiSimImuSensorData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    FVector AccelNED = FVector::ZeroVector; // Body Frame m/s² (Including gravity reaction)

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    FVector GyroNED = FVector::ZeroVector; // Body Frame rad/s

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    FVector MagNED = FVector(0.22f, 0.01f, 0.42f); // Body Frame Gauss (Earth Magnetic Field)

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    FQuat OrientationNED = FQuat::Identity; // World NED to Body NED

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    FVector AttitudeEulerRad = FVector::ZeroVector; // [Roll, Pitch, Yaw] in radians
};

USTRUCT(BlueprintType)
struct PISIM_API FPiSimBaroSensorData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    float AbsPressureHPa = 1013.25f; // Absolute air pressure (hPa / mbar)

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    float DiffPressureHPa = 0.0f; // Pitot tube dynamic pressure (hPa)

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    float PressureAltitudeM = 0.0f; // Barometric altitude AMSL (meters)

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    float TemperatureC = 20.0f; // Ambient temperature (°C)
};

USTRUCT(BlueprintType)
struct PISIM_API FPiSimGpsSensorData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    double LatitudeDeg = 41.1035; // WGS84 Latitude (Default: Hezarfen Airport)

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    double LongitudeDeg = 28.5539; // WGS84 Longitude

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    float AltitudeAMSL = 120.0f; // Altitude above mean sea level (meters)

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    FVector VelNED = FVector::ZeroVector; // Velocity in NED (m/s)

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    float GroundSpeedMs = 0.0f; // 2D Horizontal speed (m/s)

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    float HeadingDeg = 0.0f; // Ground course heading (degrees [0..360])

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    int32 FixType = 3; // 3 = 3D Fix

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    int32 SatellitesVisible = 14;

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    float EPH = 0.8f; // Horizontal dilution of precision

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    float EPV = 1.1f; // Vertical dilution of precision
};

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimVirtualSensorSuite : public UActorComponent
{
    GENERATED_BODY()

public:
    UPiSimVirtualSensorSuite();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Updates all sensor physics from actor's rigid body kinematics
    void UpdateSensors(
        float DeltaTime,
        const FVector& BodyLocationUE,
        const FVector& WorldLinearVelocityUE,
        const FVector& WorldLinearAccelUE,
        const FVector& WorldAngularVelDeg,
        const FQuat& WorldOrientation,
        float AirspeedKmh
    );

    // Formats and exports data for ArduPilot SITL JSON protocol (UDP Port 9002)
    FString BuildArduPilotJsonPayload(double SimTimestampSec) const;

    // Current Sensor Readouts
    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    FPiSimImuSensorData ImuData;

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    FPiSimBaroSensorData BaroData;

    UPROPERTY(BlueprintReadOnly, Category = "PiSim|Sensors")
    FPiSimGpsSensorData GpsData;

    // Origin Reference for GPS conversion
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GPS")
    double HomeLatitudeDeg = 41.1035;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GPS")
    double HomeLongitudeDeg = 28.5539;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GPS")
    float HomeAltitudeAMSL = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GPS")
    FVector WorldOriginLocationUE = FVector::ZeroVector;

    // Aircraft Model Body Axes relative to Component (Default FBX: Nose=-Y, Right=+X, Up=+Z)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Axes")
    FVector ModelForwardAxis = FVector(0.0f, -1.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Axes")
    FVector ModelRightAxis = FVector(1.0f, 0.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Axes")
    FVector ModelUpAxis = FVector(0.0f, 0.0f, 1.0f);

private:
    float GpsUpdateTimer = 0.0f;
    const float GpsIntervalSec = 0.1f; // 10 Hz GPS rate
};
