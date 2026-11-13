// GPSSensor.h
// GPS Alıcısı: Simülasyon konumunu gerçek dünya koordinatlarına çevirir. Sahte başlangıç konumu (Ankara, İstanbul vb.) yapılandırmasını destekler.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IPiSimInspectableModule.h"
#include "IPiSimActuatorInterface.h"
#include "GPSSensor.generated.h"

class APiSimVehicle;

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimGPSSensor : public UActorComponent, public IPiSimInspectableModule, public IPiSimActuatorInterface
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

    // IPiSimActuatorInterface (Modül kimlik & ACK sözleşmesi)
    virtual void SetActuatorNormalized(float Value) override {}
    virtual void SetActuatorDirect(float PhysicalValue) override {}
    virtual void SetActuatorPWM(int32 InPWM) override {}
    virtual float GetActuatorNormalized() const override { return 0.0f; }
    virtual FName GetTargetBoneName() const override { return TargetBoneName; }
    virtual void SetTargetBoneName(FName InBoneName) override { TargetBoneName = InBoneName; }
    virtual FString GetActuatorType() const override { return TEXT("GPSSensor"); }
    virtual int32 GetArduPilotChannel() const override { return ServoChannel; }
    virtual void SetArduPilotChannel(int32 InChannel) override { ServoChannel = InChannel; }
    virtual FVector GetActuatorAxis() const override { return SensorLocalOffset; }
    virtual void SetActuatorAxis(const FVector& InAxis) override { SensorLocalOffset = InAxis; }
    virtual void InitializeModuleIdentity(APiSimVehicle* InVehicle) override;
    virtual void HandleAckMessage(const FString& TopicName, int32 AckCode) override { LastAckCode = AckCode; }

    UPROPERTY()
    APiSimVehicle* ParentVehicle = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    FString ParentVehicleName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    int32 VehicleIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    int32 ServoChannel = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GPS")
    FName TargetBoneName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|GPS")
    FVector SensorLocalOffset = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Communication")
    int32 LastAckCode = 2;

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
    APiSimVehicle* CachedVehicle = nullptr;

    FVector InitialWorldLocation = FVector::ZeroVector;
    bool bInitializedLocation = false;
};
