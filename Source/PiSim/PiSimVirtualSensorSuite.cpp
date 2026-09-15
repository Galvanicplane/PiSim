// PiSimVirtualSensorSuite.cpp
// High-Fidelity Virtual Sensor Suite Implementation.

#include "PiSimVirtualSensorSuite.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

UPiSimVirtualSensorSuite::UPiSimVirtualSensorSuite()
{
    PrimaryComponentTick.bCanEverTick = true;
    bWantsInitializeComponent = true;
}

void UPiSimVirtualSensorSuite::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UPiSimVirtualSensorSuite::UpdateSensors(
    float DeltaTime,
    const FVector& BodyLocationUE,
    const FVector& WorldLinearVelocityUE,
    const FVector& WorldLinearAccelUE,
    const FVector& WorldAngularVelDeg,
    const FQuat& WorldOrientation,
    float AirspeedKmh)
{
    // =========================================================================
    // 1) IMU İVMEÖLÇER (ACCELEROMETER - m/s² BODY NED)
    // =========================================================================
    // İvmeölçer, serbest düşüşte 0 m/s² okur; masada dururken yukarı doğru 1G (+9.81 m/s²) tepki kuvveti ölçer.
    // Dolayısıyla ölçülen spesifik kuvvet: a_meas = a_linear - g_gravity
    const FVector GravityWorldUE = FVector(0.0f, 0.0f, -980.665f); // cm/s² Z-up
    FVector SpecificForceWorldUE = WorldLinearAccelUE - GravityWorldUE;

    // Gövde yerel koordinatlarına dönüştür
    FVector SpecificForceBodyUE = WorldOrientation.UnrotateVector(SpecificForceWorldUE);

    // Unreal Engine (X-İleri, Y-Sağ, Z-Yukarı, cm/s²) -> Havacılık Body NED (X-Burun, Y-Sağ Kanat, Z-Aşağı, m/s²)
    ImuData.AccelNED.X = SpecificForceBodyUE.X * 0.01f;
    ImuData.AccelNED.Y = SpecificForceBodyUE.Y * 0.01f;
    ImuData.AccelNED.Z = -SpecificForceBodyUE.Z * 0.01f;

    // =========================================================================
    // 2) IMU JİROSKOP (GYROSCOPE - rad/s BODY NED)
    // =========================================================================
    // UE5 açısal hızı (Roll-X, Pitch-Y, Yaw-Z deg/s) -> Body NED rad/s
    ImuData.GyroNED.X = FMath::DegreesToRadians(WorldAngularVelDeg.X);
    ImuData.GyroNED.Y = FMath::DegreesToRadians(WorldAngularVelDeg.Y);
    ImuData.GyroNED.Z = FMath::DegreesToRadians(-WorldAngularVelDeg.Z);

    // =========================================================================
    // 3) DÜNYA MANYETİK ALANI (MAGNETOMETER - GAUSS)
    // =========================================================================
    // Tipik orta enlem Dünya manyetik alanı vektörü (NED: North=0.22, East=0.01, Down=0.42 Gauss)
    const FVector EarthMagNED(0.22f, 0.01f, 0.42f);
    // UE5 rotasyonu ile gövde eksenine yansıt
    FRotator BodyRot = WorldOrientation.Rotator();
    ImuData.MagNED = BodyRot.UnrotateVector(EarthMagNED);

    // Oryantasyon Kuaterniyonu (NED uyumlu)
    ImuData.OrientationNED = FQuat(WorldOrientation.X, WorldOrientation.Y, -WorldOrientation.Z, WorldOrientation.W);

    // =========================================================================
    // 4) BAROMETRE & PİTOT TÜPÜ (BAROMETER & AIRSPEED)
    // =========================================================================
    // İrtifa: Başlangıç irtifası (AMSL) + Unreal Z ekseni değişimi (cm -> m)
    float AltM = HomeAltitudeAMSL + ((BodyLocationUE.Z - WorldOriginLocationUE.Z) * 0.01f);
    BaroData.PressureAltitudeM = AltM;

    // Uluslararası Standart Atmosfer (ISA) Barometrik Basınç Formülü
    // P = P0 * (1 - 2.25577e-5 * h)^5.25588
    float ClampedAlt = FMath::Clamp(AltM, -500.0f, 15000.0f);
    BaroData.AbsPressureHPa = 1013.25f * FMath::Pow(1.0f - (2.25577e-5f * ClampedAlt), 5.25588f);
    BaroData.TemperatureC = 15.0f - (0.0065f * ClampedAlt);

    // Pitot Tüpü Dinamik Basıncı: q = 0.5 * rho * V² (1 Pa = 0.01 hPa)
    float AirspeedMs = AirspeedKmh / 3.6f;
    const float AirDensityRho = 1.225f; // kg/m³ deniz seviyesi
    float DynamicPressurePa = 0.5f * AirDensityRho * (AirspeedMs * AirspeedMs);
    BaroData.DiffPressureHPa = DynamicPressurePa * 0.01f; // Pa to hPa

    // =========================================================================
    // 5) GPS KONUM & HIZ (WGS84 u-blox Simülasyonu - 10 Hz)
    // =========================================================================
    GpsUpdateTimer += DeltaTime;
    if (GpsUpdateTimer >= GpsIntervalSec)
    {
        GpsUpdateTimer = 0.0f;

        // Yer düzlemi ofsetleri (cm -> m)
        double DeltaNorthM = (BodyLocationUE.X - WorldOriginLocationUE.X) * 0.01;
        double DeltaEastM = (BodyLocationUE.Y - WorldOriginLocationUE.Y) * 0.01;

        // 1 derece enlem ~ 111,139 metre
        double MetersPerLatDeg = 111139.0;
        double MetersPerLonDeg = 111139.0 * FMath::Cos(FMath::DegreesToRadians(HomeLatitudeDeg));

        GpsData.LatitudeDeg = HomeLatitudeDeg + (DeltaNorthM / MetersPerLatDeg);
        GpsData.LongitudeDeg = HomeLongitudeDeg + (DeltaEastM / (MetersPerLonDeg > 1.0 ? MetersPerLonDeg : 1.0));
        GpsData.AltitudeAMSL = AltM;

        // Hız vektörü (NED m/s)
        FVector VelWorldMs = WorldLinearVelocityUE * 0.01f;
        GpsData.VelNED.X = VelWorldMs.X;
        GpsData.VelNED.Y = VelWorldMs.Y;
        GpsData.VelNED.Z = -VelWorldMs.Z;

        // Yatay yer hızı
        GpsData.GroundSpeedMs = FMath::Sqrt(VelWorldMs.X * VelWorldMs.X + VelWorldMs.Y * VelWorldMs.Y);

        // Seyir açısı (Course over Ground) [0..360]
        float CourseRad = FMath::Atan2(VelWorldMs.Y, VelWorldMs.X);
        float CourseDeg = FMath::RadiansToDegrees(CourseRad);
        if (CourseDeg < 0.0f) CourseDeg += 360.0f;
        GpsData.HeadingDeg = CourseDeg;
    }
}

FString UPiSimVirtualSensorSuite::BuildArduPilotJsonPayload(double SimTimestampSec) const
{
    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);

    RootObject->SetNumberField(TEXT("timestamp"), SimTimestampSec);

    // IMU Object
    TSharedPtr<FJsonObject> ImuObject = MakeShareable(new FJsonObject);
    TArray<TSharedPtr<FJsonValue>> GyroArr;
    GyroArr.Add(MakeShareable(new FJsonValueNumber(ImuData.GyroNED.X)));
    GyroArr.Add(MakeShareable(new FJsonValueNumber(ImuData.GyroNED.Y)));
    GyroArr.Add(MakeShareable(new FJsonValueNumber(ImuData.GyroNED.Z)));
    ImuObject->SetArrayField(TEXT("gyro"), GyroArr);

    TArray<TSharedPtr<FJsonValue>> AccelArr;
    AccelArr.Add(MakeShareable(new FJsonValueNumber(ImuData.AccelNED.X)));
    AccelArr.Add(MakeShareable(new FJsonValueNumber(ImuData.AccelNED.Y)));
    AccelArr.Add(MakeShareable(new FJsonValueNumber(ImuData.AccelNED.Z)));
    ImuObject->SetArrayField(TEXT("accel_body"), AccelArr);
    RootObject->SetObjectField(TEXT("imu"), ImuObject);

    // Position [Lat, Lon, Alt]
    TArray<TSharedPtr<FJsonValue>> PosArr;
    PosArr.Add(MakeShareable(new FJsonValueNumber(GpsData.LatitudeDeg)));
    PosArr.Add(MakeShareable(new FJsonValueNumber(GpsData.LongitudeDeg)));
    PosArr.Add(MakeShareable(new FJsonValueNumber(GpsData.AltitudeAMSL)));
    RootObject->SetArrayField(TEXT("position"), PosArr);

    // Attitude [roll, pitch, yaw] in radians (NED frame) — ArduPilot 4.x JSON SITL requires this format.
    // "quaternion" array is NOT supported in this ArduPilot version; use Euler angles.
    FRotator AttRot = ImuData.OrientationNED.Rotator();
    TArray<TSharedPtr<FJsonValue>> AttArr;
    AttArr.Add(MakeShareable(new FJsonValueNumber(FMath::DegreesToRadians(AttRot.Roll))));
    AttArr.Add(MakeShareable(new FJsonValueNumber(FMath::DegreesToRadians(AttRot.Pitch))));
    AttArr.Add(MakeShareable(new FJsonValueNumber(FMath::DegreesToRadians(AttRot.Yaw))));
    RootObject->SetArrayField(TEXT("attitude"), AttArr);

    // Velocity [Vx, Vy, Vz] in NED m/s
    TArray<TSharedPtr<FJsonValue>> VelArr;
    VelArr.Add(MakeShareable(new FJsonValueNumber(GpsData.VelNED.X)));
    VelArr.Add(MakeShareable(new FJsonValueNumber(GpsData.VelNED.Y)));
    VelArr.Add(MakeShareable(new FJsonValueNumber(GpsData.VelNED.Z)));
    RootObject->SetArrayField(TEXT("velocity"), VelArr);

    // Pitot Airspeed m/s
    float AirspeedMs = FMath::Sqrt(FMath::Max(0.0f, BaroData.DiffPressureHPa * 100.0f * 2.0f / 1.225f));
    RootObject->SetNumberField(TEXT("airspeed"), AirspeedMs);

    // Use condensed (no-whitespace) JSON to match ArduPilot parser expectations
    FString OutputString;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

    // ArduPilot SITL JSON parser requires messages to be terminated with '\n'
    return OutputString + TEXT("\n");
}
