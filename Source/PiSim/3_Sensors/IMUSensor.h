// IMUSensor.h
// IMU Sensörü: 3-Eksen İvmeölçer, Jiroskop ve Oryantasyon verisi üretir ve TelemetryGateway'e besler.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IPiSimInspectableModule.h"
#include "IPiSimActuatorInterface.h"
#include "IMUSensor.generated.h"

class APiSimVehicle;

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimIMUSensor : public UActorComponent, public IPiSimInspectableModule, public IPiSimActuatorInterface
{
    GENERATED_BODY()

public:
    UPiSimIMUSensor();

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
    virtual FString GetActuatorType() const override { return TEXT("IMUSensor"); }
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|IMU")
    FVector SensorLocalOffset = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Communication")
    int32 LastAckCode = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|IMU")
    FName TargetBoneName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|IMU")
    float NoiseIntensity = 0.01f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|IMU")
    FVector LinearAcceleration = FVector::ZeroVector; // m/s^2

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|IMU")
    FVector AngularVelocityDeg = FVector::ZeroVector; // deg/s

protected:
    UPROPERTY()
    APiSimVehicle* CachedVehicle = nullptr;

    FVector PreviousVelocity = FVector::ZeroVector;
    bool bHasPreviousVelocity = false;
};
