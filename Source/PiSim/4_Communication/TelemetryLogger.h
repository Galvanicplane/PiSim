// TelemetryLogger.h
// 1 Dakikalık SQL Batch Telemetri Günlük Kaydedici:
// 60Hz sensör döngüsünü aksatmadan periyodik örnekleme yapar (2Hz) ve her 60 saniyede bir
// birikmiş telemetri satırlarını SQL veritabanı dosyasına (Saved/Telemetry/flight_telemetry.sql)
// BEGIN TRANSACTION ... COMMIT bloğu ile atomik olarak yazar.
// Ayrıca WebUI Dashboard'un canlı tablo/grafik çizmesi için JSON tablosu sunar.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TelemetryLogger.generated.h"

class UPiSimTelemetryGateway;
class APiSimVehicle;

USTRUCT(BlueprintType)
struct PISIM_API FPiSimTelemetryRow
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Telemetry")
    double TimestampSec = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Telemetry")
    FString VehicleCode = TEXT("rover_01");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Telemetry")
    int32 VehicleId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Telemetry")
    float RollDeg = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Telemetry")
    float PitchDeg = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Telemetry")
    float YawDeg = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Telemetry")
    float AltitudeM = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Telemetry")
    float AirspeedMs = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Telemetry")
    float BatteryPct = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Telemetry")
    double Latitude = 39.9334;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Telemetry")
    double Longitude = 32.8597;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Telemetry")
    FString FlightMode = TEXT("STABILIZE");
};

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimTelemetryLogger : public UActorComponent
{
    GENERATED_BODY()

public:
    UPiSimTelemetryLogger();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Manuel olarak birikmiş batch verisini anında SQL'e yazar */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Telemetry")
    void FlushBatchToSql();

    /** Dashboard tablosu için en son kayıtlı satırları JSON olarak döner */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Telemetry")
    FString GetRecentTelemetryTableJson(int32 MaxRows = 100) const;

    /** SQL dosyasının tam yolunu döner */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Telemetry")
    FString GetSqlFilePath() const;

    /** Toplam kaydedilen satır sayısı */
    UFUNCTION(BlueprintCallable, Category = "PiSim|Telemetry")
    int32 GetTotalLoggedRows() const { return TotalLoggedRows; }

    /** Batch yazma periyodu (saniye) - Varsayılan 60 sn (1 dakika) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Telemetry")
    float FlushIntervalSec = 60.0f;

    /** Örnekleme frekansı (Hz) - 2Hz = her 0.5 saniyede 1 satır */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Telemetry")
    float SampleRateHz = 2.0f;

    /** SQL Loglama aktif mi? */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Telemetry")
    bool bEnableSqlLogging = true;

protected:
    UPROPERTY()
    UPiSimTelemetryGateway* CachedGateway = nullptr;

    UPROPERTY()
    APiSimVehicle* CachedVehicle = nullptr;

    // Henüz SQL dosyasına yazılmamış (1 dakika içinde toplanan) satırlar
    TArray<FPiSimTelemetryRow> PendingBatch;

    // Dashboard'a anlık tablo olarak sunulan halka bellek (Son 300 satır)
    TArray<FPiSimTelemetryRow> RecentRows;

    float SampleTimer = 0.0f;
    float FlushTimer = 0.0f;
    int32 TotalLoggedRows = 0;

    void SampleCurrentState();
    void EnsureSqlHeaderExists(const FString& FilePath);
    void ResetSqlFile(const FString& FilePath);
};
