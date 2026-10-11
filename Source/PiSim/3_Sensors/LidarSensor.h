// LidarSensor.h
// LiDAR Mesafe ve Lazer Tarama Sensörü: Işın taraması (Raycast) ile mesafe ölçer ve TelemetryGateway'e nokta verisi aktarır.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IPiSimInspectableModule.h"
#include "LidarSensor.generated.h"

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimLidarSensor : public UActorComponent, public IPiSimInspectableModule
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
