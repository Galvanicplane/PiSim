// ROS2MessageTypes.h
// Binary message structures aligned with ROS 2 CDR specification.
// Uses #pragma pack(push, 1) to guarantee byte layout and cross-platform alignment.

#pragma once

#include "CoreMinimal.h"
#include "ROS2MessageTypes.generated.h"

#pragma pack(push, 1)

/**
 * ROS 2 geometry_msgs/msg/Twist binary representation.
 * Size: 48 bytes (6x double / float64)
 */
struct FROSTwistRaw
{
    double LinearX;
    double LinearY;
    double LinearZ;
    double AngularX;
    double AngularY;
    double AngularZ;
};

/**
 * ROS 2 sensor_msgs/msg/Imu binary representation.
 * Size: 80 bytes (10x double / float64)
 */
struct FROSImuRaw
{
    // Orientation Quaternion (X, Y, Z, W)
    double OrientationX;
    double OrientationY;
    double OrientationZ;
    double OrientationW;

    // Angular Velocity (X, Y, Z rad/s)
    double AngularVelocityX;
    double AngularVelocityY;
    double AngularVelocityZ;

    // Linear Acceleration (X, Y, Z m/s^2)
    double LinearAccelerationX;
    double LinearAccelerationY;
    double LinearAccelerationZ;
};

#pragma pack(pop)

USTRUCT(BlueprintType)
struct PISIM_API FROSTwistMessage
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROS2|Twist")
    FVector Linear = FVector::ZeroVector; // m/s in ROS frame

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROS2|Twist")
    FVector Angular = FVector::ZeroVector; // rad/s in ROS frame

    /** Serialize to binary CDR payload */
    bool ToBinary(TArray<uint8>& OutBytes) const
    {
        OutBytes.SetNumUninitialized(sizeof(FROSTwistRaw));
        FROSTwistRaw* Raw = reinterpret_cast<FROSTwistRaw*>(OutBytes.GetData());

        Raw->LinearX = Linear.X;
        Raw->LinearY = Linear.Y;
        Raw->LinearZ = Linear.Z;

        Raw->AngularX = Angular.X;
        Raw->AngularY = Angular.Y;
        Raw->AngularZ = Angular.Z;

        return true;
    }

    /** Deserialize from binary CDR payload */
    static bool FromBinary(const TArray<uint8>& InBytes, FROSTwistMessage& OutMsg)
    {
        if (InBytes.Num() < sizeof(FROSTwistRaw))
        {
            return false;
        }

        const FROSTwistRaw* Raw = reinterpret_cast<const FROSTwistRaw*>(InBytes.GetData());
        OutMsg.Linear = FVector(Raw->LinearX, Raw->LinearY, Raw->LinearZ);
        OutMsg.Angular = FVector(Raw->AngularX, Raw->AngularY, Raw->AngularZ);

        return true;
    }
};

USTRUCT(BlueprintType)
struct PISIM_API FROSImuMessage
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROS2|Imu")
    FVector4 Orientation = FVector4(0, 0, 0, 1); // Quaternion X, Y, Z, W

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROS2|Imu")
    FVector AngularVelocity = FVector::ZeroVector; // rad/s

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROS2|Imu")
    FVector LinearAcceleration = FVector::ZeroVector; // m/s^2

    /** Serialize to binary CDR payload */
    bool ToBinary(TArray<uint8>& OutBytes) const
    {
        OutBytes.SetNumUninitialized(sizeof(FROSImuRaw));
        FROSImuRaw* Raw = reinterpret_cast<FROSImuRaw*>(OutBytes.GetData());

        Raw->OrientationX = Orientation.X;
        Raw->OrientationY = Orientation.Y;
        Raw->OrientationZ = Orientation.Z;
        Raw->OrientationW = Orientation.W;

        Raw->AngularVelocityX = AngularVelocity.X;
        Raw->AngularVelocityY = AngularVelocity.Y;
        Raw->AngularVelocityZ = AngularVelocity.Z;

        Raw->LinearAccelerationX = LinearAcceleration.X;
        Raw->LinearAccelerationY = LinearAcceleration.Y;
        Raw->LinearAccelerationZ = LinearAcceleration.Z;

        return true;
    }

    /** Deserialize from binary CDR payload */
    static bool FromBinary(const TArray<uint8>& InBytes, FROSImuMessage& OutMsg)
    {
        if (InBytes.Num() < sizeof(FROSImuRaw))
        {
            return false;
        }

        const FROSImuRaw* Raw = reinterpret_cast<const FROSImuRaw*>(InBytes.GetData());
        OutMsg.Orientation = FVector4(Raw->OrientationX, Raw->OrientationY, Raw->OrientationZ, Raw->OrientationW);
        OutMsg.AngularVelocity = FVector(Raw->AngularVelocityX, Raw->AngularVelocityY, Raw->AngularVelocityZ);
        OutMsg.LinearAcceleration = FVector(Raw->LinearAccelerationX, Raw->LinearAccelerationY, Raw->LinearAccelerationZ);

        return true;
    }
};

USTRUCT(BlueprintType)
struct PISIM_API FROSCompressedImageMessage
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROS2|Image")
    FString Format = TEXT("jpeg");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROS2|Image")
    TArray<uint8> CompressedData;
};
