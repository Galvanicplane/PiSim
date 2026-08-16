// ROS2UE5Converter.h
// Utility class for converting units and coordinate frames between ROS 2 and UE5.
// ROS 2: SI Units (m, rad, m/s, rad/s), Right-Handed System (X-Forward, Y-Left, Z-Up).
// UE5: Centimeters & Degrees (cm, deg, cm/s, deg/s), Left-Handed System (X-Forward, Y-Right, Z-Up).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/KismetMathLibrary.h"

class PISIM_API FROS2UE5Converter
{
public:
    // Scale factor between meters and centimeters
    static constexpr float M_TO_CM = 100.0f;
    static constexpr float CM_TO_M = 0.01f;

    // Linear Position & Velocity Transformations
    // ROS 2 (X: Fwd, Y: Left, Z: Up) -> UE5 (X: Fwd, Y: Right, Z: Up)
    static FORCEINLINE FVector ROS2ToUE5Position(const FVector& PosRosMeters)
    {
        return FVector(PosRosMeters.X * M_TO_CM, -PosRosMeters.Y * M_TO_CM, PosRosMeters.Z * M_TO_CM);
    }

    static FORCEINLINE FVector UE5ToROS2Position(const FVector& PosUE5Cm)
    {
        return FVector(PosUE5Cm.X * CM_TO_M, -PosUE5Cm.Y * CM_TO_M, PosUE5Cm.Z * CM_TO_M);
    }

    static FORCEINLINE FVector ROS2ToUE5LinearVelocity(const FVector& VelRosMetersPerSec)
    {
        return FVector(VelRosMetersPerSec.X * M_TO_CM, -VelRosMetersPerSec.Y * M_TO_CM, VelRosMetersPerSec.Z * M_TO_CM);
    }

    static FORCEINLINE FVector UE5ToROS2LinearVelocity(const FVector& VelUE5CmPerSec)
    {
        return FVector(VelUE5CmPerSec.X * CM_TO_M, -VelUE5CmPerSec.Y * CM_TO_M, VelUE5CmPerSec.Z * CM_TO_M);
    }

    static FORCEINLINE FVector UE5ToROS2LinearAcceleration(const FVector& AccelUE5CmPerSecSq)
    {
        return FVector(AccelUE5CmPerSecSq.X * CM_TO_M, -AccelUE5CmPerSecSq.Y * CM_TO_M, AccelUE5CmPerSecSq.Z * CM_TO_M);
    }

    // Angular Velocity Transformations
    // ROS 2 Angular (rad/s around X, Y, Z right-handed) -> UE5 Angular (deg/s around X, Y, Z left-handed)
    static FORCEINLINE FVector ROS2ToUE5AngularVelocity(const FVector& AngVelRosRadPerSec)
    {
        // Pitch/Roll/Yaw axis mapping between right and left handed systems
        // Roll (around X): positive right-hand = positive left-hand -> +X
        // Pitch (around Y): positive right-hand = negative left-hand -> -Y
        // Yaw (around Z): positive right-hand = negative left-hand -> -Z
        return FVector(
            FMath::RadiansToDegrees(AngVelRosRadPerSec.X),
            -FMath::RadiansToDegrees(AngVelRosRadPerSec.Y),
            -FMath::RadiansToDegrees(AngVelRosRadPerSec.Z)
        );
    }

    static FORCEINLINE FVector UE5ToROS2AngularVelocity(const FVector& AngVelUE5DegPerSec)
    {
        return FVector(
            FMath::DegreesToRadians(AngVelUE5DegPerSec.X),
            -FMath::DegreesToRadians(AngVelUE5DegPerSec.Y),
            -FMath::DegreesToRadians(AngVelUE5DegPerSec.Z)
        );
    }

    // Quaternion Orientation Transformations
    // ROS 2 (X, Y, Z, W) -> UE5 FQuat (X, Y, Z, W)
    static FORCEINLINE FQuat ROS2ToUE5Quaternion(double X, double Y, double Z, double W)
    {
        // Converts Right-Handed Quaternion to Left-Handed FQuat
        return FQuat(static_cast<float>(X), static_cast<float>(-Y), static_cast<float>(-Z), static_cast<float>(W));
    }

    static FORCEINLINE void UE5ToROS2Quaternion(const FQuat& QuatUE5, double& OutX, double& OutY, double& OutZ, double& OutW)
    {
        OutX = static_cast<double>(QuatUE5.X);
        OutY = static_cast<double>(-QuatUE5.Y);
        OutZ = static_cast<double>(-QuatUE5.Z);
        OutW = static_cast<double>(QuatUE5.W);
    }
};
