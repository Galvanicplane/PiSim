// TelemetryGateway.cpp

#include "TelemetryGateway.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

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

// @state: WIP - Barometre sensör verisini kilitleyip kaydeder
void UPiSimTelemetryGateway::FeedBaroData(const FPiSimBaroData& InData)
{
    FScopeLock Lock(&TelemetryLock);
    LatestBaro = InData;
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

// @state: WIP - Dış iş parçacığı için thread-safe Baro okuması
bool UPiSimTelemetryGateway::GetLatestBaroData(FPiSimBaroData& OutData)
{
    FScopeLock Lock(&TelemetryLock);
    OutData = LatestBaro;
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

// @state: WIP - ArduPilot SITL JSON durum paketini üretir
FString UPiSimTelemetryGateway::BuildSitlJsonState(double SimTimestampSec)
{
    FScopeLock Lock(&TelemetryLock);

    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
    RootObject->SetNumberField(TEXT("timestamp"), SimTimestampSec > 0.0 ? SimTimestampSec : (double)FrameCounter * 0.02);

    // IMU: gyro & accel_body
    TSharedPtr<FJsonObject> ImuObject = MakeShareable(new FJsonObject);
    TArray<TSharedPtr<FJsonValue>> GyroArr;
    GyroArr.Add(MakeShareable(new FJsonValueNumber(LatestIMU.AngularVelocityRad.X)));
    GyroArr.Add(MakeShareable(new FJsonValueNumber(LatestIMU.AngularVelocityRad.Y)));
    GyroArr.Add(MakeShareable(new FJsonValueNumber(LatestIMU.AngularVelocityRad.Z)));
    ImuObject->SetArrayField(TEXT("gyro"), GyroArr);

    TArray<TSharedPtr<FJsonValue>> AccelArr;
    AccelArr.Add(MakeShareable(new FJsonValueNumber(LatestIMU.LinearAcceleration.X)));
    AccelArr.Add(MakeShareable(new FJsonValueNumber(LatestIMU.LinearAcceleration.Y)));
    AccelArr.Add(MakeShareable(new FJsonValueNumber(LatestIMU.LinearAcceleration.Z)));
    ImuObject->SetArrayField(TEXT("accel_body"), AccelArr);
    RootObject->SetObjectField(TEXT("imu"), ImuObject);

    // Position [North, East, Down] in meters
    TArray<TSharedPtr<FJsonValue>> PosArr;
    PosArr.Add(MakeShareable(new FJsonValueNumber(0.0)));
    PosArr.Add(MakeShareable(new FJsonValueNumber(0.0)));
    PosArr.Add(MakeShareable(new FJsonValueNumber(-LatestBaro.PressureAltitudeM)));
    RootObject->SetArrayField(TEXT("position"), PosArr);

    // Attitude [Roll, Pitch, Yaw] in radians (NED)
    FRotator Rot = LatestIMU.Orientation.Rotator();
    TArray<TSharedPtr<FJsonValue>> AttArr;
    AttArr.Add(MakeShareable(new FJsonValueNumber(FMath::DegreesToRadians(Rot.Roll))));
    AttArr.Add(MakeShareable(new FJsonValueNumber(FMath::DegreesToRadians(-Rot.Pitch))));
    AttArr.Add(MakeShareable(new FJsonValueNumber(FMath::DegreesToRadians(Rot.Yaw))));
    RootObject->SetArrayField(TEXT("attitude"), AttArr);

    // Velocity [Vx, Vy, Vz] in NED m/s
    TArray<TSharedPtr<FJsonValue>> VelArr;
    VelArr.Add(MakeShareable(new FJsonValueNumber(LatestGPS.VelocityNED.X)));
    VelArr.Add(MakeShareable(new FJsonValueNumber(LatestGPS.VelocityNED.Y)));
    VelArr.Add(MakeShareable(new FJsonValueNumber(LatestGPS.VelocityNED.Z)));
    RootObject->SetArrayField(TEXT("velocity"), VelArr);

    // Pitot Airspeed m/s
    float AirspeedMs = FMath::Sqrt(FMath::Max(0.0f, LatestBaro.DiffPressureHPa * 100.0f * 2.0f / 1.225f));
    RootObject->SetNumberField(TEXT("airspeed"), AirspeedMs);

    // Realtime flags
    RootObject->SetBoolField(TEXT("no_time_sync"), true);
    RootObject->SetBoolField(TEXT("no_lockstep"), true);

    FString OutputString;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
    OutputString.Append(TEXT("\n"));

    return OutputString;
}
