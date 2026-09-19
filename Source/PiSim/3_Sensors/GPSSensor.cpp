// GPSSensor.cpp

#include "GPSSensor.h"
#include "TelemetryGateway.h"
#include "GameFramework/Actor.h"

// @state: WIP - GPS sensörü bileşeni yapıcısı
UPiSimGPSSensor::UPiSimGPSSensor()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

// @state: WIP - Başlangıç konumunu yakalar ve Gateway referansını bağlar
void UPiSimGPSSensor::BeginPlay()
{
    Super::BeginPlay();

    if (GetOwner())
    {
        CachedGateway = GetOwner()->FindComponentByClass<UPiSimTelemetryGateway>();
        InitialWorldLocation = GetOwner()->GetActorLocation();
        bInitializedLocation = true;
    }
}

// @state: WIP - Her frame araç yer değiştirmesine göre anlık GPS koordinatı hesaplar
void UPiSimGPSSensor::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!GetOwner())
    {
        return;
    }

    if (!bInitializedLocation)
    {
        InitialWorldLocation = GetOwner()->GetActorLocation();
        bInitializedLocation = true;
    }

    FVector CurrentWorldLocation = GetOwner()->GetActorLocation();
    FVector DeltaMeters = (CurrentWorldLocation - InitialWorldLocation) / 100.0f; // UE cm to Meters

    // Enlem/Boylam dönüşümü (WGS84 basitleştirilmiş düzlem projeksiyonu)
    // 1 derece enlem ~= 111320 metre
    double DeltaLat = DeltaMeters.X / 111320.0;
    double LatRad = FMath::DegreesToRadians(OriginLatitude);
    double DeltaLon = DeltaMeters.Y / (111320.0 * FMath::Cos(LatRad));

    CurrentLatitude = OriginLatitude + DeltaLat;
    CurrentLongitude = OriginLongitude + DeltaLon;
    CurrentAltitudeMeters = OriginAltitudeMeters + DeltaMeters.Z;

    FVector Velocity = GetOwner()->GetVelocity();
    CurrentSpeedKmh = (Velocity.Size() / 100.0f) * 3.6f;

    if (CachedGateway)
    {
        FPiSimGPSData GpsData;
        GpsData.Latitude = CurrentLatitude;
        GpsData.Longitude = CurrentLongitude;
        GpsData.AltitudeMeters = CurrentAltitudeMeters;
        GpsData.SpeedKmh = CurrentSpeedKmh;
        CachedGateway->FeedGPSData(GpsData);
    }
}

// @state: WIP - Başlangıç orijin koordinatlarını günceller
void UPiSimGPSSensor::SetOriginCoordinates(double InLat, double InLon, float InAlt)
{
    OriginLatitude = InLat;
    OriginLongitude = InLon;
    OriginAltitudeMeters = InAlt;
}

// @state: WIP - Şehir önayarlarını uygular (Ankara, İstanbul, İzmir vb.)
void UPiSimGPSSensor::SetOriginPreset(const FString& PresetName)
{
    if (PresetName == TEXT("Ankara"))
    {
        SetOriginCoordinates(39.9334, 32.8597, 850.0f);
    }
    else if (PresetName == TEXT("Istanbul"))
    {
        SetOriginCoordinates(41.0082, 28.9784, 40.0f);
    }
    else if (PresetName == TEXT("Izmir"))
    {
        SetOriginCoordinates(38.4237, 27.1428, 10.0f);
    }
}

// @state: WIP - Otopark UI parametre listesini döner
void UPiSimGPSSensor::GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties)
{
    FPiSimPropertyDescriptor PresetDesc;
    PresetDesc.PropertyId = TEXT("Preset");
    PresetDesc.DisplayName = TEXT("Başlangıç Konumu Şehir");
    PresetDesc.PropertyType = EPiSimPropertyType::Dropdown;
    PresetDesc.CurrentStringValue = TEXT("Ankara");
    PresetDesc.DropdownOptions = { TEXT("Ankara"), TEXT("Istanbul"), TEXT("Izmir") };
    PresetDesc.bContinuousTickUpdate = false;
    OutProperties.Add(PresetDesc);

    FPiSimPropertyDescriptor AltDesc;
    AltDesc.PropertyId = TEXT("OriginAltitudeMeters");
    AltDesc.DisplayName = TEXT("Başlangıç Rakımı (Metre)");
    AltDesc.PropertyType = EPiSimPropertyType::Slider;
    AltDesc.MinValue = 0.0f;
    AltDesc.MaxValue = 3000.0f;
    AltDesc.CurrentFloatValue = OriginAltitudeMeters;
    AltDesc.bContinuousTickUpdate = false;
    OutProperties.Add(AltDesc);
}

// @state: WIP - UI sliderından gelen rakım değerini günceller
void UPiSimGPSSensor::SetInspectablePropertyFloat(FName PropertyId, float NewValue)
{
    if (PropertyId == TEXT("OriginAltitudeMeters"))
    {
        OriginAltitudeMeters = NewValue;
    }
}

// @state: WIP - UI açılır listesinden seçilen şehri uygular
void UPiSimGPSSensor::SetInspectablePropertyString(FName PropertyId, const FString& NewValue)
{
    if (PropertyId == TEXT("Preset"))
    {
        SetOriginPreset(NewValue);
    }
}

// @state: WIP - UI aksiyon butonunu işler
void UPiSimGPSSensor::TriggerInspectableAction(FName ActionId)
{
    if (ActionId == TEXT("ResetGPS"))
    {
        if (GetOwner())
        {
            InitialWorldLocation = GetOwner()->GetActorLocation();
        }
    }
}
