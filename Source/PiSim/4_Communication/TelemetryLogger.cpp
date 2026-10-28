// TelemetryLogger.cpp
// 1 Dakikalık SQL Batch Telemetri Günlük Kaydedici Uygulaması

#include "TelemetryLogger.h"
#include "TelemetryGateway.h"
#include "Vehicle.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// @state: WIP - Yapıcı: Her frame tick aktif, varsayılan periyot 60 sn
UPiSimTelemetryLogger::UPiSimTelemetryLogger()
{
    PrimaryComponentTick.bCanEverTick = true;
    FlushIntervalSec = 10.0f; // 10 Saniyede bir SQL'e yaz (Kullanıcı talebi)
    SampleRateHz = 5.0f;      // Saniyede 5 örnek
    bEnableSqlLogging = true;
}

// @state: WIP - BeginPlay: Gateway ve Araç referanslarını önbelleğe alır
void UPiSimTelemetryLogger::BeginPlay()
{
    Super::BeginPlay();

    if (GetOwner())
    {
        CachedVehicle = Cast<APiSimVehicle>(GetOwner());
        CachedGateway = GetOwner()->FindComponentByClass<UPiSimTelemetryGateway>();
    }

    // SQL dosyasını ve başlığını hazırla (Mevcut araçların kayıtlarını silme!)
    if (bEnableSqlLogging)
    {
        EnsureSqlHeaderExists(GetSqlFilePath());
        // Yeni araç açılır açılmaz ilk kaydını SQL'e yazarak Pi 5'in hemen görmesini sağla
        SampleCurrentState();
        FlushBatchToSql();
    }
}

// @state: WIP - EndPlay: Kalan son batch verilerini SQL'e yazar
void UPiSimTelemetryLogger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (bEnableSqlLogging)
    {
        FlushBatchToSql();
    }
    Super::EndPlay(EndPlayReason);
}

// @state: WIP - Tick: Belirlenen frekansta örnek toplar, her 60 saniyede bir batch SQL yazar
void UPiSimTelemetryLogger::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableSqlLogging)
    {
        return;
    }

    // 1. Örnekleme Zamanlayıcısı (2 Hz)
    SampleTimer += DeltaTime;
    float SampleInterval = 1.0f / FMath::Max(0.1f, SampleRateHz);
    if (SampleTimer >= SampleInterval)
    {
        SampleCurrentState();
        SampleTimer = 0.0f;
    }

    // 2. Batch SQL Yazma Zamanlayıcısı (60 sn - 1 Dakika)
    FlushTimer += DeltaTime;
    if (FlushTimer >= FlushIntervalSec)
    {
        FlushBatchToSql();
        FlushTimer = 0.0f;
    }
}

// @state: WIP - Gateway ve Araçtan anlık telemetri durumunu çeker ve bellekte toplar
void UPiSimTelemetryLogger::SampleCurrentState()
{
    FPiSimTelemetryRow Row;
    Row.TimestampSec = (GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0;
    Row.VehicleId = (CachedVehicle) ? CachedVehicle->VehicleId : 0;
    Row.VehicleCode = (CachedVehicle && !CachedVehicle->VehicleCode.IsEmpty()) ? CachedVehicle->VehicleCode : FString::Printf(TEXT("rover_%02d"), Row.VehicleId + 1);

    if (CachedGateway)
    {
        FPiSimIMUData Imu;
        if (CachedGateway->GetLatestIMUData(Imu))
        {
            FRotator Rot = Imu.Orientation.Rotator();
            Row.RollDeg = Rot.Roll;
            Row.PitchDeg = Rot.Pitch;
            Row.YawDeg = Rot.Yaw;
        }

        FPiSimBaroData Baro;
        if (CachedGateway->GetLatestBaroData(Baro))
        {
            Row.AltitudeM = Baro.PressureAltitudeM;
            Row.AirspeedMs = FMath::Sqrt(FMath::Max(0.0f, Baro.DiffPressureHPa * 100.0f * 2.0f / 1.225f));
        }

        FPiSimGPSData Gps;
        if (CachedGateway->GetLatestGPSData(Gps))
        {
            Row.Latitude = Gps.Latitude;
            Row.Longitude = Gps.Longitude;
        }
    }

    // Varsayılan pil ve mod
    Row.BatteryPct = 95.0f;
    Row.FlightMode = TEXT("AUTO_MISSION");

    // 1 dakikalık batch havuzuna ekle
    PendingBatch.Add(Row);

    // Dashboard UI için halka bellek havuzuna ekle (Maksimum 300 satır)
    RecentRows.Add(Row);
    if (RecentRows.Num() > 300)
    {
        RecentRows.RemoveAt(0);
    }
}

// @state: WIP - Birikmiş örnekleri SQL dosyasına BEGIN TRANSACTION ... COMMIT bloğuyla ekler
void UPiSimTelemetryLogger::FlushBatchToSql()
{
    if (PendingBatch.Num() == 0)
    {
        return;
    }

    FString FilePath = GetSqlFilePath();
    EnsureSqlHeaderExists(FilePath);

    FString SqlBatch;
    SqlBatch.Reserve(PendingBatch.Num() * 160);

    SqlBatch.Append(TEXT("\n-- =========================================================================\n"));
    SqlBatch.Append(FString::Printf(TEXT("-- 🕒 1 DAKİKALIK BATCH KAYDI [%s] (Satır Sayısı: %d, Araç ID: %d)\n"), 
        *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")), PendingBatch.Num(), (CachedVehicle ? CachedVehicle->VehicleId : 0)));
    SqlBatch.Append(TEXT("-- =========================================================================\n"));
    SqlBatch.Append(TEXT("BEGIN TRANSACTION;\n"));

    for (const FPiSimTelemetryRow& Row : PendingBatch)
    {
        SqlBatch.Append(FString::Printf(
            TEXT("INSERT INTO vehicle_telemetry (timestamp_sec, vehicle_code, vehicle_id, roll_deg, pitch_deg, yaw_deg, altitude_m, airspeed_ms, battery_pct, lat, lon, flight_mode) VALUES (%.2f, '%s', %d, %.2f, %.2f, %.2f, %.2f, %.2f, %.1f, %.6f, %.6f, '%s');\n"),
            Row.TimestampSec,
            *Row.VehicleCode,
            Row.VehicleId,
            Row.RollDeg,
            Row.PitchDeg,
            Row.YawDeg,
            Row.AltitudeM,
            Row.AirspeedMs,
            Row.BatteryPct,
            Row.Latitude,
            Row.Longitude,
            *Row.FlightMode
        ));
    }

    SqlBatch.Append(TEXT("COMMIT;\n"));

    // Dosyaya ekleme modu (Append) ile kaydet
    if (FFileHelper::SaveStringToFile(SqlBatch, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append))
    {
        TotalLoggedRows += PendingBatch.Num();
        UE_LOG(LogTemp, Log, TEXT("💾 [UPiSimTelemetryLogger] 10 Saniyelik Telemetri Batch SQL Veritabanına Yazıldı: %d satır (Toplam: %d). Dosya: %s"),
            PendingBatch.Num(), TotalLoggedRows, *FilePath);
        if (GEngine)
        {
            FString ScreenMsg = FString::Printf(TEXT("💾 [SQL BATCH] %s: %d satır SQL'e yazıldı (Toplam: %d) -> flight_telemetry.sql"),
                (CachedVehicle ? *CachedVehicle->VehicleCode : TEXT("rover")), PendingBatch.Num(), TotalLoggedRows);
            GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor::Cyan, ScreenMsg);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ [UPiSimTelemetryLogger] SQL dosyasına yazılamadı: %s"), *FilePath);
    }

    PendingBatch.Empty();
}

// @state: WIP - Dashboard için en son telemetri satırlarını JSON tablosu olarak döner
FString UPiSimTelemetryLogger::GetRecentTelemetryTableJson(int32 MaxRows) const
{
    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
    RootObject->SetNumberField(TEXT("total_logged"), TotalLoggedRows);
    RootObject->SetNumberField(TEXT("pending_batch_count"), PendingBatch.Num());
    RootObject->SetNumberField(TEXT("flush_interval_sec"), FlushIntervalSec);
    RootObject->SetStringField(TEXT("sql_file"), GetSqlFilePath());

    TArray<TSharedPtr<FJsonValue>> RowsArray;
    int32 StartIdx = FMath::Max(0, RecentRows.Num() - MaxRows);

    for (int32 i = StartIdx; i < RecentRows.Num(); ++i)
    {
        const FPiSimTelemetryRow& Row = RecentRows[i];
        TSharedPtr<FJsonObject> RowObj = MakeShareable(new FJsonObject);
        RowObj->SetNumberField(TEXT("time"), Row.TimestampSec);
        RowObj->SetStringField(TEXT("vehicle_code"), Row.VehicleCode);
        RowObj->SetNumberField(TEXT("vehicle_id"), Row.VehicleId);
        RowObj->SetNumberField(TEXT("roll"), Row.RollDeg);
        RowObj->SetNumberField(TEXT("pitch"), Row.PitchDeg);
        RowObj->SetNumberField(TEXT("yaw"), Row.YawDeg);
        RowObj->SetNumberField(TEXT("altitude"), Row.AltitudeM);
        RowObj->SetNumberField(TEXT("speed"), Row.AirspeedMs * 3.6f); // km/s
        RowObj->SetNumberField(TEXT("battery"), Row.BatteryPct);
        RowObj->SetNumberField(TEXT("lat"), Row.Latitude);
        RowObj->SetNumberField(TEXT("lon"), Row.Longitude);
        RowObj->SetStringField(TEXT("mode"), Row.FlightMode);

        RowsArray.Add(MakeShareable(new FJsonValueObject(RowObj)));
    }

    RootObject->SetArrayField(TEXT("rows"), RowsArray);

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

    return OutputString;
}

// @state: WIP - SQL dosyasının tam yolunu döner (Saved/Telemetry/flight_telemetry.sql)
FString UPiSimTelemetryLogger::GetSqlFilePath() const
{
    return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Telemetry/flight_telemetry.sql"));
}

// @state: WIP - SQL dosyasının dizinini ve CREATE TABLE başlığını oluşturur
void UPiSimTelemetryLogger::EnsureSqlHeaderExists(const FString& FilePath)
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.FileExists(*FilePath))
    {
        ResetSqlFile(FilePath);
    }
}

// @state: WIP - SQL dosyasını sıfırlar ve temiz bir tablo şeması oluşturur
void UPiSimTelemetryLogger::ResetSqlFile(const FString& FilePath)
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    FString Directory = FPaths::GetPath(FilePath);

    if (!PlatformFile.DirectoryExists(*Directory))
    {
        PlatformFile.CreateDirectoryTree(*Directory);
    }

    FString TableSchema = TEXT("-- =========================================================================\n")
                          TEXT("-- PiSim Uçuş Telemetri Veritabanı (SQLite & SQL Standart Uyumlu)\n")
                          TEXT("-- Oturum Başlangıcında Otomatik Sıfırlanır (Eski veriler temizlenir)\n")
                          TEXT("-- =========================================================================\n\n")
                          TEXT("DROP TABLE IF EXISTS vehicle_telemetry;\n")
                          TEXT("CREATE TABLE IF NOT EXISTS vehicle_telemetry (\n")
                          TEXT("    id INTEGER PRIMARY KEY AUTOINCREMENT,\n")
                          TEXT("    timestamp_sec REAL NOT NULL,\n")
                          TEXT("    vehicle_code TEXT NOT NULL,\n")
                          TEXT("    vehicle_id INTEGER NOT NULL,\n")
                          TEXT("    roll_deg REAL,\n")
                          TEXT("    pitch_deg REAL,\n")
                          TEXT("    yaw_deg REAL,\n")
                          TEXT("    altitude_m REAL,\n")
                          TEXT("    airspeed_ms REAL,\n")
                          TEXT("    battery_pct REAL,\n")
                          TEXT("    lat REAL,\n")
                          TEXT("    lon REAL,\n")
                          TEXT("    flight_mode TEXT\n")
                          TEXT(");\n\n")
                          TEXT("CREATE INDEX IF NOT EXISTS idx_telemetry_time ON vehicle_telemetry(timestamp_sec);\n")
                          TEXT("CREATE INDEX IF NOT EXISTS idx_telemetry_code ON vehicle_telemetry(vehicle_code);\n")
                          TEXT("CREATE INDEX IF NOT EXISTS idx_telemetry_vehicle ON vehicle_telemetry(vehicle_id);\n\n");

    FFileHelper::SaveStringToFile(TableSchema, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Log, TEXT("🧹 [UPiSimTelemetryLogger] Telemetri SQL Veritabanı Temizlendi ve Sıfırlandı: %s"), *FilePath);
}
