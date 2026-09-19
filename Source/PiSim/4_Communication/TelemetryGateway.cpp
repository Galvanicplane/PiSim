// TelemetryGateway.cpp

#include "TelemetryGateway.h"

// @state: WIP - Telemetry Gateway bileşen yapıcısı
UPiSimTelemetryGateway::UPiSimTelemetryGateway()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// @state: WIP - IMU sensör verisini kilitleyip kaydeder
void UPiSimTelemetryGateway::FeedIMUData(const FPiSimIMUData& InData)
{
    FScopeLock Lock(&TelemetryLock);
    LatestIMU = InData;
}

// @state: WIP - GPS sensör verisini kilitleyip kaydeder
void UPiSimTelemetryGateway::FeedGPSData(const FPiSimGPSData& InData)
{
    FScopeLock Lock(&TelemetryLock);
    LatestGPS = InData;
}

// @state: WIP - Kamera sıkıştırılmış JPEG çerçevesini kilitleyip kaydeder
void UPiSimTelemetryGateway::FeedCameraFrame(const TArray<uint8>& InJpegBytes)
{
    FScopeLock Lock(&FrameLock);
    LatestCameraJpeg = InJpegBytes;
    FrameCounter++;
}

// @state: WIP - Dış iş parçacığı için thread-safe IMU okuması
bool UPiSimTelemetryGateway::GetLatestIMUData(FPiSimIMUData& OutData)
{
    FScopeLock Lock(&TelemetryLock);
    OutData = LatestIMU;
    return true;
}

// @state: WIP - Dış iş parçacığı için thread-safe GPS okuması
bool UPiSimTelemetryGateway::GetLatestGPSData(FPiSimGPSData& OutData)
{
    FScopeLock Lock(&TelemetryLock);
    OutData = LatestGPS;
    return true;
}

// @state: WIP - Video akış iş parçacığı için en son JPEG verisini çeker
bool UPiSimTelemetryGateway::GetLatestCameraFrame(TArray<uint8>& OutJpegBytes)
{
    FScopeLock Lock(&FrameLock);
    if (LatestCameraJpeg.Num() == 0)
    {
        return false;
    }
    OutJpegBytes = LatestCameraJpeg;
    return true;
}

// @state: WIP - ArduPilot ve PX4 SITL formatında JSON durum dizgisi oluşturur
FString UPiSimTelemetryGateway::BuildSitlJsonState()
{
    FScopeLock Lock(&TelemetryLock);

    FRotator Rot = LatestIMU.Orientation.Rotator();
    return FString::Printf(TEXT("{\"timestamp\":%u,\"roll\":%.3f,\"pitch\":%.3f,\"yaw\":%.3f,\"lat\":%.7f,\"lon\":%.7f,\"alt\":%.2f,\"speed\":%.2f}\n"),
        FrameCounter,
        Rot.Roll, Rot.Pitch, Rot.Yaw,
        LatestGPS.Latitude, LatestGPS.Longitude,
        LatestGPS.AltitudeMeters, LatestGPS.SpeedKmh);
}
