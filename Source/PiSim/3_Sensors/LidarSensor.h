// LidarSensor.h
// LiDAR Mesafe ve Lazer Tarama Sensörü: Işın taraması (Raycast) ile mesafe ölçer ve TelemetryGateway'e nokta verisi aktarır.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IPiSimInspectableModule.h"
#include "IPiSimActuatorInterface.h"
#include "LidarSensor.generated.h"

class APiSimVehicle;

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimLidarSensor : public UActorComponent, public IPiSimInspectableModule, public IPiSimActuatorInterface
{
    GENERATED_BODY()

public:
    UPiSimLidarSensor();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // IPiSimInspectableModule
    virtual void GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties) override;
    virtual void SetInspectablePropertyFloat(FName PropertyId, float NewValue) override;
    virtual void SetInspectablePropertyString(FName PropertyId, const FString& NewValue) override;
    virtual void TriggerInspectableAction(FName ActionId) override;

    // IPiSimActuatorInterface
    virtual void SetActuatorNormalized(float Value) override {}
    virtual void SetActuatorDirect(float PhysicalValue) override {}
    virtual void SetActuatorPWM(int32 InPWM) override {}
    virtual float GetActuatorNormalized() const override { return 0.0f; }
    virtual FName GetTargetBoneName() const override { return TargetBoneName; }
    virtual void SetTargetBoneName(FName InBoneName) override { TargetBoneName = InBoneName; }
    virtual FString GetActuatorType() const override { return TEXT("LidarSensor"); }
    virtual int32 GetArduPilotChannel() const override { return ServoChannel; }
    virtual void SetArduPilotChannel(int32 InChannel) override { ServoChannel = InChannel; }
    virtual FVector GetActuatorAxis() const override { return SensorLocalOffset; }
    virtual void SetActuatorAxis(const FVector& InAxis) override { SensorLocalOffset = InAxis; }
    virtual void InitializeModuleIdentity(APiSimVehicle* InVehicle) override;
    virtual void HandleAckMessage(const FString& TopicName, int32 AckCode) override { if (TopicName.Contains(TEXT("lidar"), ESearchCase::IgnoreCase) || TopicName.Contains(TEXT("scan"), ESearchCase::IgnoreCase)) { LastAckCode = AckCode; } }

    UPROPERTY()
    APiSimVehicle* ParentVehicle = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    FString ParentVehicleName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    int32 VehicleIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Identity")
    int32 ServoChannel = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lidar")
    FVector SensorLocalOffset = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Communication")
    int32 LastAckCode = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lidar")
    FName TargetBoneName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lidar")
    float MaxRangeMeters = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lidar")
    int32 Channels = 16;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lidar")
    float ScanRateHz = 10.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PiSim|Lidar")
    float ClosestObstacleDistanceM = 50.0f;

protected:
    float ScanTimer = 0.0f;
    void PerformScan();
};
