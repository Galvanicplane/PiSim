// Servo.h
// Açısal Eylem & Kontrol Yüzeyi Servosu: Kemiklerin açısal limitli hareketini ve ArduPilot PWM eşleşmesini yönetir.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IPiSimInspectableModule.h"
#include "Servo.generated.h"

UCLASS(ClassGroup=(PiSim), meta=(BlueprintSpawnableComponent))
class PISIM_API UPiSimServoComponent : public UActorComponent, public IPiSimInspectableModule
{
    GENERATED_BODY()

public:
    UPiSimServoComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // IPiSimInspectableModule
    virtual void GetInspectableProperties(TArray<FPiSimPropertyDescriptor>& OutProperties) override;
    virtual void SetInspectablePropertyFloat(FName PropertyId, float NewValue) override;
    virtual void SetInspectablePropertyString(FName PropertyId, const FString& NewValue) override;
    virtual void TriggerInspectableAction(FName ActionId) override;

    UFUNCTION(BlueprintCallable, Category = "PiSim|Servo")
    void SetTargetAngle(float InAngleDeg);

    UFUNCTION(BlueprintCallable, Category = "PiSim|Servo")
    void SetFromPWM(int32 InPWM);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    FName TargetBoneName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    float MinAngleDeg = -45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    float MaxAngleDeg = 45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    float TargetAngleDeg = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    float CurrentAngleDeg = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    float SpeedDegPerSec = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    int32 ArduPilotChannel = 1; // 1 - 16

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    FString ArduPilotFunction = TEXT("Aileron"); // Aileron, Elevator, Rudder, Flap, Steer

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    FVector LocalRotationAxis = FVector(0.0f, 0.0f, 1.0f); // Varsayılan Z Yaw ekseni

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PiSim|Servo")
    USceneComponent* LinkedComponent = nullptr;

protected:
    UPROPERTY()
    class USkeletalMeshComponent* CachedSkeletalMesh = nullptr;

    FRotator RestRotation = FRotator::ZeroRotator;
    bool bHasRestRotation = false;

    void ApplyServoPhysics(float DeltaTime);
};
