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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|IMU")
    FVector LinearAcceleration = FVector::ZeroVector; // m/s^2

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|IMU")
    FVector AngularVelocity = FVector::ZeroVector; // deg/s or rad/s

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|IMU")
    FQuat Orientation = FQuat::Identity;
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
    void FeedGPSData(const FPiSimGPSData& InData);

    UFUNCTION(BlueprintCallable, Category = "PiSim|Telemetry")
    void FeedCameraFrame(const TArray<uint8>& InJpegBytes);

    // Dış köprü okuma fonksiyonları (Thread-safe)
    bool GetLatestIMUData(FPiSimIMUData& OutData);
    bool GetLatestGPSData(FPiSimGPSData& OutData);
    bool GetLatestCameraFrame(TArray<uint8>& OutJpegBytes);

    // ArduPilot / SITL için JSON çıktı üretici
    FString BuildSitlJsonState();

protected:
    FCriticalSection TelemetryLock;
    FCriticalSection FrameLock;

    FPiSimIMUData LatestIMU;
    FPiSimGPSData LatestGPS;
    TArray<uint8> LatestCameraJpeg;

    uint32 FrameCounter = 0;
};
