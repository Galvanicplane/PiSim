// TelemetryGateway.cpp

#include "TelemetryGateway.h"
#include "Sockets.h"
#include "SocketSubsystem.h" 
#include "Vehicle.h"
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

// @state: WIP - ROS 2 (sensor_msgs) & SITL Standart Telemetri Durum Paketini Üretir
FString UPiSimTelemetryGateway::BuildSitlJsonState(double SimTimestampSec)
{
    FScopeLock Lock(&TelemetryLock);

    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
    double Timestamp = SimTimestampSec > 0.0 ? SimTimestampSec : (double)FrameCounter * 0.02;
    RootObject->SetNumberField(TEXT("timestamp"), Timestamp);

    // Araç Kimlik Bilgileri (Çoklu Filo Yönetimi)
    APiSimVehicle* OwnerVehicle = Cast<APiSimVehicle>(GetOwner());
    FString VehicleCode = TEXT("rover_01");
    int32 VehicleId = 0;
    FString VehicleName = TEXT("Rover 01");
    if (OwnerVehicle)
    {
        VehicleCode = OwnerVehicle->VehicleCode;
        VehicleId = OwnerVehicle->VehicleId;
        VehicleName = OwnerVehicle->VehicleName.IsEmpty() ? FString::Printf(TEXT("Rover %02d"), VehicleId) : OwnerVehicle->VehicleName;
    }

    RootObject->SetStringField(TEXT("vehicle_code"), VehicleCode);
    RootObject->SetNumberField(TEXT("vehicle_id"), VehicleId);
    RootObject->SetStringField(TEXT("vehicle_name"), VehicleName);
    RootObject->SetBoolField(TEXT("ros2_msg"), true);

    // =========================================================================
    // ROS 2 Standart Başlığı (std_msgs/msg/Header)
    // =========================================================================
    int64 Sec = (int64)Timestamp;
    uint32 NanoSec = (uint32)((Timestamp - (double)Sec) * 1e9);

    TSharedPtr<FJsonObject> HeaderObj = MakeShareable(new FJsonObject);
    TSharedPtr<FJsonObject> StampObj = MakeShareable(new FJsonObject);
    StampObj->SetNumberField(TEXT("sec"), Sec);
    StampObj->SetNumberField(TEXT("nanosec"), NanoSec);
    HeaderObj->SetObjectField(TEXT("stamp"), StampObj);
    HeaderObj->SetStringField(TEXT("frame_id"), TEXT("base_link"));
    RootObject->SetObjectField(TEXT("header"), HeaderObj);

    // =========================================================================
    // ROS 2 IMU (sensor_msgs/msg/Imu)
    // =========================================================================
    TSharedPtr<FJsonObject> RosImuObj = MakeShareable(new FJsonObject);
    RosImuObj->SetObjectField(TEXT("header"), HeaderObj);

    TSharedPtr<FJsonObject> OriObj = MakeShareable(new FJsonObject);
    OriObj->SetNumberField(TEXT("x"), LatestIMU.Orientation.X);
    OriObj->SetNumberField(TEXT("y"), LatestIMU.Orientation.Y);
    OriObj->SetNumberField(TEXT("z"), LatestIMU.Orientation.Z);
    OriObj->SetNumberField(TEXT("w"), LatestIMU.Orientation.W);
    RosImuObj->SetObjectField(TEXT("orientation"), OriObj);

    TSharedPtr<FJsonObject> AngVelObj = MakeShareable(new FJsonObject);
    AngVelObj->SetNumberField(TEXT("x"), LatestIMU.AngularVelocityRad.X);
    AngVelObj->SetNumberField(TEXT("y"), LatestIMU.AngularVelocityRad.Y);
    AngVelObj->SetNumberField(TEXT("z"), LatestIMU.AngularVelocityRad.Z);
    RosImuObj->SetObjectField(TEXT("angular_velocity"), AngVelObj);

    TSharedPtr<FJsonObject> LinAccObj = MakeShareable(new FJsonObject);
    LinAccObj->SetNumberField(TEXT("x"), LatestIMU.LinearAcceleration.X);
    LinAccObj->SetNumberField(TEXT("y"), LatestIMU.LinearAcceleration.Y);
    LinAccObj->SetNumberField(TEXT("z"), LatestIMU.LinearAcceleration.Z);
    RosImuObj->SetObjectField(TEXT("linear_acceleration"), LinAccObj);

    RootObject->SetObjectField(TEXT("ros2_imu"), RosImuObj);

    // =========================================================================
    // ROS 2 GNSS / GPS (sensor_msgs/msg/NavSatFix)
    // =========================================================================
    TSharedPtr<FJsonObject> RosNavSatObj = MakeShareable(new FJsonObject);
    TSharedPtr<FJsonObject> GpsHeaderObj = MakeShareable(new FJsonObject);
    GpsHeaderObj->SetObjectField(TEXT("stamp"), StampObj);
    GpsHeaderObj->SetStringField(TEXT("frame_id"), TEXT("gps_link"));
    RosNavSatObj->SetObjectField(TEXT("header"), GpsHeaderObj);

    TSharedPtr<FJsonObject> StatusObj = MakeShareable(new FJsonObject);
    StatusObj->SetNumberField(TEXT("status"), 0); // STATUS_FIX
    StatusObj->SetNumberField(TEXT("service"), 1); // SERVICE_GPS
    RosNavSatObj->SetObjectField(TEXT("status"), StatusObj);

    RosNavSatObj->SetNumberField(TEXT("latitude"), LatestGPS.Latitude);
    RosNavSatObj->SetNumberField(TEXT("longitude"), LatestGPS.Longitude);
    RosNavSatObj->SetNumberField(TEXT("altitude"), LatestGPS.AltitudeMeters);
    RootObject->SetObjectField(TEXT("ros2_navsat"), RosNavSatObj);

    // =========================================================================
    // ROS 2 Hız / Twist (geometry_msgs/msg/Twist)
    // =========================================================================
    TSharedPtr<FJsonObject> RosTwistObj = MakeShareable(new FJsonObject);
    TSharedPtr<FJsonObject> LinTwist = MakeShareable(new FJsonObject);
    LinTwist->SetNumberField(TEXT("x"), LatestGPS.VelocityNED.X);
    LinTwist->SetNumberField(TEXT("y"), LatestGPS.VelocityNED.Y);
    LinTwist->SetNumberField(TEXT("z"), LatestGPS.VelocityNED.Z);
    RosTwistObj->SetObjectField(TEXT("linear"), LinTwist);
    RosTwistObj->SetObjectField(TEXT("angular"), AngVelObj);
    RootObject->SetObjectField(TEXT("ros2_twist"), RosTwistObj);

    // =========================================================================
    // ROS 2 Batarya (sensor_msgs/msg/BatteryState)
    // =========================================================================
    TSharedPtr<FJsonObject> RosBattObj = MakeShareable(new FJsonObject);
    RosBattObj->SetObjectField(TEXT("header"), HeaderObj);
    RosBattObj->SetNumberField(TEXT("voltage"), 12.6);
    RosBattObj->SetNumberField(TEXT("percentage"), 0.98);
    RosBattObj->SetBoolField(TEXT("present"), true);
    RootObject->SetObjectField(TEXT("ros2_battery"), RosBattObj);

    // =========================================================================
    // SITL / Gazebo / WebUI Geriye Dönük Uyumluluk Alanları
    // =========================================================================
    TSharedPtr<FJsonObject> SitlImuObject = MakeShareable(new FJsonObject);
    TArray<TSharedPtr<FJsonValue>> GyroArr;
    GyroArr.Add(MakeShareable(new FJsonValueNumber(LatestIMU.AngularVelocityRad.X)));
    GyroArr.Add(MakeShareable(new FJsonValueNumber(LatestIMU.AngularVelocityRad.Y)));
    GyroArr.Add(MakeShareable(new FJsonValueNumber(LatestIMU.AngularVelocityRad.Z)));
    SitlImuObject->SetArrayField(TEXT("gyro"), GyroArr);

    TArray<TSharedPtr<FJsonValue>> AccelArr;
    AccelArr.Add(MakeShareable(new FJsonValueNumber(LatestIMU.LinearAcceleration.X)));
    AccelArr.Add(MakeShareable(new FJsonValueNumber(LatestIMU.LinearAcceleration.Y)));
    AccelArr.Add(MakeShareable(new FJsonValueNumber(LatestIMU.LinearAcceleration.Z)));
    SitlImuObject->SetArrayField(TEXT("accel_body"), AccelArr);
    RootObject->SetObjectField(TEXT("imu"), SitlImuObject);

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

    // Doğrudan derece ve hız alanları
    RootObject->SetNumberField(TEXT("roll"), Rot.Roll);
    RootObject->SetNumberField(TEXT("pitch"), Rot.Pitch);
    RootObject->SetNumberField(TEXT("yaw"), Rot.Yaw);
    RootObject->SetNumberField(TEXT("altitude"), LatestBaro.PressureAltitudeM);

    float AirspeedMs = FMath::Sqrt(FMath::Max(0.0f, LatestBaro.DiffPressureHPa * 100.0f * 2.0f / 1.225f));
    RootObject->SetNumberField(TEXT("airspeed"), AirspeedMs);
    RootObject->SetNumberField(TEXT("speed"), AirspeedMs * 3.6f);
    RootObject->SetNumberField(TEXT("battery"), 98.0);
    double OutLat = FMath::Abs(LatestGPS.Latitude) > 0.0001 ? LatestGPS.Latitude : (39.9334 + (double)(FMath::Max(1, VehicleId) - 1) * 0.00018);
    double OutLon = FMath::Abs(LatestGPS.Longitude) > 0.0001 ? LatestGPS.Longitude : (32.8597 + (double)(FMath::Max(1, VehicleId) - 1) * 0.00018);
    RootObject->SetNumberField(TEXT("lat"), OutLat);
    RootObject->SetNumberField(TEXT("lon"), OutLon);
    RootObject->SetStringField(TEXT("flight_mode"), TEXT("AKTİF"));

    // Realtime flags
    RootObject->SetBoolField(TEXT("no_time_sync"), true);
    RootObject->SetBoolField(TEXT("no_lockstep"), true);

    // Heartbeat identification & Dinamik Araç Selamlaması
    RootObject->SetStringField(TEXT("magic"), TEXT("PISIM_TELEMETRY"));
    RootObject->SetStringField(TEXT("source"), TEXT("UE5_UNREAL"));
    FString MsgContent = FString::Printf(TEXT("HI FROM %s"), *VehicleCode);
    RootObject->SetStringField(TEXT("msg"), MsgContent);

    FString OutputString;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
    OutputString.Append(TEXT("\n"));

    return OutputString;
}


// @state: WIP - Simülasyon genelinde veya araç seviyesinde Pi 5'e keşif probu / heartbeat fırlatır ("HI FROM UNREAL")
void UPiSimTelemetryGateway::BroadcastProbe(const FString& InMsg)
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        return;
    }

    FSocket* TempSocket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("PiSimTelemetryProbe"), true);
    if (!TempSocket)
    {
        return;
    }

    TempSocket->SetBroadcast(true);
    FString ProbePayload = FString::Printf(TEXT("{\"magic\":\"PISIM_PROBE\",\"source\":\"UE5_UNREAL\",\"vehicle_code\":\"unreal_sim\",\"msg\":\"%s\"}\n"), *InMsg);
    FTCHARToUTF8 Utf8(*ProbePayload);
    int32 BytesSent = 0;

    TSharedRef<FInternetAddr> BcastAddr = SocketSubsystem->CreateInternetAddr();
    bool bValid = false;
    BcastAddr->SetIp(TEXT("255.255.255.255"), bValid);
    BcastAddr->SetPort(7499);
    if (bValid)
    {
        TempSocket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), BytesSent, *BcastAddr);
    }
    SocketSubsystem->DestroySocket(TempSocket);
}
