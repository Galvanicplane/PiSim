// IMUSensor.h
// IMU Sensörü: 3-Eksen İvmeölçer, Jiroskop ve Oryantasyon verisi üretir ve TelemetryGateway'e besler.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IPiSimInspectableModule.h"
#include "IMUSensor.generated.h"

class UPiSimTelemetryGateway;

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimIMUSensor : public UActorComponent, public IPiSimInspectableModule
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
    UPiSimTelemetryGateway* CachedGateway = nullptr;

    FVector PreviousVelocity = FVector::ZeroVector;
    bool bHasPreviousVelocity = false;
};
