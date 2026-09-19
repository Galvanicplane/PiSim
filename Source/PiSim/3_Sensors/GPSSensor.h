// GPSSensor.h
// GPS Alıcısı: Simülasyon konumunu gerçek dünya koordinatlarına çevirir. Sahte başlangıç konumu (Ankara, İstanbul vb.) yapılandırmasını destekler.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IPiSimInspectableModule.h"
#include "GPSSensor.generated.h"

class UPiSimTelemetryGateway;

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimGPSSensor : public UActorComponent, public IPiSimInspectableModule
{
    GENERATED_BODY()

public:
    UPiSimGPSSensor();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // IPiSimInspectableModule
    virtual void GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties) override;
    virtual void SetInspectablePropertyFloat(FName PropertyId, float NewValue) override;
    virtual void SetInspectablePropertyString(FName PropertyId, const FString& NewValue) override;
    virtual void TriggerInspectableAction(FName ActionId) override;

    UFUNCTION(BlueprintCallable, Category = "PiSim|GPS")
    void SetOriginCoordinates(double InLat, double InLon, float InAlt);

    UFUNCTION(BlueprintCallable, Category = "PiSim|GPS")
    void SetOriginPreset(const FString& PresetName);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GPS")
    double OriginLatitude = 39.9334; // Ankara Kızılay

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GPS")
    double OriginLongitude = 32.8597;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GPS")
    float OriginAltitudeMeters = 850.0f; // Ankara rakım

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|GPS")
    double CurrentLatitude = 39.9334;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|GPS")
    double CurrentLongitude = 32.8597;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|GPS")
    float CurrentAltitudeMeters = 850.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|GPS")
    float CurrentSpeedKmh = 0.0f;

protected:
    UPROPERTY()
    UPiSimTelemetryGateway* CachedGateway = nullptr;

    FVector InitialWorldLocation = FVector::ZeroVector;
    bool bInitializedLocation = false;
};
