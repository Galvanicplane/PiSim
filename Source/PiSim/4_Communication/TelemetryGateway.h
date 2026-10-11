// TelemetryGateway.h
// Ortak Çıkış Kapısı: Tüm sensör verilerinin toplandığı ve dış köprülere (Pi5, ArduPilot, PX4) dağıtıldığı merkezi bileşen.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HAL/CriticalSection.h"
#include "TelemetryGateway.generated.h"

USTRUCT(BlueprintType)
struct PISIM_API FPiSimIMUData
{
    GENERATED_BODY()

    /** Body-frame ivme (m/s^2) - Yerçekimi tepkisi dahil */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|IMU")
    FVector LinearAcceleration = FVector::ZeroVector;

    /** Body-frame açısal hız (rad/s) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|IMU")
    FVector AngularVelocityRad = FVector::ZeroVector;

    /** Oryantasyon kuaterniyonu (World NED to Body) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|IMU")
    FQuat Orientation = FQuat::Identity;

    /** Manyetometre verisi (Gauss) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|IMU")
    FVector MagnetometerGauss = FVector(0.22f, 0.01f, 0.42f);
};

USTRUCT(BlueprintType)
struct PISIM_API FPiSimBaroData
{
    GENERATED_BODY()

    /** Mutlak basınç (hPa / mbar) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Baro")
    float AbsPressureHPa = 1013.25f;

    /** Pitot tüpü dinamik fark basıncı (hPa) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Baro")
    float DiffPressureHPa = 0.0f;

    /** Barometrik irtifa AMSL (metre) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Baro")
    float PressureAltitudeM = 0.0f;

    /** Ortam sıcaklığı (°C) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Baro")
    float TemperatureC = 15.0f;
};

USTRUCT(BlueprintType)
struct PISIM_API FPiSimGPSData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GPS")
    double Latitude = 39.9334; // Ankara default

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GPS")
    double Longitude = 32.8597;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GPS")
    float AltitudeMeters = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GPS")
    float SpeedKmh = 0.0f;

    /** NED hız vektörü (m/s) [Kuzey, Doğu, Aşağı] */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GPS")
    FVector VelocityNED = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GPS")
    int32 FixType = 3; // 3D Fix

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GPS")
    int32 Satellites = 14;
};

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimTelemetryGateway : public UActorComponent
{
    GENERATED_BODY()

public:
    UPiSimTelemetryGateway();

    // Sensör besleme fonksiyonları
    UFUNCTION(BlueprintCallable, Category = "PiSim|Telemetry")
    void FeedIMUData(const FPiSimIMUData& InData);

    UFUNCTION(BlueprintCallable, Category = "PiSim|Telemetry")
    void FeedBaroData(const FPiSimBaroData& InData);

    UFUNCTION(BlueprintCallable, Category = "PiSim|Telemetry")
    void FeedGPSData(const FPiSimGPSData& InData);

    UFUNCTION(BlueprintCallable, Category = "PiSim|Telemetry")
    void FeedCameraFrame(const TArray<uint8>& InJpegBytes);

    // Dış köprü okuma fonksiyonları (Thread-safe)
    bool GetLatestIMUData(FPiSimIMUData& OutData);
    bool GetLatestBaroData(FPiSimBaroData& OutData);
    bool GetLatestGPSData(FPiSimGPSData& OutData);
    bool GetLatestCameraFrame(TArray<uint8>& OutJpegBytes);

    // ArduPilot / SITL için JSON çıktı üretici
    FString BuildSitlJsonState(double SimTimestampSec = 0.0);

protected:
    FCriticalSection TelemetryLock;
    FCriticalSection FrameLock;

    FPiSimIMUData LatestIMU;
    FPiSimBaroData LatestBaro;
    FPiSimGPSData LatestGPS;
    TArray<uint8> LatestCameraJpeg;

    uint32 FrameCounter = 0;
};
