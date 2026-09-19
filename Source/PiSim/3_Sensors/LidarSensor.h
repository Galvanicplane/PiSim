// LidarSensor.h
// LiDAR Mesafe ve Lazer Tarama Sensörü (Taslak)

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

    // IPiSimInspectableModule
    virtual void GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties) override {}
    virtual void SetInspectablePropertyFloat(FName PropertyId, float NewValue) override {}
    virtual void SetInspectablePropertyString(FName PropertyId, const FString& NewValue) override {}
    virtual void TriggerInspectableAction(FName ActionId) override {}

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Lidar")
    FName TargetBoneName = NAME_None;
};
