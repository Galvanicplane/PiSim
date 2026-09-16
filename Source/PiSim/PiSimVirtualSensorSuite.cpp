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
    // a_meas = a_linear - g_gravity
    const FVector GravityWorldUE = FVector(0.0f, 0.0f, -980.665f); // cm/s² Z-up
    FVector SpecificForceWorldUE = WorldLinearAccelUE - GravityWorldUE;

    // Gövde yerel koordinatlarına dönüştür (Unreal Model space)
    FVector SpecificForceBodyUE = WorldOrientation.UnrotateVector(SpecificForceWorldUE);

    // Unreal Model Space -> Aircraft Body NED (m/s²):
    // ModelForwardAxis (Burun, varsayılan -Y) -> Body NED +X
    // ModelRightAxis   (Sağ kanat, varsayılan +X) -> Body NED +Y
    // ModelUpAxis      (Üst gövde, varsayılan +Z) -> Body NED -Z (Aşağı = +Z_NED)
    ImuData.AccelNED.X = (SpecificForceBodyUE | ModelForwardAxis.GetSafeNormal()) * 0.01f;
    ImuData.AccelNED.Y = (SpecificForceBodyUE | ModelRightAxis.GetSafeNormal()) * 0.01f;
    ImuData.AccelNED.Z = -(SpecificForceBodyUE | ModelUpAxis.GetSafeNormal()) * 0.01f;

    // =========================================================================
    // 2) IMU JİROSKOP (GYROSCOPE - rad/s BODY NED)
    // =========================================================================
    // Gövde yerel açısal hızını bul (deg/s)
    FVector BodyAngularVelDeg = WorldOrientation.UnrotateVector(WorldAngularVelDeg);

    // Model Space -> Aircraft Body NED (rad/s):
    // GyroNED.X = Roll hızı  (ModelForwardAxis / Burun etrafında dönme)
    // GyroNED.Y = Pitch hızı (ModelRightAxis / Kanat etrafında yunuslama)
    // GyroNED.Z = Yaw hızı   (Aşağı eksen etrafında sapma)
    ImuData.GyroNED.X = FMath::DegreesToRadians(BodyAngularVelDeg | ModelForwardAxis.GetSafeNormal());
    ImuData.GyroNED.Y = FMath::DegreesToRadians(BodyAngularVelDeg | ModelRightAxis.GetSafeNormal());
    ImuData.GyroNED.Z = FMath::DegreesToRadians(-(BodyAngularVelDeg | ModelUpAxis.GetSafeNormal()));

    // =========================================================================
    // 3) DÜNYA MANYETİK ALANI (MAGNETOMETER - GAUSS)
    // =========================================================================
    // Tipik orta enlem Dünya manyetik alanı vektörü (NED: North=0.22, East=0.01, Down=0.42 Gauss)
    // UE5 Dünya koordinatlarında: X=North, Y=East, Z=-Down
    const FVector EarthMagWorldUE(0.22f, 0.01f, -0.42f);
    FVector MagBodyUE = WorldOrientation.UnrotateVector(EarthMagWorldUE);
    ImuData.MagNED.X = (MagBodyUE | ModelForwardAxis.GetSafeNormal());
    ImuData.MagNED.Y = (MagBodyUE | ModelRightAxis.GetSafeNormal());
    ImuData.MagNED.Z = -(MagBodyUE | ModelUpAxis.GetSafeNormal());

    // =========================================================================
    // 4) HAVACILIK ATTITUDE (ROLL, PITCH, YAW - NED RADIAN)
    // =========================================================================
    // Modelin burun, sağ kanat ve yukarı vektörlerinin Dünya uzayındaki mutlak doğrultuları:
    FVector WorldForward = WorldOrientation.RotateVector(ModelForwardAxis.GetSafeNormal()).GetSafeNormal();
    FVector WorldRight   = WorldOrientation.RotateVector(ModelRightAxis.GetSafeNormal()).GetSafeNormal();
    FVector WorldUp      = WorldOrientation.RotateVector(ModelUpAxis.GetSafeNormal()).GetSafeNormal();

    // Havacılık Tait-Bryan (Z-Y-X) açıları (NED uzayında):
    // Yunuslama (Pitch): Burnun yatay düzlemle yaptığı açı (Aşağı dalış: eksi, Yukarı tırmanış: artı)
    float PitchRad = FMath::Asin(FMath::Clamp(WorldForward.Z, -1.0f, 1.0f));

    // Sapma/Pusula (Yaw/Heading): Burnun Kuzey'den (+X) Doğu'ya (+Y) yönelimi
    float YawRad = FMath::Atan2(WorldForward.Y, WorldForward.X);

    // Yana Yatış (Roll): Kanatların burun ekseni etrafında yatışı (Sağa yatış: artı, Sola yatış: eksi)
    float RollRad = FMath::Atan2(-WorldRight.Z, WorldUp.Z);

    ImuData.AttitudeEulerRad = FVector(RollRad, PitchRad, YawRad);

    // Oryantasyon kuaterniyonunu Tait-Bryan açılarından türet
    FRotator AerospaceRot(FMath::RadiansToDegrees(PitchRad), FMath::RadiansToDegrees(YawRad), FMath::RadiansToDegrees(RollRad));
    ImuData.OrientationNED = AerospaceRot.Quaternion();

    // =========================================================================
    // 5) BAROMETRE & PİTOT TÜPÜ (BAROMETER & AIRSPEED)
    // =========================================================================
    float AltM = HomeAltitudeAMSL + ((BodyLocationUE.Z - WorldOriginLocationUE.Z) * 0.01f);
    BaroData.PressureAltitudeM = AltM;

    float ClampedAlt = FMath::Clamp(AltM, -500.0f, 15000.0f);
    BaroData.AbsPressureHPa = 1013.25f * FMath::Pow(1.0f - (2.25577e-5f * ClampedAlt), 5.25588f);
    BaroData.TemperatureC = 15.0f - (0.0065f * ClampedAlt);

    float AirspeedMs = AirspeedKmh / 3.6f;
    const float AirDensityRho = 1.225f; // kg/m³ deniz seviyesi
    float DynamicPressurePa = 0.5f * AirDensityRho * (AirspeedMs * AirspeedMs);
    BaroData.DiffPressureHPa = DynamicPressurePa * 0.01f; // Pa to hPa

    // =========================================================================
    // 6) GPS KONUM & HIZ (WGS84 u-blox Simülasyonu - 50 Hz Kesintisiz)
    // =========================================================================
    // Yer düzlemi ofsetleri (cm -> m)
    double DeltaNorthM = (BodyLocationUE.X - WorldOriginLocationUE.X) * 0.01;
    double DeltaEastM = (BodyLocationUE.Y - WorldOriginLocationUE.Y) * 0.01;

    // 1 derece enlem ~ 111,139 metre
    double MetersPerLatDeg = 111139.0;
    double MetersPerLonDeg = 111139.0 * FMath::Cos(FMath::DegreesToRadians(HomeLatitudeDeg));

    GpsData.LatitudeDeg = HomeLatitudeDeg + (DeltaNorthM / MetersPerLatDeg);
    GpsData.LongitudeDeg = HomeLongitudeDeg + (DeltaEastM / (MetersPerLonDeg > 1.0 ? MetersPerLonDeg : 1.0));
    GpsData.AltitudeAMSL = AltM;

    // Hız vektörü (NED m/s: North=+X, East=+Y, Down=-Z)
    FVector VelWorldMs = WorldLinearVelocityUE * 0.01f;
    GpsData.VelNED.X = VelWorldMs.X;
    GpsData.VelNED.Y = VelWorldMs.Y;
    GpsData.VelNED.Z = -VelWorldMs.Z;

    // Yatay yer hızı
    GpsData.GroundSpeedMs = FMath::Sqrt(VelWorldMs.X * VelWorldMs.X + VelWorldMs.Y * VelWorldMs.Y);

    // Seyir açısı (Course over Ground) [0..360]
    if (GpsData.GroundSpeedMs > 0.5f)
    {
        float CourseRad = FMath::Atan2(VelWorldMs.Y, VelWorldMs.X);
        float CourseDeg = FMath::RadiansToDegrees(CourseRad);
        if (CourseDeg < 0.0f) CourseDeg += 360.0f;
        GpsData.HeadingDeg = CourseDeg;
    }
    else
    {
        float YawDeg = FMath::RadiansToDegrees(YawRad);
        if (YawDeg < 0.0f) YawDeg += 360.0f;
        GpsData.HeadingDeg = YawDeg;
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

    // Position [North, East, Down] in meters (relative to origin)
    double DeltaNorthM = (GpsData.LatitudeDeg - HomeLatitudeDeg) * 111139.0;
    double DeltaEastM = (GpsData.LongitudeDeg - HomeLongitudeDeg) * (111139.0 * FMath::Cos(FMath::DegreesToRadians(HomeLatitudeDeg)));
    double DeltaDownM = -(GpsData.AltitudeAMSL - HomeAltitudeAMSL);

    TArray<TSharedPtr<FJsonValue>> PosArr;
    PosArr.Add(MakeShareable(new FJsonValueNumber(DeltaNorthM)));
    PosArr.Add(MakeShareable(new FJsonValueNumber(DeltaEastM)));
    PosArr.Add(MakeShareable(new FJsonValueNumber(DeltaDownM)));
    RootObject->SetArrayField(TEXT("position"), PosArr);

    // Attitude [roll, pitch, yaw] in radians (NED frame)
    TArray<TSharedPtr<FJsonValue>> AttArr;
    AttArr.Add(MakeShareable(new FJsonValueNumber(ImuData.AttitudeEulerRad.X)));
    AttArr.Add(MakeShareable(new FJsonValueNumber(ImuData.AttitudeEulerRad.Y)));
    AttArr.Add(MakeShareable(new FJsonValueNumber(ImuData.AttitudeEulerRad.Z)));
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

    // Realtime simulation flags: no lockstep waiting, direct Sim AHRS (AHRS_EKF_TYPE 10)
    RootObject->SetBoolField(TEXT("no_time_sync"), true);
    RootObject->SetBoolField(TEXT("no_lockstep"), true);

    // Use condensed (no-whitespace) JSON to match ArduPilot parser expectations
    FString OutputString;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

    // ArduPilot SITL JSON parser requires messages to be terminated with '\n'
    return OutputString + TEXT("\n");
}
