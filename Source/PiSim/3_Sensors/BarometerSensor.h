// BarometerSensor.h
// Sanal Barometre ve Pitot Tüpü Sensörü: ISA standart atmosfer modeli ile barometrik basınç,
// dinamik fark basıncı (Pitot tüpü), irtifa ve sıcaklık verisi üretir ve TelemetryGateway'e aktarır.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IPiSimInspectableModule.h"
#include "BarometerSensor.generated.h"

class UPiSimTelemetryGateway;

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimBarometerSensor : public UActorComponent, public IPiSimInspectableModule
{
    GENERATED_BODY()

public:
    UPiSimBarometerSensor();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // IPiSimInspectableModule
    virtual void GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties) override;
    virtual void SetInspectablePropertyFloat(FName PropertyId, float NewValue) override;
    virtual void SetInspectablePropertyString(FName PropertyId, const FString& NewValue) override;
    virtual void TriggerInspectableAction(FName ActionId) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Baro")
    FName TargetBoneName = NAME_None;

    /** Deniz seviyesi standart basınç (hPa / mbar) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Baro")
    float SeaLevelPressureHPa = 1013.25f;

    /** Simülasyon dünyasının zemin rakımı AMSL (metre) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Baro")
    float GroundElevationAMSL = 120.0f;

    /** Ölçülen mutlak atmosfer basıncı (hPa) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Baro")
    float AbsolutePressureHPa = 1013.25f;

    /** Pitot tüpü dinamik fark basıncı (hPa) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Baro")
    float DifferentialPressureHPa = 0.0f;

    /** Barometrik irtifa AMSL (metre) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Baro")
    float PressureAltitudeM = 120.0f;

    /** Çevre sıcaklığı (°C) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Baro")
    float TemperatureC = 15.0f;

    /** Sensör gürültü miktarı (hPa) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Baro")
    float NoiseStdDevHPa = 0.02f;

protected:
    UPROPERTY()
    UPiSimTelemetryGateway* CachedGateway = nullptr;

    void CalculateAtmosphericPhysics(float DeltaTime);
};
