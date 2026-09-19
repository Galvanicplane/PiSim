// Propeller.h
// İtki ve Pervane Mikro Modülü: Motor devri (RPM), tork ve ileri itki (Thrust Newton) fiziğini yönetir.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IPiSimInspectableModule.h"
#include "Propeller.generated.h"

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimPropellerComponent : public UActorComponent, public IPiSimInspectableModule
{
    GENERATED_BODY()

public:
    UPiSimPropellerComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // IPiSimInspectableModule
    virtual void GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties) override;
    virtual void SetInspectablePropertyFloat(FName PropertyId, float NewValue) override;
    virtual void SetInspectablePropertyString(FName PropertyId, const FString& NewValue) override;
    virtual void TriggerInspectableAction(FName ActionId) override;

    UFUNCTION(BlueprintCallable, Category = "PiSim|Propeller")
    void SetThrottle(float InThrottleNormalized); // 0.0 .. 1.0

    UFUNCTION(BlueprintCallable, Category = "PiSim|Propeller")
    void SetFromPWM(int32 InPWM); // 1000 .. 2000 us

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Propeller")
    FName TargetBoneName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Propeller")
    float Throttle = 0.0f; // 0.0 .. 1.0

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Propeller")
    float MaxThrustNewtons = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Propeller")
    float MaxRPM = 8000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Propeller")
    float CurrentRPM = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Propeller")
    int32 ArduPilotChannel = 3; // Varsayılan Gaz kanalı (CH3)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Propeller")
    FVector LocalThrustDirection = FVector(1.0f, 0.0f, 0.0f); // Varsayılan X ileri

protected:
    UPROPERTY()
    class USkeletalMeshComponent* CachedSkeletalMesh = nullptr;

    void ApplyThrustPhysics(float DeltaTime);
};
