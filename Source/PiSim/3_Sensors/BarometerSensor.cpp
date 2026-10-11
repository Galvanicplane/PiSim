// BarometerSensor.cpp

#include "BarometerSensor.h"
#include "TelemetryGateway.h"
#include "GameFramework/Actor.h"

// @state: WIP - Barometre bileşeni yapıcısı
UPiSimBarometerSensor::UPiSimBarometerSensor()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// @state: WIP - Başlangıçta TelemetryGateway referansını yakalar
void UPiSimBarometerSensor::BeginPlay()
{
    Super::BeginPlay();

    if (GetOwner())
    {
        CachedGateway = GetOwner()->FindComponentByClass<UPiSimTelemetryGateway>();
    }
}

// @state: WIP - Her frame ISA standart atmosfer fiziği hesaplar ve Gateway'e iletir
void UPiSimBarometerSensor::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CachedGateway && GetOwner())
    {
        CachedGateway = GetOwner()->FindComponentByClass<UPiSimTelemetryGateway>();
    }

    CalculateAtmosphericPhysics(DeltaTime);
}

// @state: WIP - Otopark UI parametre tanımları
void UPiSimBarometerSensor::GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties)
{
    FPiSimPropertyDescriptor ElevDesc;
    ElevDesc.PropertyId = TEXT("GroundElevationAMSL");
    ElevDesc.DisplayName = TEXT("Zemin Rakımı (m)");
    ElevDesc.PropertyType = EPiSimPropertyType::Slider;
    ElevDesc.MinValue = 0.0f;
    ElevDesc.MaxValue = 3000.0f;
    ElevDesc.CurrentFloatValue = GroundElevationAMSL;
    ElevDesc.bContinuousTickUpdate = false;
    OutProperties.Add(ElevDesc);

    FPiSimPropertyDescriptor NoiseDesc;
    NoiseDesc.PropertyId = TEXT("NoiseStdDevHPa");
    NoiseDesc.DisplayName = TEXT("Gürültü Sapması (hPa)");
    NoiseDesc.PropertyType = EPiSimPropertyType::Slider;
    NoiseDesc.MinValue = 0.0f;
    NoiseDesc.MaxValue = 0.5f;
    NoiseDesc.CurrentFloatValue = NoiseStdDevHPa;
    NoiseDesc.bContinuousTickUpdate = false;
    OutProperties.Add(NoiseDesc);
}

// @state: WIP - UI sliderından gelen değeri uygular
void UPiSimBarometerSensor::SetInspectablePropertyFloat(FName PropertyId, float NewValue)
{
    if (PropertyId == TEXT("GroundElevationAMSL"))
    {
        GroundElevationAMSL = FMath::Clamp(NewValue, -500.0f, 10000.0f);
    }
    else if (PropertyId == TEXT("NoiseStdDevHPa"))
    {
        NoiseStdDevHPa = FMath::Clamp(NewValue, 0.0f, 2.0f);
    }
}

// @state: WIP - UI metin kutusu işlemi
void UPiSimBarometerSensor::SetInspectablePropertyString(FName PropertyId, const FString& NewValue)
{
}

// @state: WIP - UI buton aksiyonu
void UPiSimBarometerSensor::TriggerInspectableAction(FName ActionId)
{
}

// @state: WIP - ISA Barometre & Pitot dinamik basınç hesabı
void UPiSimBarometerSensor::CalculateAtmosphericPhysics(float DeltaTime)
{
    if (!GetOwner())
    {
        return;
    }

    // UE Z ekseni santimetredir (100 cm = 1 m)
    FVector WorldLoc = GetOwner()->GetActorLocation();
    float AltitudeAboveGroundM = WorldLoc.Z / 100.0f;
    PressureAltitudeM = GroundElevationAMSL + AltitudeAboveGroundM;

    // ISA Standart Atmosfer Modeli: T = 15 - 0.0065 * h (°C)
    TemperatureC = 15.0f - (0.0065f * PressureAltitudeM);
    float TempKelvin = TemperatureC + 273.15f;

    // P = P0 * (1 - 0.0065 * h / 288.15)^5.25588
    float BaroFactor = 1.0f - (0.0065f * PressureAltitudeM / 288.15f);
    BaroFactor = FMath::Max(0.01f, BaroFactor);
    AbsolutePressureHPa = SeaLevelPressureHPa * FMath::Pow(BaroFactor, 5.25588f);

    // Gürültü ekle
    if (NoiseStdDevHPa > 0.0f)
    {
        AbsolutePressureHPa += FMath::RandRange(-NoiseStdDevHPa, NoiseStdDevHPa);
    }

    // Hava Yoğunluğu (Ideal Gaz: rho = P * 100 / (R * T))
    const float R_Specific = 287.05f; // J/(kg*K)
    float AirDensityKgM3 = (AbsolutePressureHPa * 100.0f) / (R_Specific * TempKelvin);

    // Pitot Tüpü Dinamik Basınç: q = 0.5 * rho * v^2 (hPa cinsinden = Pa / 100)
    FVector VelocityUE = GetOwner()->GetVelocity();
    float SpeedMs = VelocityUE.Size() / 100.0f;
    float DynamicPressurePa = 0.5f * AirDensityKgM3 * (SpeedMs * SpeedMs);
    DifferentialPressureHPa = DynamicPressurePa / 100.0f;

    // Gateway'e besle
    if (CachedGateway)
    {
        FPiSimBaroData BaroData;
        BaroData.AbsPressureHPa = AbsolutePressureHPa;
        BaroData.DiffPressureHPa = DifferentialPressureHPa;
        BaroData.PressureAltitudeM = PressureAltitudeM;
        BaroData.TemperatureC = TemperatureC;
        CachedGateway->FeedBaroData(BaroData);
    }
}
